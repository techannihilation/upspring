//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#ifdef WIN32
#include <windows.h>
#endif

#include "EditorIncl.h"

//#ifdef USE_IK

#include <IL/il.h>
#include <IL/ilut.h>

#include <fltk/run.h>
#include <fltk/file_chooser.h>
#include <fltk/ColorChooser.h>

#include "EditorDef.h"

#include "EditorUI.h"
#include "Util.h"
#include "ModelDrawer.h"
#include "CurvedSurface.h"
#include "ObjectView.h"
#include "Texture.h"
#include "CfgParser.h"
#include "config.h"

#include "AnimationUI.h"
#include "FileSearch.h"
#include "MeshIterators.h"
#include "swig/ScriptInterface.h"
#include "string_util.h"


extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include "spdlog/spdlog.h"
#include "spdlog/sinks/callback_sink.h"

#include "CLI/App.hpp"
#include "CLI/Argv.hpp"

#include <iostream>
#include <fstream>
#include <memory>
#include <filesystem>

const char* ViewSettingsFile = "data/views.cfg";
const char* ArchiveListFile = "data/archives.cfg";
const char* TextureGroupConfig = "data/texgroups.cfg";

/*
 * 	"All Supported (*.{bmp,gif,jpg,png})"
        "All Files (*)\0"
 */

const char* FileChooserPattern =
    "Spring model (S3O)\0s3o\0"
    "Total Annihilation model (3DO)\0 3do\0"
    "3D Studio (3DS)\0 3ds\0"
    "Wavefront OBJ\0obj\0";

// ------------------------------------------------------------------------------------------------
// ArchiveList
// ------------------------------------------------------------------------------------------------

bool ArchiveList::Load() {
  std::string const fn = ups::config::get().app_path() / ArchiveListFile;
  CfgList* cfg = CfgValue::LoadFile(fn.c_str());

  if (cfg == nullptr) {
    return false;
  }

  auto* archs = dynamic_cast<CfgList*>(cfg->GetValue("Archives"));
  if (archs != nullptr) {
    for (auto& child : archs->childs) {
      auto* lit = dynamic_cast<CfgLiteral*>(child.value);
      if (lit != nullptr) {
        archives.insert(lit->value);
      }
    }
  }
  delete cfg;
  return true;
}

bool ArchiveList::Save() {
  std::string const fn = ups::config::get().app_path() / ArchiveListFile;
  CfgWriter writer(fn.c_str());
  if (writer.IsFailed()) {
    return false;
  }

  CfgList cfg;
  auto* archs = new CfgList;
  cfg.AddValue("Archives", archs);
  int index = 0;
  for (auto s = archives.begin(); s != archives.end(); ++s, index++) {
    char tmp[20];
    sprintf(tmp, "arch%d", index);
    archs->AddLiteral(tmp, s->c_str());
  }
  cfg.Write(writer, true);
  return true;
}
// ------------------------------------------------------------------------------------------------
// Editor Callback
// ------------------------------------------------------------------------------------------------

std::vector<EditorViewWindow*> EditorUI::EditorCB::GetViews() {
  std::vector<EditorViewWindow*> vl;
  vl.reserve(ui->viewsGroup->children());
  for (int a = 0; a < ui->viewsGroup->children(); ++a) {
    vl.push_back(dynamic_cast<EditorViewWindow*>(ui->viewsGroup->child(a)));
  }
  return vl;
}

void EditorUI::EditorCB::MergeView(EditorViewWindow* own, EditorViewWindow* other) {
  /*int w = other->w ();*/
  if (own->x() > other->x()) {
    own->set_x(other->x());
  }
  if (own->y() > other->y()) {
    own->set_y(other->y());
  }
  if (own->r() < other->r()) {
    own->set_r(other->r());
  }
  if (own->b() < other->b()) {
    own->set_b(other->b());
  }
  delete other;
  ui->Update();
}

void EditorUI::EditorCB::AddView(EditorViewWindow* v) {
  v->editor = this;
  ui->viewsGroup->add(v);
  ui->Update();
}

Tool* EditorUI::EditorCB::GetTool() {
  if (ui->currentTool != nullptr) {
    ui->currentTool->editor = this;
  }
  return ui->currentTool;
}

// ------------------------------------------------------------------------------------------------
// EditorUI
// ------------------------------------------------------------------------------------------------

static void EditorUIProgressCallback(float part, void* data) {
  auto* ui = static_cast<EditorUI*>(data);
  ui->progress->position(part);
  ui->progress->redraw();
}

void EditorUI::Initialize() {
  optimizeOnLoad = true;

  archives.Load();

  ilInit();
  ilutInit();
  ilutRenderer(ILUT_OPENGL);

  callback.ui = this;

  objectViewer = new ObjectView(this, objectTree);
  uiIK = new IK_UI(&callback);
  uiTimeline = new TimelineUI(&callback);
  uiAnimTrackEditor = new AnimTrackEditorUI(&callback, uiTimeline);
  uiRotator = new RotatorUI;
  uiRotator->CreateUI(&callback);

  tools.SetEditor(&callback);
  tools.camera->button = selectCameraTool;
  tools.move->button = selectMoveTool;
  tools.rotate->button = selectRotateTool;
  tools.scale->button = selectScaleTool;
  tools.texmap->button = selectTextureTool;
  tools.color->button = selectColorTool;
  tools.flip->button = selectFlipTool;
  tools.originMove->button = selectOriginMoveTool;
  tools.rotateTex->button = selectRotateTexTool;
  tools.toggleCurvedPoly->button = selectCurvedPolyTool;
  tools.LoadImages();

  currentTool = tools.GetDefaultTool();
  modelDrawer = new ModelDrawer;
  model = nullptr;
  SetModel(new Model);
  UpdateTitle();

  textureHandler = new TextureHandler();

  for (auto arch = archives.archives.begin(); arch != archives.archives.end(); ++arch) {
    textureHandler->Load3DO(arch->c_str());
  }

  textureGroupHandler = new TextureGroupHandler(textureHandler);
  textureGroupHandler->Load((ups::config::get().app_path() / TextureGroupConfig).c_str());

  UpdateTextureGroups();
  InitTexBrowser();

  uiMapping = new MappingUI(&callback);
  uiTexBuilder = new TexBuilderUI(nullptr, nullptr);

  LoadSettings();

  // create 4 views if no views were specified in the config (ie: there was no config)
  if (viewsGroup->children() == 0) {
    viewsGroup->begin();

    EditorViewWindow* views[4];
    int const vw = viewsGroup->w();
    int const vh = viewsGroup->h();

    for (int a = 0; a < 4; a++) {
      int const Xofs = (a & 1) * vw / 2;
      int const Yofs = (a & 2) * vh / 4;
      int W = vw / 2;
      int H = vh / 2;
      if (Xofs != 0) {
        W = vw - Xofs;
      }
      if (Yofs != 0) {
        H = vh - Yofs;
      }
      views[a] = new EditorViewWindow(Xofs, Yofs, W, H, &callback);
      views[a]->SetMode(a);
      views[a]->bDrawRadius = false;
    }
    views[3]->rendermode = M3D_TEX;

    viewsGroup->end();
  }
}

void EditorUI::Show(bool initial) {
  if (initial) {
    viewsGroup->suppressRescale = true;
  }
  window->show();
  viewsGroup->suppressRescale = false;
  Update();
}

EditorUI::~EditorUI() {
  SAFE_DELETE(uiMapping);
  SAFE_DELETE(uiIK);
  SAFE_DELETE(uiTimeline);
  SAFE_DELETE(uiTexBuilder);
  SAFE_DELETE(uiAnimTrackEditor);
  SAFE_DELETE(uiRotator);

  if (textureGroupHandler != nullptr) {
    textureGroupHandler->Save((ups::config::get().app_path() / TextureGroupConfig).c_str());
    delete textureGroupHandler;
    textureGroupHandler = nullptr;
  }

  SAFE_DELETE(textureHandler);

  SAFE_DELETE(objectViewer);
  SAFE_DELETE(modelDrawer);
  SAFE_DELETE(model);

  ilShutDown();
}

fltk::Color EditorUI::SetTeamColor() {
  fltk::Color r = 0;
  if (fltk::color_chooser("Set team color:", r)) {
    teamColor = r;
    return r;
  }
  return teamColor;
}

void EditorUI::uiSetRenderMethod(RenderMethod rm) {
  modelDrawer->SetRenderMethod(rm);
  viewsGroup->redraw();
}

static void CollectTextures(MdlObject* par_object, std::set<std::shared_ptr<Texture>>& par_textures) {
  PolyMesh* pm = par_object->GetPolyMesh();
  if (pm != nullptr) {
    for (auto& a : pm->poly) {
      if (a->texture) {
        par_textures.emplace(a->texture);
      }
    }
  }
  for (auto& child : par_object->childs) {
    CollectTextures(child, par_textures);
  }
}

void EditorUI::uiAddUnitTextures() {
  // go through all the textures used by the unit
  TextureGroup* cur = GetCurrentTexGroup();
  if ((cur != nullptr) && (model->root != nullptr)) {
    CollectTextures(model->root, cur->textures);
    InitTexBrowser();
  }
}

void EditorUI::uiCut() {
  copyBuffer.Cut(model);

  Update();
}

void EditorUI::uiCopy() {
  copyBuffer.Copy(model);
  Update();
}

void EditorUI::uiPaste() {
  std::vector<MdlObject*> sel = model->GetSelectedObjects();
  MdlObject* where = nullptr;
  if (!sel.empty()) {
    if (sel.size() == 1) {
      where = sel.front();
    } else {
      fltk::message("You can't select multiple objects to paste in.");
      return;
    }
  }

  copyBuffer.Paste(model, where);

  Update();
}

void EditorUI::uiAddObject() {
  const char* r = fltk::input("Add empty object", nullptr);
  if (r != nullptr) {
    auto* obj = new MdlObject;
    obj->name = r;

    if (model->root != nullptr) {
      MdlObject* parent = GetSingleSel();
      if (parent == nullptr) {
        delete obj;
      } else {
        obj->parent = parent;
        parent->childs.push_back(obj);
        parent->isOpen = true;
      }
    } else {
      model->root = obj;
    }

    Update();
  }
}

void EditorUI::uiDeleteSelection() {
  if (currentTool->needsPolySelect()) {
    std::vector<MdlObject*> const objects = model->GetObjectList();
    for (const auto& object : objects) {
      PolyMesh* pm = object->GetPolyMesh();

      if (pm != nullptr) {
        std::vector<Poly*> polygons;
        for (auto* pl : pm->poly) {
          if (!pl->isSelected) {
            polygons.push_back(pl);
          } else {
            delete pl;
          }
        }
        pm->poly = polygons;
      }
    }
  } else {
    std::vector<MdlObject*> const objects = model->GetSelectedObjects();
    std::vector<MdlObject*> filtered;
    for (const auto& object : objects) {
      if (object->HasSelectedParent()) {
        continue;
      }
      filtered.push_back(object);
    }
    for (auto& a : filtered) {
      model->DeleteObject(a);
    }
  }
  Update();
}

void EditorUI::uiApplyTransform() {
  // apply the transformation of each object onto itself, and remove the orientation+offset
  std::vector<MdlObject*> const sel = model->GetSelectedObjects();
  for (auto* o : sel) {
    o->ApplyTransform(true, true, true);
  }

  Update();
}

void EditorUI::uiUniformScale() {
  std::vector<MdlObject*> const sel = model->GetSelectedObjects();
  if (sel.empty()) {
    return;
  }

  const char* scalestr = fltk::input("Scale selected objects with this factor", "1");
  if (scalestr != nullptr) {
    float const scale = atof(scalestr);

    for (const auto& a : sel) {
      if (a->HasSelectedParent()) {
        continue;
      }

      a->scale *= scale;
    }

    Update();
  }
}

void EditorUI::uiRotate3DOTex() {
  if (!currentTool->needsPolySelect()) {
    fltk::message("Use the polygon selection tool first to select the polygons");
  } else {
    std::vector<MdlObject*> const obj = model->GetObjectList();
    for (const auto& o : obj) {
      PolyMesh* pm = o->GetPolyMesh();

      if (pm != nullptr) {
        for (auto* pl : pm->poly) {
          if (pl->isSelected) {
            pl->RotateVerts();
          }
        }
      }
    }

    Update();
  }
}

void EditorUI::uiSwapObjects() {
  std::vector<MdlObject*> sel = model->GetSelectedObjects();
  if (sel.size() != 2) {
    fltk::message("2 objects have to be selected for object swapping");
    return;
  }

  model->SwapObjects(sel.front(), sel.back());
  sel.front()->isOpen = true;
  sel.back()->isOpen = true;

  Update();
}

void EditorUI::uiObjectPositionChanged(int axis, fltk::Input* o) {
  MdlObject* sel = GetSingleSel();

  if (sel != nullptr) {
    float const val = atof(o->value());

    if (chkOnlyMoveOrigin->value()) {
      Vector3 newPos = sel->position;
      newPos[axis] = val;

      sel->MoveOrigin(newPos - sel->position);
    } else {
      sel->position[axis] = val;
    }

    Update();
  }
}

void EditorUI::uiObjectStateChanged(Vector3 MdlObject::*state, float Vector3::*axis, fltk::Input* o,
                                    float scale) {
  MdlObject* sel = GetSingleSel();

  if (sel != nullptr) {
    float const val = atof(o->value());
    (sel->*state).*axis = val * scale;

    Update();
  }
}

void EditorUI::uiObjectRotationChanged(int axis, fltk::Input* o) {
  MdlObject* sel = GetSingleSel();

  if (sel != nullptr) {
    Vector3 euler = sel->rotation.GetEuler();
    euler.v[axis] = atof(o->value()) * DegreesToRadians;
    sel->rotation.SetEuler(euler);

    Update();
  }
}

void EditorUI::uiModelStateChanged(float* prop, fltk::Input* o) {
  *prop = atof(o->value());

  Update();
}

void EditorUI::uiCalculateMidPos() {
  model->EstimateMidPosition();

  Update();
}

void EditorUI::uiCalculateRadius() {
  model->CalculateRadius();

  Update();
}

void EditorUI::menuObjectApproxOffset() {
  std::vector<MdlObject*> sel = model->GetSelectedObjects();
  for (auto& a : sel) {
    a->ApproximateOffset();
  }

  Update();
}

void EditorUI::browserSetObjectName(MdlObject* obj) {
  if (obj == nullptr) {
    return;
  }

  const char* r = fltk::input("Set object name", obj->name.c_str());
  if (r != nullptr) {
    std::string const prev = obj->name;
    obj->name = r;
    objectTree->redraw();
  }
}

void EditorUI::SelectionUpdated() {
  std::vector<MdlObject*> sel = model->GetSelectedObjects();
  char label[128];
  if (sel.size() == 1) {
    MdlObject* f = sel.front();
    PolyMesh* pm = f->GetPolyMesh();
    sprintf(label,
            "MdlObject %s selected: position=(%4.1f,%4.1f,%4.1f) scale=(%3.2f,%3.2f,%3.2f) "
            "polycount=%lu vertexcount=%lu",
            f->name.c_str(), f->position.x, f->position.y, f->position.z, f->scale.x, f->scale.y,
            f->scale.z, pm != nullptr ? pm->poly.size() : 0, pm != nullptr ? pm->verts.size() : 0);
  } else if (sel.size() > 1) {
    int plcount = 0;
    int vcount = 0;
    for (auto& a : sel) {
      PolyMesh* pm = a->GetPolyMesh();
      if (pm != nullptr) {
        plcount += pm->poly.size();
        vcount += pm->verts.size();
      }
    }
    sprintf(label, "Multiple objects selected. Total polycount: %d, Total vertexcount: %d", plcount,
            vcount);
  } else {
    label[0] = 0;
  }
  status->value(label);
  status->redraw();

  Update();
}

void EditorUI::Update() {
  objectViewer->Update();
  viewsGroup->redraw();
  uiIK->Update();
  uiTimeline->Update();
  uiAnimTrackEditor->Update();
  uiRotator->Update();

  // update object inputs
  std::vector<MdlObject*> sel = model->GetSelectedObjects();
  fltk::Input* inputs[] = {inputPosX,   inputPosY, inputPosZ, inputScaleX, inputScaleY,
                           inputScaleZ, inputRotX, inputRotY, inputRotZ};
  if (sel.size() == 1) {
    MdlObject* s = sel.front();
    for (auto& input : inputs) {
      input->activate();
    }
    inputPosX->value(s->position.x);
    inputPosY->value(s->position.y);
    inputPosZ->value(s->position.z);
    inputScaleX->value(s->scale.x);
    inputScaleY->value(s->scale.y);
    inputScaleZ->value(s->scale.z);
    Vector3 const euler = s->rotation.GetEuler();
    inputRotX->value(euler.x / DegreesToRadians);
    inputRotY->value(euler.y / DegreesToRadians);
    inputRotZ->value(euler.z / DegreesToRadians);
  } else {
    for (auto& input : inputs) {
      input->deactivate();
    }
  }
  // model inputs
  inputRadius->value(model->radius);
  inputHeight->value(model->height);
  inputCenterX->value(model->mid.x);
  inputCenterY->value(model->mid.y);
  inputCenterZ->value(model->mid.z);
}

void EditorUI::RenderScene(IView* view) {
  if (model != nullptr) {
    const float m = 1 / 255.0F;
    Vector3 const teamc((teamColor & 0xff000000) >> 24, (teamColor & 0xff0000) >> 16,
                        (teamColor & 0xff00) >> 8);
    modelDrawer->Render(model, view, teamc * m);
  }
}

// get a single selected object and abort when multiple are selected
MdlObject* EditorUI::GetSingleSel() {
  std::vector<MdlObject*> sel = model->GetSelectedObjects();

  if (sel.size() != 1) {
    fltk::message("Select a single object");
    return nullptr;
  }
  return sel.front();
}

void EditorUI::SetMapping(int mapping) {
  model->mapping = mapping;
  viewsGroup->redraw();

  if (mapping == MAPPING_S3O) {
    texGroupS3O->activate();
    texGroup3DO->deactivate();
  } else {
    texGroupS3O->deactivate();
    texGroup3DO->activate();
  }

  mappingChooser->value(mapping);
  mappingChooser->redraw();
}

void EditorUI::SetModel(Model* mdl) {
  SAFE_DELETE(model);
  model = mdl;

  objectViewer->Update();
  viewsGroup->redraw();
  mappingChooser->value(mdl->mapping);
  SetMapping(mdl->mapping);

  inputTexture1->value(nullptr);
  inputTexture2->value(nullptr);

  for (unsigned int a = 0; a < mdl->texBindings.size(); a++) {
    SetModelTexture(a, mdl->texBindings[a].texture);
  }

  Update();
}

void EditorUI::SetTool(Tool* t) {
  if (t != nullptr) {
    t->editor = &callback;
    if (t->isToggle) {
      currentTool = t;
      tools.Disable();
      t->toggle(true);
    } else {
      t->click();
    }
    viewsGroup->redraw();
  }
}

void EditorUI::SetModelTexture(int index, std::shared_ptr<Texture> par_texture) {
  model->SetTexture(index, par_texture);

  if ((par_texture != nullptr) && (par_texture->glIdent == 0U)) {
    (dynamic_cast<EditorViewWindow*>(viewsGroup->child(0)))->make_current();
    par_texture->VideoInit();
  }

  if (index < 2) {
    fltk::FileInput* input = index != 0 ? inputTexture2 : inputTexture1;
    input->value(par_texture != nullptr ? par_texture->name.c_str() : "");
  }
}

void EditorUI::BrowseForTexture(int textureIndex) {
  static std::string fn;
  if (FileOpenDlg("Select texture:", ImageFileExt, fn)) {
    auto tex = std::make_shared<Texture>(fn);
    if (tex->IsLoaded()) {
      tex->image.FlipNonDDS(fn);
      SetModelTexture(textureIndex, tex);
      Update();
    }
  }
}

void EditorUI::ReloadTexture(int index) {
  // fltk::FileInput *input = index ? inputTexture2 : inputTexture1;

  if (model->texBindings.size() <= static_cast<uint>(index)) {
    model->texBindings.resize(index + 1);
  }

  TextureBinding const& tb = model->texBindings[index];
  auto const tex = std::make_shared<Texture>(tb.name);
  if (tex->IsLoaded()) {
    SetModelTexture(index, tex);
  }

  Update();
}

void EditorUI::ConvertToS3O() {
  static int texW = 256;
  static int texH = 256;

  texW = atoi(fltk::input("Enter texture width: ", SPrintf("%d", texW).c_str()));
  texH = atoi(fltk::input("Enter texture height: ", SPrintf("%d", texH).c_str()));
  std::string const name_ext = fltk::filename_name(filename.c_str());
  std::string const name(name_ext.c_str(), fltk::filename_ext(name_ext.c_str()));
  if (model->ConvertToS3O(GetFilePath(filename) + "/" + name + "_tex", texW, texH)) {
    // Update the UI
    SetModelTexture(0, model->texBindings[0].texture);
    SetMapping(MAPPING_S3O);
  }
  Update();
}

// Update window title
void EditorUI::UpdateTitle() {
  const char* baseTitle = "Upspring Model Editor - ";

  if (filename.empty()) {
    windowTitle = baseTitle + std::string("unnamed model");
  } else {
    windowTitle = baseTitle + filename;
  }

  window->label(windowTitle.c_str());
}

void EditorUI::UpdateTextureGroups() {
  textureGroupMenu->clear();

  for (auto* tg : textureGroupHandler->groups) {
    textureGroupMenu->add(tg->name.c_str(), 0, nullptr, tg);
  }
  textureGroupMenu->redraw();
}

void EditorUI::SelectTextureGroup(fltk::Widget* /*w*/, void* /*d*/) { InitTexBrowser(); }

TextureGroup* EditorUI::GetCurrentTexGroup() {
  assert(textureGroupHandler->groups.size() == (uint)textureGroupMenu->children());
  if (textureGroupHandler->groups.empty()) {
    return nullptr;
  }

  fltk::Widget* s = textureGroupMenu->child(textureGroupMenu->value());
  if (s != nullptr) {
    auto* tg = static_cast<TextureGroup*>(s->user_data());
    return tg;
  }
  return nullptr;
}

void EditorUI::InitTexBrowser() {
  texBrowser->clear();

  TextureGroup* tg = GetCurrentTexGroup();
  if (tg == nullptr) {
    return;
  }
  for (auto texture : tg->textures) {
    texBrowser->AddTexture(texture);
  }
  texBrowser->UpdatePositions();
  texBrowser->redraw();
}

bool EditorUI::Load(const std::string& fn) {
  Model* mdl = Model::Load(fn, optimizeOnLoad);

  if (mdl != nullptr) {
    filename = fn;
    SetModel(mdl);

    Update();
  } else {
    delete mdl;
  }
  return mdl != nullptr;
}

void EditorUI::menuObjectLoad() {
  MdlObject* obj = GetSingleSel();
  if (obj == nullptr) {
    return;
  }

  static std::string fn;
  if (FileOpenDlg("Load object:", FileChooserPattern, fn)) {
    Model* submdl = Model::Load(fn);
    if (submdl != nullptr) {
      model->InsertModel(obj, submdl);
    }

    delete submdl;

    Update();
  }
}

void EditorUI::menuObjectSave() {
  MdlObject* sel = GetSingleSel();
  if (sel == nullptr) {
    return;
  }
  static std::string fn;
  char fnl[256];
  strncpy(fnl, fn.c_str(), sizeof(fnl));
  strncpy(fltk::filename_name(fnl), sel->name.c_str(), sizeof(fnl));
  fn = fnl;
  if (FileSaveDlg("Save object:", FileChooserPattern, fn)) {
    auto* submdl = new Model;
    submdl->root = sel;
    Model::Save(submdl, fn);
    submdl->root = nullptr;
    delete submdl;
  }
}

void EditorUI::menuObjectReplace() {
  MdlObject* old = GetSingleSel();
  if (old == nullptr) {
    return;
  }
  char buf[128];
  sprintf(buf, "Select model to replace \'%s\' ", old->name.c_str());
  static std::string fn;
  if (FileOpenDlg(buf, FileChooserPattern, fn)) {
    Model* submdl = Model::Load(fn);
    if (submdl != nullptr) {
      model->ReplaceObject(old, submdl->root);
      submdl->root = nullptr;

      Update();
    }
    delete submdl;
  }
}

void EditorUI::menuObjectMerge() {
  std::vector<MdlObject*> const sel = model->GetSelectedObjects();

  for (const auto& a : sel) {
    MdlObject* parent = a->parent;
    if (parent != nullptr) {
      parent->MergeChild(a);
    }
  }

  Update();
}

void EditorUI::menuObjectFlipPolygons() {
  std::vector<MdlObject*> const sel = model->GetSelectedObjects();
  for (const auto& a : sel) {
    for (PolyIterator pi(a); !pi.End(); pi.Next()) {
      pi->Flip();
    }

    a->InvalidateRenderData();
  }

  Update();
}

void EditorUI::menuObjectRecalcNormals() {
  std::vector<MdlObject*> const sel = model->GetSelectedObjects();
  for (const auto& a : sel) {
    PolyMesh* pm = a->GetPolyMesh();
    if (pm != nullptr) {
      pm->CalculateNormals();
    }

    a->InvalidateRenderData();
  }

  Update();
}

void EditorUI::menuObjectResetScaleRot() {
  std::vector<MdlObject*> const sel = model->GetSelectedObjects();
  for (const auto& a : sel) {
    a->rotation = Rotator();
    a->scale.set(1, 1, 1);
  }
  Update();
}

void EditorUI::menuObjectResetTransform() {
  std::vector<MdlObject*> const sel = model->GetSelectedObjects();
  for (const auto& a : sel) {
    a->position = Vector3();
    a->rotation = Rotator();
    a->scale.set(1, 1, 1);
  }

  Update();
}

void EditorUI::menuObjectResetPos() {
  std::vector<MdlObject*> const sel = model->GetSelectedObjects();
  for (const auto& a : sel) {
    a->position = Vector3();
  }

  Update();
}

void EditorUI::menuObjectShowAnimWindows() {
  uiIK->Show();
  uiTimeline->Show();
}

void EditorUI::menuObjectGenCSurf() {
  std::vector<MdlObject*> const sel = model->GetSelectedObjects();
  for (const auto& a : sel) {
    delete a->csurfobj;

    a->csurfobj = new csurf::Object;
    a->csurfobj->GenerateFromPolyMesh(a->GetPolyMesh());
  }
}

void EditorUI::menuEditOptimizeAll() {
  std::vector<MdlObject*> const objects = model->GetObjectList();
  for (const auto& object : objects) {
    if (object->GetPolyMesh() != nullptr) {
      object->GetPolyMesh()->Optimize(PolyMesh::IsEqualVertexTCNormal);
    }
  }
}

void EditorUI::menuEditOptimizeSelected() {
  std::vector<MdlObject*> const objects = model->GetObjectList();
  for (const auto& object : objects) {
    if (object->isSelected && (object->GetPolyMesh() != nullptr)) {
      object->GetPolyMesh()->Optimize(PolyMesh::IsEqualVertexTCNormal);
    }
  }
}

void EditorUI::menuFileSaveAs() {
  static std::string fn;
  if (FileSaveDlg("Save model file:", FileChooserPattern, fn)) {
    filename = fn;
    Model::Save(model, fn);
    UpdateTitle();
  }
}

void EditorUI::menuFileNew() { SetModel(new Model); }

// this will also be called by the main window callback (on exit)
void EditorUI::menuFileExit() {
  uiIK->Hide();
  uiMapping->Hide();
  uiTimeline->Hide();
  uiAnimTrackEditor->Hide();
  uiRotator->Hide();
  window->hide();
}

void EditorUI::menuFileLoad() {
  static std::string fn;
  const bool r = FileOpenDlg("Load model file:", FileChooserPattern, fn);

  if (r) {
    Load(fn);
    UpdateTitle();
  }
}

void EditorUI::menuFileSave() {
  if (filename.empty()) {
    menuFileSaveAs();
    return;
  }

  Model::Save(model, filename);
}

void EditorUI::menuSettingsShowArchiveList() {
  ArchiveListUI sui(archives);
  if (sui.window->exec()) {
    archives = sui.settings;
    archives.Save();

    for (auto arch = archives.archives.begin(); arch != archives.archives.end(); ++arch) {
      textureHandler->Load3DO(arch->c_str());
    }
  }
}

// Show texture group window
void EditorUI::menuSettingsTextureGroups() {
  TexGroupUI const texGroupUI(textureGroupHandler, textureHandler);
  textureGroupHandler->Save((ups::config::get().app_path() /  TextureGroupConfig).c_str());

  UpdateTextureGroups();
  InitTexBrowser();
}

void EditorUI::menuSettingsSetBgColor() {
  Vector3 col;
  if (fltk::color_chooser("Select view background color:", col.x, col.y, col.z)) {
    for (int a = 0; a < viewsGroup->children(); ++a) {
      (dynamic_cast<EditorViewWindow*>(viewsGroup->child(a)))->backgroundColor = col;
    }
    viewsGroup->redraw();
  }
}

void EditorUI::menuSetSpringDir() {
  SelectDirectory(
      "Select S3O texture loading directory\n"
      "(for example c:\\games\\spring\\unittextures)",
      Texture::textureLoadDir);
}

void EditorUI::menuSettingsRestoreViews() { LoadSettings(); }

void EditorUI::SaveSettings() {
  std::string const path = ups::config::get().app_path() /  ViewSettingsFile;
  CfgWriter writer(path.c_str());
  if (writer.IsFailed()) {
    fltk::message("Failed to open %s for writing view settings", path.c_str());
    return;
  }
  CfgList cfg;
  SerializeConfig(cfg, true);
  cfg.Write(writer, true);
}

void EditorUI::LoadSettings() {
  std::string const path = ups::config::get().app_path() / ViewSettingsFile;
  CfgList* cfg = CfgValue::LoadFile(path.c_str());
  if (cfg != nullptr) {
    SerializeConfig(*cfg, false);
    delete cfg;
  }
}

void EditorUI::LoadToolWindowSettings() {
  std::string const path = ups::config::get().app_path() / ViewSettingsFile;
  CfgList* cfg = CfgValue::LoadFile(path.c_str());
  if (cfg != nullptr) {
    // tool window visibility
    bool showTimeline = false;
    bool showTrackEdit = false;
    bool showIK = false;
    CFG_LOAD(*cfg, showTimeline);
    CFG_LOAD(*cfg, showTrackEdit);
    CFG_LOAD(*cfg, showIK);
    if (showTrackEdit) {
      uiAnimTrackEditor->Show();
    }
    if (showIK) {
      uiIK->Show();
    }
    if (showTimeline) {
      uiTimeline->Show();
    }

    delete cfg;
  }
}

void EditorUI::SerializeConfig(CfgList& cfg, bool store) {
  // Serialize editor window properties
  int x = window->x();
  int y = window->y();
  int width = window->w();
  int height = window->h();
  std::string& springTexDir = Texture::textureLoadDir;
  if (store) {
    CFG_STOREN(cfg, x);
    CFG_STOREN(cfg, y);
    CFG_STOREN(cfg, width);
    CFG_STOREN(cfg, height);
    CFG_STORE(cfg, springTexDir);
    CFG_STORE(cfg, optimizeOnLoad);
    bool const showTimeline = uiTimeline->window->visible();
    CFG_STORE(cfg, showTimeline);
    bool const showTrackEdit = uiAnimTrackEditor->window->visible();
    CFG_STORE(cfg, showTrackEdit);
    bool const showIK = uiIK->window->visible();
    CFG_STORE(cfg, showIK);
  } else {
    CFG_LOADN(cfg, x);
    CFG_LOADN(cfg, y);
    CFG_LOADN(cfg, width);
    CFG_LOADN(cfg, height);
    CFG_LOAD(cfg, springTexDir);
    CFG_LOAD(cfg, optimizeOnLoad);

    window->resize(x, y, width, height);
  }
  // Serialize views: first match the number of views specified in the config
  int numViews = viewsGroup->children();
  if (store) {
    CFG_STOREN(cfg, numViews);
  } else {
    CFG_LOADN(cfg, numViews);
    int const nv = numViews - viewsGroup->children();
    if (nv > 0) {
      viewsGroup->begin();
      for (int a = 0; a < nv; a++) {
        auto* wnd = new EditorViewWindow(0, 0, 0, 0, &callback);
        wnd->SetMode(a % 4);
      }
      viewsGroup->end();
    } else {
      for (int a = viewsGroup->children() - 1; a >= numViews; a--) {
        auto* wnd = dynamic_cast<EditorViewWindow*>(viewsGroup->child(a));
        viewsGroup->remove(wnd);
        delete wnd;
      }
    }
  }
  // then serialize view settings for each view in the config
  int viewIndex = 0;
  char viewName[16];
  for (int a = 0; a < viewsGroup->children(); ++a) {
    sprintf(viewName, "View%d", viewIndex++);
    CfgList* viewcfg = nullptr;
    if (store) {
      viewcfg = new CfgList;
      cfg.AddValue(viewName, viewcfg);
    } else {
      viewcfg = dynamic_cast<CfgList*>(cfg.GetValue(viewName));
    }
    if (viewcfg != nullptr) {
      (dynamic_cast<EditorViewWindow*>(viewsGroup->child(a)))->Serialize(*viewcfg, store);
    }
  }
}

void EditorUI::menuMappingImportUV() {
  static std::string fn;
  if (FileOpenDlg("Select model to copy UV coordinates from:", FileChooserPattern, fn)) {
    IProgressCtl progctl;

    progctl.cb = EditorUIProgressCallback;
    progctl.data = this;

    progress->range(0.0F, 1.0F, 0.01F);
    progress->position(0.0F);
    progress->show();
    model->ImportUVMesh(fn.c_str(), progctl);
    progress->hide();
  }
}

void EditorUI::menuMappingExportUV() {
  static std::string fn;
  if (FileSaveDlg("Save merged model for UV mapping:", FileChooserPattern, fn)) {
    model->ExportUVMesh(fn.c_str());
  }
}

void EditorUI::menuMappingShow() { uiMapping->Show(); }

void EditorUI::menuMappingShowTexBuilder() { uiTexBuilder->Show(); }

void EditorUI::menuScriptLoad() {
  const char* pattern = "Lua script (LUA)\0*.lua\0";

  static std::string lastLuaScript;
  if (FileOpenDlg("Load script:", pattern, lastLuaScript)) {
    if (luaL_dofile(luaState, lastLuaScript.c_str()) != 0) {
      const char* err = lua_tostring(luaState, -1);
      fltk::message("Error while executing %s: %s", lastLuaScript.c_str(), err);
    }
  }
}

static EditorUI* editorUI = nullptr;

static void scriptClickCB(fltk::Widget* /*w*/, void* d) {
  auto* s = static_cast<ScriptedMenuItem*>(d);

  char buf[64];
  SNPRINTF(buf, sizeof(buf), "%s()", s->funcName.c_str());
  if (luaL_dostring(editorUI->luaState, buf) != 0) {
    const char* err = lua_tostring(editorUI->luaState, -1);
    fltk::message("Error while executing %s: %s", s->name.c_str(), err);
  }

  editorUI->Update();
}

void upsAddMenuItem(const char* name, const char* funcName) {
  auto* s = new ScriptedMenuItem;
  s->funcName = funcName;
  editorUI->scripts.push_back(s);

  fltk::Widget* w = editorUI->menu->add(name, 0, scriptClickCB, s);
  s->name = w->label();
}

IEditor* upsGetEditor() { return &editorUI->callback; }

void EditorUI::menuHelpAbout() {
  fltk::message(
      "Upspring, created by Jelmer Cnossen\n"
      "Model editor for the Spring RTS project: spring.clan-sy.com\n"
      "Using FLTK, lib3ds, GLEW, zlib, and DevIL/OpenIL");
}

// ------------------------------------------------------------------------------------------------
// Application Entry Point
// ------------------------------------------------------------------------------------------------

extern "C" {
int luaopen_upspring(lua_State* lua_state);
}

void run_script(CLI::App &app, const std::string& par_script_file) {
  // Initialise Lua
  lua_State* lua_state = luaL_newstate();
  luaL_openlibs(lua_state);
  luaopen_upspring(lua_state);

  // Arg forwarding
  auto remaining = app.remaining();
  if (!remaining.empty()) {
    if (remaining[0] != "--") {
      spdlog::error("no -- delimiter for arguments found");
      std::exit(1);
    }

    lua_createtable(lua_state, remaining.size(), remaining.size());
    lua_pushstring(lua_state, par_script_file.c_str());
    lua_rawseti(lua_state, -2, 0);
    for (std::size_t i = 1; i < remaining.size(); ++i) {
      lua_pushstring(lua_state, remaining[i].c_str());
      lua_rawseti(lua_state, -2, static_cast<int64_t>(i));
      spdlog::debug("{}: {}", i, remaining[i]);
    }
    lua_setglobal(lua_state, "arg");
  } else {
    lua_createtable(lua_state, 1, 1);
    lua_pushstring(lua_state, par_script_file.c_str());
    lua_rawseti(lua_state, -2, 0);
    lua_setglobal(lua_state, "arg");
  }

  int status = luaL_loadfile(lua_state, par_script_file.c_str());
  if (status == LUA_OK) {
    status = lua_pcall(lua_state, 0, 0, 0);
  }
  if (status != LUA_OK) {
    const char* err = lua_tostring(lua_state, -1);
    spdlog::error("executing '{}': {}", par_script_file, err);
    lua_close(lua_state);
    lua_pop(lua_state, 1);
    return std::exit(1);
  }

  lua_close(lua_state);

  std::exit(0);
}

int main(int argc, char* argv[]) {
#ifdef DEBUG
  spdlog::set_level(spdlog::level::debug);
#else
  spdlog::set_level(spdlog::level::info);
#endif

  CLI::App app{"Upspring - the Spring RTS model editor", "Upspring"};
  app.prefix_command();

  app.set_version_flag("--version", std::string(UPSPRING_VERSION));

  app.add_option_function<std::string>("--run,-r", [&app](const std::string par_script_file) { run_script(app, par_script_file); }, "Run a lua script file and exit");
  app.callback([&app]() {
    CLI::App subApp;
    std::string model_file;
    subApp.add_option("file", model_file, "Model file to load");
    subApp.parse(app.remaining_for_passthrough());

    /**
     * Config
     */
    ups::config::get().app_path(std::filesystem::path(CLI::argv()[0]).remove_filename());

    /**
     * Normal app start
     */

    // Bring up the main editor dialog
    EditorUI editor;
    editorUI = &editor;
    editor.Show(true);
    editor.LoadToolWindowSettings();

    // Initialise Lua
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    luaopen_upspring(L);

    editor.luaState = L;

    if (luaL_dofile(L, "scripts/init.lua") != 0) {
      const char* err = lua_tostring(L, -1);
      fltk::message("Error while executing init.lua: %s", err);
    }

    // Setup logger callback, so error messages are reported with fltk::message
    spdlog::callback_logger_st("custom_callback_logger", [](const spdlog::details::log_msg& msg) {
      if (msg.level != spdlog::level::err && msg.level != spdlog::level::warn) {
        return;
      }

      fltk::message(msg.payload.data());
    });

    // NOTE: the "scripts/plugins" literal was changed to "scripts/plugins/"
    std::list<std::string>* luaFiles = FindFiles("*.lua", false,
  #ifdef WIN32
                                                  "scripts\\plugins");
  #else
                                                  "scripts/plugins/");
  #endif

    for (auto i = luaFiles->begin(); i != luaFiles->end(); ++i) {
      if (luaL_dofile(L, i->c_str()) != 0) {
        const char* err = lua_tostring(L, -1);
        fltk::message("Error while executing \'%s\': %s", i->c_str(), err);
      }
    }

    if (!model_file.empty()) {
      editor.Load(model_file);
    }

    std::exit(fltk::run());
  });


  try {
    (app).parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    return (app).exit(e);
  }
}

// #endif

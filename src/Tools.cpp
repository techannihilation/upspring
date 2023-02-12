//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#include "EditorIncl.h"
#include "EditorDef.h"
#include "View.h"
#include "Tools.h"
#include "Model.h"
#include "Texture.h"
#include "MeshIterators.h"
#include "Util.h"

#include "FileSystem/CZipArchive.h"

#include "string_util.h"
#include "spdlog/spdlog.h"

#include <memory>

#include <fltk/ColorChooser.h>
#include <IL/il.h>
#include <fltk/Image.h>

#ifdef _MSC_VER
#include <float.h>
#define ISNAN(c) _isnan(c)
#else
#define ISNAN(c) isnan(c)
#endif

// ------------------------------------------------------------------------------------------------
// CopyBuffer - implements cut/copy/paste actions
// ------------------------------------------------------------------------------------------------

CopyBuffer::CopyBuffer() {}

CopyBuffer::~CopyBuffer() { Clear(); }

void CopyBuffer::Clear() {
  for (uint a = 0; a < buffer.size(); a++) delete buffer[a];
  buffer.clear();
}

void CopyBuffer::Copy(Model* mdl) {
  Clear();

  std::vector<MdlObject*> sel = mdl->GetSelectedObjects();
  for (uint a = 0; a < sel.size(); a++) {
    if (sel[a]->HasSelectedParent()) continue;  // the parent will be copied anyway

    buffer.push_back(sel[a]->Clone());
  }
}

void CopyBuffer::Cut(Model* mdl) {
  Clear();

  for (;;) {
    std::vector<MdlObject*> sel = mdl->GetSelectedObjects();
    if (sel.empty()) break;

    MdlObject* obj = sel.front();
    if (obj->parent) {
      MdlObject* p = obj->parent;
      p->childs.erase(find(p->childs.begin(), p->childs.end(), obj));
      obj->parent = 0;
    } else {
      assert(mdl->root == obj);
      mdl->root = 0;
    }

    obj->isSelected = false;
    buffer.push_back(obj);
  }
}

void CopyBuffer::Paste(Model* mdl, MdlObject* where) {
  for (uint a = 0; a < buffer.size(); a++) {
    if (where) {
      MdlObject* obj = buffer[a]->Clone();
      where->childs.push_back(obj);
      obj->parent = where;
      obj->isOpen = true;
    } else {
      if (mdl->root) {
        fltk::message("There can only be one root object.");
        return;
      }
      mdl->root = buffer[a]->Clone();
      mdl->root->parent = 0;
    }
  }
}

// ------------------------------------------------------------------------------------------------
// Toolbox controls
// ------------------------------------------------------------------------------------------------

void ModifyObjects(MdlObject* obj, Vector3 d, void (*fn)(MdlObject* obj, Vector3 d)) {
  fn(obj, d);
  for (uint a = 0; a < obj->childs.size(); a++) ModifyObjects(obj->childs[a], d, fn);
}

static const float SpeedMod = 0.05f;
static const float AngleMod = 1.0f;

struct ECameraTool : Tool {
  ECameraTool() {
    isToggle = true;
    imageFile = "camera.gif";
  }

  bool toggle(bool enable) { return true; }

  void mouse(EditorViewWindow* view, int msg, Point move);
} static CameraTool;

// ------------------------- move -----------------------------

struct EMoveTool : Tool {
  EMoveTool() {
    isToggle = true;
    imageFile = "move.gif";
  }

  bool toggle(bool enable) { return true; }

  static void apply(MdlObject* obj, Vector3 d) {
    if (obj->isSelected) obj->position += d;
  }

  static void ApplyMoveOp(Model* mdl, void* ud) {
    Vector3* d = (Vector3*)ud;
    if (mdl->root) ModifyObjects(mdl->root, *d, apply);
  }

  void mouse(EditorViewWindow* view, int msg, Point move) {
    Point m = move;

    if ((fltk::event_state() & fltk::ALT) && !(fltk::event_state() & fltk::CTRL)) {
      CameraTool.mouse(view, msg, move);
      return;
    }

    if ((msg == fltk::MOVE || msg == fltk::DRAG) && (fltk::event_state() & fltk::BUTTON1)) {
      Vector3 d;
      switch (view->GetMode()) {
        case MAP_3D:
          return;
        case MAP_XY:
          d = Vector3(m.x, -m.y, 0.0f);
          break;
        case MAP_XZ:
          d = Vector3(m.x, 0.0f, -m.y);
          break;
        case MAP_YZ:
          d = Vector3(0.0f, -m.y, m.x);
          break;
      }
      d /= view->cam.zoom;
      d *= SpeedMod;

      editor->RedrawViews();
    }
    if (fltk::event_state() & fltk::BUTTON2) view->cam.MouseEvent(view, msg, move);
  }
} static MoveTool;

void ECameraTool::mouse(EditorViewWindow* view, int msg, Point move) {
  int s = fltk::event_state();

  if ((fltk::event_state() & fltk::CTRL) && !(fltk::event_state() & fltk::ALT)) {
    MoveTool.mouse(view, msg, move);
    return;
  }

  view->cam.MouseEvent(view, msg, move);
}

// ------------------------------ rotate ------------------------------

struct ERotateTool : public Tool {
  ERotateTool() {
    imageFile = "rotate.gif";
    isToggle = true;
  }

  bool toggle(bool enabletool) { return true; }

  static void apply(MdlObject* n, Vector3 rot) {
    if (n->isSelected) {
      n->rotation.AddEulerAbsolute(rot);

      //			Vector3 euler = n->rotation.GetEuler();
      //			if (ISNAN(euler.x) || ISNAN(euler.y) || ISNAN(euler.z))
      //				logger.Print("Rot: %f,%f,%f\n",rot.x,rot.y,rot.z);
    }
  }

  static void ApplyRotateOp(Model* mdl, void* d) {
    Vector3* rot = (Vector3*)d;

    if (mdl->root) ModifyObjects(mdl->root, *rot, apply);
  }

  void mouse(EditorViewWindow* view, int msg, Point move) {
    if ((fltk::event_state() & fltk::ALT) && !(fltk::event_state() & fltk::CTRL)) {
      CameraTool.mouse(view, msg, move);
      return;
    }

    if ((msg == fltk::DRAG || msg == fltk::MOVE) && (fltk::event_state() & fltk::BUTTON1)) {
      Vector3 rot;
      float r = AngleMod * move.x / 180.0f * M_PI;
      switch (view->GetMode()) {
        case MAP_3D:
          return;
        case MAP_XY:
          rot.set(0, 0, -r);
          break;
        case MAP_XZ:
          rot.set(0, -r, 0);
          break;
        case MAP_YZ:
          rot.set(-r, 0, 0);
          break;
      }

      editor->RedrawViews();
    } else if (fltk::event_state() & fltk::BUTTON2)
      view->cam.MouseEvent(view, msg, move);
  }
} static RotateTool;

// --------------------------------- scale --------------------------------------

struct EScaleTool : public Tool {
  EScaleTool() {
    imageFile = "scale.gif";
    isToggle = true;
  }

  bool toggle(bool enabletool) { return true; }

  static void limitscale(Vector3* s) {
    if (fabs(s->x) < EPSILON) s->x = (s->x >= 0) ? EPSILON : -EPSILON;
    if (fabs(s->y) < EPSILON) s->y = (s->y >= 0) ? EPSILON : -EPSILON;
    if (fabs(s->z) < EPSILON) s->z = (s->z >= 0) ? EPSILON : -EPSILON;
  }

  static void apply(MdlObject* obj, Vector3 scale) {
    if (obj->isSelected) {
      obj->scale *= scale;
      limitscale(&obj->scale);
    }
  }

  static void ApplyScaleOp(Model* mdl, void* d) {
    Vector3* s = (Vector3*)d;

    if (mdl->root) ModifyObjects(mdl->root, *s, apply);
  }

  void mouse(EditorViewWindow* view, int msg, Point move) {
    if ((fltk::event_state() & fltk::ALT) && !(fltk::event_state() & fltk::CTRL)) {
      CameraTool.mouse(view, msg, move);
      return;
    }

    if ((msg == fltk::DRAG || msg == fltk::MOVE) && (fltk::event_state() & fltk::BUTTON1)) {
      Vector3 s;
      float sx = 1.0f + move.x * 0.01f;
      float sy = 1.0f - move.y * 0.01f;

      switch (view->GetMode()) {
        case MAP_3D:
          return;
        case MAP_XY:
          s = Vector3(sx, sy, 1.0f);
          break;
        case MAP_XZ:
          s = Vector3(sx, 1.0f, sy);
          break;
        case MAP_YZ:
          s = Vector3(1.0f, sy, sx);
          break;
      }

      editor->RedrawViews();
    } else if (fltk::event_state() & fltk::BUTTON2)
      view->cam.MouseEvent(view, msg, move);
  }

} static ScaleTool;

// --------------------------------- origin move tool --------------------------------------
//  move the objects while moving the vertices in the opposite direction

struct EOriginMoveTool : public Tool {
  EOriginMoveTool() {
    imageFile = "originmove.png";
    isToggle = true;
  }

  bool toggle(bool enabletool) { return true; }

  static void apply(MdlObject* o, Vector3 move) {
    if (!o->isSelected) return;

    o->MoveOrigin(move);
  }

  static void ApplyOriginMoveOp(Model* mdl, void* ud) {
    Vector3* d = (Vector3*)ud;

    if (mdl->root) ModifyObjects(mdl->root, *d, apply);
  }

  void mouse(EditorViewWindow* view, int msg, Point move) {
    Point m = move;

    if ((msg == fltk::MOVE || msg == fltk::DRAG) && (fltk::event_state() & fltk::BUTTON1)) {
      Vector3 d;
      switch (view->GetMode()) {
        case MAP_3D:
          return;
        case MAP_XY:
          d = Vector3(m.x, -m.y, 0.0f);
          break;
        case MAP_XZ:
          d = Vector3(m.x, 0.0f, -m.y);
          break;
        case MAP_YZ:
          d = Vector3(0.0f, -m.y, m.x);
          break;
      }
      d /= view->cam.zoom;
      d *= SpeedMod;

      editor->RedrawViews();
    }
  }

} static OriginMoveTool;

// --------------------------------- apply texture tool --------------------------------------

struct ETextureTool : Tool {
  bool enabled;
  ETextureTool() {
    imageFile = "texture.gif";
    isToggle = true;
    enabled = false;
  }

  static void applyTexture(MdlObject* o, std::shared_ptr<Texture> par_tex) {
    PolyMesh* pm = o->GetPolyMesh();
    if (pm) {
      for (int a = 0; a < pm->poly.size(); a++) {
        if (pm->poly[a]->isSelected) {
          pm->poly[a]->texture = par_tex;
          pm->poly[a]->texname = par_tex->name;
        }
      }
    }
    for (uint a = 0; a < o->childs.size(); a++) applyTexture(o->childs[a], par_tex);
  }

  static void callback(std::shared_ptr<Texture> tex, void* data) {
    ETextureTool* tool = (ETextureTool*)data;
    Model* model = tool->editor->GetMdl();
    if (model->root) applyTexture(model->root, tex);
    tool->editor->RedrawViews();
  }

  static void deselect(MdlObject* o) {
    for (PolyIterator pi(o); !pi.End(); pi.Next()) pi->isSelected = false;
  }

  bool toggle(bool enable) {
    if (enable) {
      editor->SetTextureSelectCallback(callback, this);
      MdlObject* r = editor->GetMdl()->root;
      if (r) IterateObjects(r, deselect);
    } else
      editor->SetTextureSelectCallback(0, 0);
    enabled = enable;
    return true;
  }

  bool needsPolySelect() { return enabled; }

  void mouse(EditorViewWindow* view, int msg, Point move) { CameraTool.mouse(view, msg, move); }
} static TextureMapTool;

// --------------------------------- Tools collection --------------------------------------

struct EPolyColorTool : Tool {
  Vector3 color;

  EPolyColorTool() : Tool() {
    imageFile = "color.gif";
    isToggle = false;
  }

  static void applyColor(MdlObject* o, Vector3 color) {
    for (PolyIterator pi(o); !pi.End(); pi.Next())
      if (pi->isSelected) {
        pi->color = color;
        pi->texname.clear();
        pi->texture = 0;
      }
    for (uint a = 0; a < o->childs.size(); a++) applyColor(o->childs[a], color);
  }

  bool toggle(bool enable) { return true; }
  void click() {
    if (!fltk::color_chooser("Color for selected polygons", color.x, color.y, color.z)) return;

    Model* m = editor->GetMdl();
    if (m->root) applyColor(m->root, color);

    editor->RedrawViews();
  }
} static PolygonColorTool;

struct EPolyFlipTool : Tool {
  EPolyFlipTool() : Tool() {
    imageFile = "polyflip.png";
    isToggle = false;
  }

  static void flip(MdlObject* o) {
    for (PolyIterator pi(o); !pi.End(); pi.Next())
      if (pi->isSelected) pi->Flip();
  }

  bool toggle(bool enable) { return true; }
  void click() {
    Model* m = editor->GetMdl();
    if (m->root) IterateObjects(m->root, flip);
    editor->RedrawViews();
  }
} static PolygonFlipTool;

struct ERotateTexTool : Tool {
  ERotateTexTool() {
    isToggle = false;
    imageFile = "rotatetex.png";
  }

  static void rotatetex(MdlObject* o) {
    for (PolyIterator p(o); !p.End(); p.Next())
      if (p->isSelected) p->RotateVerts();
  }

  bool toggle(bool enable) { return true; }
  void click() {
    Model* m = editor->GetMdl();
    if (m->root) IterateObjects(m->root, rotatetex);
    editor->RedrawViews();
  }
} static RotateTexTool;

struct ECurvedPolyTool : Tool {
  ECurvedPolyTool() { isToggle = false, imageFile = "curvedpoly.png"; }

  static void togglecurved(MdlObject* o) {
    for (PolyIterator p(o); !p.End(); p.Next())
      if (p->isSelected) p->isCurved = !p->isCurved;
  }

  bool toggle(bool enable) { return true; }
  void click() {
    Model* m = editor->GetMdl();
    if (m->root) IterateObjects(m->root, togglecurved);
    editor->RedrawViews();
  }
} static ToggleCurvedPolyTool;

// --------------------------------- Tools collection --------------------------------------
Tools::Tools() {
  camera = &CameraTool;
  move = &MoveTool;
  rotate = &RotateTool;
  scale = &ScaleTool;
  texmap = &TextureMapTool;
  color = &PolygonColorTool;
  flip = &PolygonFlipTool;
  originMove = &OriginMoveTool;
  rotateTex = &RotateTexTool;
  toggleCurvedPoly = &ToggleCurvedPolyTool;

  Tool* _tools[] = {camera,     move,      rotate,           scale, texmap, color, flip,
                    originMove, rotateTex, toggleCurvedPoly, 0};
  for (uint a = 0; _tools[a]; a++) tools.push_back(_tools[a]);
}

Tool* Tools::GetDefaultTool() { return camera; }

void Tools::Disable() {
  for (uint a = 0; a < tools.size(); a++)
    if (tools[a]->isToggle) tools[a]->toggle(false);
}

void Tools::SetEditor(IEditor* editor) {
  for (uint a = 0; a < tools.size(); a++) tools[a]->editor = editor;
}

void Tools::LoadImages()
{
  auto archive = CZipArchive("data/buttons.ups");
  for (auto &tool : tools) {
    std::string const name = to_lower(tool->imageFile);
    std::vector<std::uint8_t> buffer{};
    if (!archive.GetFileByName(name, buffer)) {
      spdlog::error("Failed to get tool image '{}' from 'data/buttons.ups'", tool->imageFile);
      continue;
    }

    Image image;
    if (!image.load(buffer)) {
      spdlog::error("Failed to load image: '{}', error was: {}", tool->imageFile, image.error());
      continue;
    }

    if (!image.has_alpha() && !image.AddAlpha()) {
      spdlog::error("Failed to add alpha to tool image: '{}', error was: {}", tool->imageFile, image.error());
      continue;
    }

    fltk::Image* img = new fltk::Image(image.data(), fltk::RGBA, image.width(), image.height());
    tool->button->image(img);
    tool->button->label("");
  }
}

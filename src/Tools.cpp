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

CopyBuffer::CopyBuffer() = default;

CopyBuffer::~CopyBuffer() { Clear(); }

void CopyBuffer::Clear() {
  for (auto& a : buffer) {
    delete a;
  }
  buffer.clear();
}

void CopyBuffer::Copy(Model* mdl) {
  Clear();

  std::vector<MdlObject*> sel = mdl->GetSelectedObjects();
  for (auto& a : sel) {
    if (a->HasSelectedParent()) {
      continue;  // the parent will be copied anyway
    }

    buffer.push_back(a->Clone());
  }
}

void CopyBuffer::Cut(Model* mdl) {
  Clear();

  for (;;) {
    std::vector<MdlObject*> sel = mdl->GetSelectedObjects();
    if (sel.empty()) {
      break;
    }

    MdlObject* obj = sel.front();
    if (obj->parent != nullptr) {
      MdlObject* p = obj->parent;
      p->childs.erase(find(p->childs.begin(), p->childs.end(), obj));
      obj->parent = nullptr;
    } else {
      assert(mdl->root == obj);
      mdl->root = nullptr;
    }

    obj->isSelected = false;
    buffer.push_back(obj);
  }
}

void CopyBuffer::Paste(Model* mdl, MdlObject* where) {
  for (auto& a : buffer) {
    if (where != nullptr) {
      MdlObject* obj = a->Clone();
      where->childs.push_back(obj);
      obj->parent = where;
      obj->isOpen = true;
    } else {
      if (mdl->root != nullptr) {
        fltk::message("There can only be one root object.");
        return;
      }
      mdl->root = a->Clone();
      mdl->root->parent = nullptr;
    }
  }
}

// ------------------------------------------------------------------------------------------------
// Toolbox controls
// ------------------------------------------------------------------------------------------------

void ModifyObjects(MdlObject* obj, Vector3 d, void (*fn)(MdlObject* obj, Vector3 d)) {
  fn(obj, d);
  for (auto& child : obj->childs) {
    ModifyObjects(child, d, fn);
  }
}

static const float SpeedMod = 0.05F;
static const float AngleMod = 1.0F;

struct ECameraTool : Tool {
  virtual ~ECameraTool() = default;
  ECameraTool() {
    isToggle = true;
    imageFile = "camera.gif";
  }

  bool toggle(bool /*enable*/) override { return true; }

  void mouse(EditorViewWindow* view, int msg, Point move) override;
} static CameraTool;

// ------------------------- move -----------------------------

struct EMoveTool : Tool {
  virtual ~EMoveTool() = default;
  EMoveTool() {
    isToggle = true;
    imageFile = "move.gif";
  }

  bool toggle(bool /*enable*/) override { return true; }

  static void apply(MdlObject* obj, Vector3 d) {
    if (obj->isSelected) {
      obj->position += d;
    }
  }

  static void ApplyMoveOp(Model* mdl, void* ud) {
    auto* d = static_cast<Vector3*>(ud);
    if (mdl->root != nullptr) {
      ModifyObjects(mdl->root, *d, apply);
    }
  }

  void mouse(EditorViewWindow* view, int msg, Point move) override {
    Point const m = move;

    if (((fltk::event_state() & fltk::ALT) != 0U) && ((fltk::event_state() & fltk::CTRL) == 0U)) {
      CameraTool.mouse(view, msg, move);
      return;
    }

    if ((msg == fltk::MOVE || msg == fltk::DRAG) && ((fltk::event_state() & fltk::BUTTON1) != 0U)) {
      Vector3 d;
      switch (view->GetMode()) {
        case MAP_3D:
          return;
        case MAP_XY:
          d = Vector3(m.x, -m.y, 0.0F);
          break;
        case MAP_XZ:
          d = Vector3(m.x, 0.0F, -m.y);
          break;
        case MAP_YZ:
          d = Vector3(0.0F, -m.y, m.x);
          break;
      }
      d /= view->cam.zoom;
      d *= SpeedMod;

      editor->RedrawViews();
    }
    if ((fltk::event_state() & fltk::BUTTON2) != 0U) {
      view->cam.MouseEvent(view, msg, move);
    }
  }
} static MoveTool;

void ECameraTool::mouse(EditorViewWindow* view, int msg, Point move) {
  int const s = fltk::event_state();

  if (((fltk::event_state() & fltk::CTRL) != 0U) && ((fltk::event_state() & fltk::ALT) == 0U)) {
    MoveTool.mouse(view, msg, move);
    return;
  }

  view->cam.MouseEvent(view, msg, move);
}

// ------------------------------ rotate ------------------------------

struct ERotateTool : public Tool {
  virtual ~ERotateTool() = default;
  ERotateTool() {
    imageFile = "rotate.gif";
    isToggle = true;
  }

  bool toggle(bool /*enabletool*/) override { return true; }

  static void apply(MdlObject* n, Vector3 rot) {
    if (n->isSelected) {
      n->rotation.AddEulerAbsolute(rot);

      //			Vector3 euler = n->rotation.GetEuler();
      //			if (ISNAN(euler.x) || ISNAN(euler.y) || ISNAN(euler.z))
      //				logger.Print("Rot: %f,%f,%f\n",rot.x,rot.y,rot.z);
    }
  }

  static void ApplyRotateOp(Model* mdl, void* d) {
    auto* rot = static_cast<Vector3*>(d);

    if (mdl->root != nullptr) {
      ModifyObjects(mdl->root, *rot, apply);
    }
  }

  void mouse(EditorViewWindow* view, int msg, Point move) override {
    if (((fltk::event_state() & fltk::ALT) != 0U) && ((fltk::event_state() & fltk::CTRL) == 0U)) {
      CameraTool.mouse(view, msg, move);
      return;
    }

    if ((msg == fltk::DRAG || msg == fltk::MOVE) && ((fltk::event_state() & fltk::BUTTON1) != 0U)) {
      Vector3 rot;
      float const r = AngleMod * move.x / 180.0F * M_PI;
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
    } else if ((fltk::event_state() & fltk::BUTTON2) != 0U) {
      view->cam.MouseEvent(view, msg, move);
    }
  }
} static RotateTool;

// --------------------------------- scale --------------------------------------

struct EScaleTool : public Tool {
  virtual ~EScaleTool() = default;
  EScaleTool() {
    imageFile = "scale.gif";
    isToggle = true;
  }

  bool toggle(bool /*enabletool*/) override { return true; }

  static void limitscale(Vector3* s) {
    if (fabs(s->x) < EPSILON) {
      s->x = (s->x >= 0) ? EPSILON : -EPSILON;
    }
    if (fabs(s->y) < EPSILON) {
      s->y = (s->y >= 0) ? EPSILON : -EPSILON;
    }
    if (fabs(s->z) < EPSILON) {
      s->z = (s->z >= 0) ? EPSILON : -EPSILON;
    }
  }

  static void apply(MdlObject* obj, Vector3 scale) {
    if (obj->isSelected) {
      obj->scale *= scale;
      limitscale(&obj->scale);
    }
  }

  static void ApplyScaleOp(Model* mdl, void* d) {
    auto* s = static_cast<Vector3*>(d);

    if (mdl->root != nullptr) {
      ModifyObjects(mdl->root, *s, apply);
    }
  }

  void mouse(EditorViewWindow* view, int msg, Point move) override {
    if (((fltk::event_state() & fltk::ALT) != 0U) && ((fltk::event_state() & fltk::CTRL) == 0U)) {
      CameraTool.mouse(view, msg, move);
      return;
    }

    if ((msg == fltk::DRAG || msg == fltk::MOVE) && ((fltk::event_state() & fltk::BUTTON1) != 0U)) {
      Vector3 s;
      float const sx = 1.0F + move.x * 0.01F;
      float const sy = 1.0F - move.y * 0.01F;

      switch (view->GetMode()) {
        case MAP_3D:
          return;
        case MAP_XY:
          s = Vector3(sx, sy, 1.0F);
          break;
        case MAP_XZ:
          s = Vector3(sx, 1.0F, sy);
          break;
        case MAP_YZ:
          s = Vector3(1.0F, sy, sx);
          break;
      }

      editor->RedrawViews();
    } else if ((fltk::event_state() & fltk::BUTTON2) != 0U) {
      view->cam.MouseEvent(view, msg, move);
    }
  }

} static ScaleTool;

// --------------------------------- origin move tool --------------------------------------
//  move the objects while moving the vertices in the opposite direction

struct EOriginMoveTool : public Tool {
  virtual ~EOriginMoveTool() = default;
  EOriginMoveTool() {
    imageFile = "originmove.png";
    isToggle = true;
  }

  bool toggle(bool /*enabletool*/) override { return true; }

  static void apply(MdlObject* o, Vector3 move) {
    if (!o->isSelected) {
      return;
    }

    o->MoveOrigin(move);
  }

  static void ApplyOriginMoveOp(Model* mdl, void* ud) {
    auto* d = static_cast<Vector3*>(ud);

    if (mdl->root != nullptr) {
      ModifyObjects(mdl->root, *d, apply);
    }
  }

  void mouse(EditorViewWindow* view, int msg, Point move) override {
    Point const m = move;

    if ((msg == fltk::MOVE || msg == fltk::DRAG) && ((fltk::event_state() & fltk::BUTTON1) != 0U)) {
      Vector3 d;
      switch (view->GetMode()) {
        case MAP_3D:
          return;
        case MAP_XY:
          d = Vector3(m.x, -m.y, 0.0F);
          break;
        case MAP_XZ:
          d = Vector3(m.x, 0.0F, -m.y);
          break;
        case MAP_YZ:
          d = Vector3(0.0F, -m.y, m.x);
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
  virtual ~ETextureTool() = default;
  bool enabled{false};
  ETextureTool() {
    imageFile = "texture.gif";
    isToggle = true;
  }

  static void applyTexture(MdlObject* o, std::shared_ptr<Texture> par_tex) {
    PolyMesh* pm = o->GetPolyMesh();
    if (pm != nullptr) {
      for (auto& a : pm->poly) {
        if (a->isSelected) {
          a->texture = par_tex;
          a->texname = par_tex->name;
        }
      }
    }
    for (auto& child : o->childs) {
      applyTexture(child, par_tex);
    }
  }

  static void callback(std::shared_ptr<Texture> tex, void* data) {
    auto* tool = static_cast<ETextureTool*>(data);
    Model* model = tool->editor->GetMdl();
    if (model->root != nullptr) {
      applyTexture(model->root, tex);
    }
    tool->editor->RedrawViews();
  }

  static void deselect(MdlObject* o) {
    for (PolyIterator pi(o); !pi.End(); pi.Next()) {
      pi->isSelected = false;
    }
  }

  bool toggle(bool enable) override {
    if (enable) {
      editor->SetTextureSelectCallback(callback, this);
      MdlObject* r = editor->GetMdl()->root;
      if (r != nullptr) {
        IterateObjects(r, deselect);
      }
    } else {
      editor->SetTextureSelectCallback(nullptr, nullptr);
    }
    enabled = enable;
    return true;
  }

  bool needsPolySelect() override { return enabled; }

  void mouse(EditorViewWindow* view, int msg, Point move) override {
    CameraTool.mouse(view, msg, move);
  }
} static TextureMapTool;

// --------------------------------- Tools collection --------------------------------------

struct EPolyColorTool : Tool {
  virtual ~EPolyColorTool() = default;
  Vector3 color;

  EPolyColorTool() : Tool() {
    imageFile = "color.gif";
    isToggle = false;
  }

  static void applyColor(MdlObject* o, Vector3 color) {
    for (PolyIterator pi(o); !pi.End(); pi.Next()) {
      if (pi->isSelected) {
        pi->color = color;
        pi->texname.clear();
        pi->texture = nullptr;
      }
    }
    for (auto& child : o->childs) {
      applyColor(child, color);
    }
  }

  bool toggle(bool /*enable*/) override { return true; }
  void click() override {
    if (!fltk::color_chooser("Color for selected polygons", color.x, color.y, color.z)) {
      return;
    }

    Model* m = editor->GetMdl();
    if (m->root != nullptr) {
      applyColor(m->root, color);
    }

    editor->RedrawViews();
  }
} static PolygonColorTool;

struct EPolyFlipTool : Tool {
  virtual ~EPolyFlipTool() = default;
  EPolyFlipTool() : Tool() {
    imageFile = "polyflip.png";
    isToggle = false;
  }

  static void flip(MdlObject* o) {
    for (PolyIterator pi(o); !pi.End(); pi.Next()) {
      if (pi->isSelected) {
        pi->Flip();
      }
    }
  }

  bool toggle(bool /*enable*/) override { return true; }
  void click() override {
    Model* m = editor->GetMdl();
    if (m->root != nullptr) {
      IterateObjects(m->root, flip);
    }
    editor->RedrawViews();
  }
} static PolygonFlipTool;

struct ERotateTexTool : Tool {
  virtual ~ERotateTexTool() = default;
  ERotateTexTool() {
    isToggle = false;
    imageFile = "rotatetex.png";
  }

  static void rotatetex(MdlObject* o) {
    for (PolyIterator p(o); !p.End(); p.Next()) {
      if (p->isSelected) {
        p->RotateVerts();
      }
    }
  }

  bool toggle(bool /*enable*/) override { return true; }
  void click() override {
    Model* m = editor->GetMdl();
    if (m->root != nullptr) {
      IterateObjects(m->root, rotatetex);
    }
    editor->RedrawViews();
  }
} static RotateTexTool;

struct ECurvedPolyTool : Tool {
  virtual ~ECurvedPolyTool() = default;
  ECurvedPolyTool() { isToggle = false, imageFile = "curvedpoly.png"; }

  static void togglecurved(MdlObject* o) {
    for (PolyIterator p(o); !p.End(); p.Next()) {
      if (p->isSelected) {
        p->isCurved = !p->isCurved;
      }
    }
  }

  bool toggle(bool /*enable*/) override { return true; }
  void click() override {
    Model* m = editor->GetMdl();
    if (m->root != nullptr) {
      IterateObjects(m->root, togglecurved);
    }
    editor->RedrawViews();
  }
} static ToggleCurvedPolyTool;

// --------------------------------- Tools collection --------------------------------------
Tools::Tools() : camera(&CameraTool), move(&MoveTool), rotate(&RotateTool), scale(&ScaleTool) {
  texmap = &TextureMapTool;
  color = &PolygonColorTool;
  flip = &PolygonFlipTool;
  originMove = &OriginMoveTool;
  rotateTex = &RotateTexTool;
  toggleCurvedPoly = &ToggleCurvedPolyTool;

  Tool* _tools[] = {camera,     move,      rotate,           scale,  texmap, color, flip,
                    originMove, rotateTex, toggleCurvedPoly, nullptr};
  for (std::uint32_t a = 0; _tools[a] != nullptr; a++) {
    tools.push_back(_tools[a]);
  }
}

Tool* Tools::GetDefaultTool() const { return camera; }

void Tools::Disable() {
  for (auto& tool : tools) {
    if (tool->isToggle) {
      tool->toggle(false);
    }
  }
}

void Tools::SetEditor(IEditor* editor) {
  for (auto& tool : tools) {
    tool->editor = editor;
  }
}

void Tools::LoadImages() {
  auto archive = CZipArchive("data/buttons.ups");
  for (auto& tool : tools) {
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

    if (!image.has_alpha() && !image.add_alpha()) {
      spdlog::error("Failed to add alpha to tool image: '{}', error was: {}", tool->imageFile,
                    image.error());
      continue;
    }

    auto* img = new fltk::Image(image.data(), fltk::RGBA, image.width(), image.height());
    tool->button->image(img);
    tool->button->label("");
  }
}

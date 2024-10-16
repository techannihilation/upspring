//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#include <fltk/Button.h>
#include <fltk/rgbImage.h>

#include "View.h"

class Tool {
 public:
  enum { map2d, map3d } maptype;

  Tool() {
    isToggle = false;
    editor = 0;
    isRadio = true;
    maptype = map2d;
    imageFile = 0;
  }
  ~Tool() {}
  // on a mapview
  // msg = fltk::PUSH, fltk::RELEASE, fltk::DRAG, fltk::MOVE
  virtual void mouse(EditorViewWindow* /*view*/, int /*msg*/, Point /*move*/) {
  }  // scene is in view
  virtual void click() {}
  virtual bool toggle(bool /*enabletool*/) { return false; }  // return true if it may be toggled
  virtual bool needsPolySelect() { return false; }

  bool isToggle;  // is this a toggle tool or regular tool?
  bool isRadio;
  IEditor* editor;
  fltk::Button* button;
  const char* imageFile;
};

struct Tools {
  Tools();
  void SetEditor(IEditor* editor);
  void Disable();
  Tool* GetDefaultTool() const;
  void LoadImages();

  Tool* camera;
  Tool* move;
  Tool* rotate;
  Tool* scale;
  Tool* texmap;
  Tool* color;
  Tool* flip;
  Tool* originMove;
  Tool* rotateTex;
  Tool* toggleCurvedPoly;

  std::vector<Tool*> tools;
};

struct MdlObject;

struct CopyBuffer {
  CopyBuffer();
  ~CopyBuffer();

  void Copy(Model* mdl);
  void Cut(Model* mdl);
  void Paste(Model* mdl, MdlObject* where);
  void Clear();

  std::vector<MdlObject*> buffer;
};

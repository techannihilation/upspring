//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------
#include "EditorIncl.h"
#include "EditorDef.h"

#include "Model.h"
#include "EditorUI.h"
#include "ObjectView.h"
#include "Util.h"

EditorUI::ObjectView::ObjectView(EditorUI* editor, fltk::Browser* tree)
    : editor(editor), tree(tree) {
  browserList.editor = editor;
  tree->list(&browserList);
}

EditorUI::ObjectView::~ObjectView() = default;

void EditorUI::ObjectView::Update() const {
  tree->redraw();
  tree->relayout();
}

// ------------------------------------------------------------------------------------------------
// List
// ------------------------------------------------------------------------------------------------

EditorUI::ObjectView::List::List() : rootObject(new MdlObject) {}
EditorUI::ObjectView::List::~List() {
  rootObject->childs.clear();
  delete rootObject;
}

MdlObject* EditorUI::ObjectView::List::root_obj() const {
  rootObject->childs.resize(1);
  rootObject->childs[0] = editor->model->root;
  return editor->model->root != nullptr ? rootObject : nullptr;
}

int EditorUI::ObjectView::List::children(const fltk::Menu* /*unused*/, const int* indexes,
                                         int level) {
  MdlObject* obj = root_obj();
  if (obj == nullptr) {
    return -1;
  }
  while ((level--) != 0) {
    int const i = *indexes++;
    // if (i < 0 || i >= group->children()) return -1;
    MdlObject* ch = obj->childs[i];
    if (ch->childs.empty()) {
      return -1;
    }
    obj = ch;
  }
  return obj->childs.size();
}

fltk::Widget* EditorUI::ObjectView::List::child(const fltk::Menu* /*unused*/, const int* indexes,
                                                int level) {
  MdlObject* obj = root_obj();
  if (obj == nullptr) {
    return nullptr;
  }
  for (;;) {
    size_t const i = *indexes++;
    if (i < 0 || i >= obj->childs.size()) {
      return nullptr;
    }
    MdlObject* ch = obj->childs[i];  // Widget* widget = group->child(i);
    if ((level--) == 0) {
      obj = ch;
      break;
    }
    if (ch->childs.empty()) {
      return nullptr;
    }
    obj = ch;
  }

  // init widget
  item.label(obj->name.c_str());
  item.w(0);
  item.user_data(obj);

  // set selection
  item.set_flag(fltk::SELECTED, obj->isSelected);

  // set open/closed state
  item.set_flag(fltk::OPENED, obj->isOpen && !obj->childs.empty());

  return &item;
}

void EditorUI::ObjectView::List::flags_changed(const fltk::Menu* /*unused*/, fltk::Widget* w) {
  auto* obj = static_cast<MdlObject*>(w->user_data());
  obj->isOpen = ((w->flags() & fltk::STATE) != 0);
  obj->isSelected = ((w->flags() & fltk::SELECTED) != 0);
  editor->SelectionUpdated();
}

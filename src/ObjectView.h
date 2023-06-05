//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

class EditorUI::ObjectView {
 public:
  ObjectView(EditorUI* editor, fltk::Browser* tree);
  ~ObjectView();

  void Update() const;

  EditorUI* editor;
  fltk::Browser* tree;

 protected:
  struct Item : public fltk::Item {
    int handle(int event);
  };

  struct List : public fltk::List {
    List();
    ~List();
    fltk::Widget* child(const fltk::Menu*, const int* indexes, int level);
    int children(const fltk::Menu*, const int* indexes, int level);
    void flags_changed(const fltk::Menu*, fltk::Widget*);
    MdlObject* root_obj() const;

    fltk::Item item;
    EditorUI* editor{0};
    MdlObject* rootObject;  // fake root object just for GUI displaying purposes
  } browserList;
};

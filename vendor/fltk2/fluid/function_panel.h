// generated by Fast Light User Interface Designer (fluid) version 2.1000

#ifndef function_panel_h
#define function_panel_h
#include <fltk/Window.h>
extern fltk::Window* function_panel;
#include <fltk/CheckButton.h>
extern fltk::CheckButton* f_public_button;
extern fltk::CheckButton* f_c_button;
#include <fltk/Input.h>
extern fltk::Input* f_name_input;
extern fltk::Input* f_attributes_input;
extern fltk::Input* f_return_type_input;
#include <fltk/ReturnButton.h>
extern fltk::ReturnButton* f_panel_ok;
#include <fltk/Button.h>
extern fltk::Button* f_panel_cancel;
#include <fltk/Choice.h>
#include <fltk/Item.h>
fltk::Window* make_function_panel();
extern fltk::Window* code_panel;
#include "CodeEditor.h"
extern CodeEditor* code_input;
#include <fltk/Group.h>
#include <fltk/InvisibleBox.h>
extern fltk::ReturnButton* code_panel_ok;
extern fltk::Button* code_panel_cancel;
fltk::Window* make_code_panel();
extern fltk::Window* codeblock_panel;
extern fltk::Input* code_before_input;
extern fltk::Input* code_after_input;
extern fltk::ReturnButton* codeblock_panel_ok;
extern fltk::Button* codeblock_panel_cancel;
fltk::Window* make_codeblock_panel();
extern fltk::Window* declblock_panel;
extern fltk::Input* decl_before_input;
extern fltk::Input* decl_after_input;
extern fltk::ReturnButton* declblock_panel_ok;
extern fltk::Button* declblock_panel_cancel;
fltk::Window* make_declblock_panel();
extern fltk::Window* decl_panel;
extern fltk::CheckButton* decl_public_button;
extern fltk::Input* decl_input;
extern fltk::ReturnButton* decl_panel_ok;
extern fltk::Button* decl_panel_cancel;
fltk::Window* make_decl_panel();
extern fltk::Window* class_panel;
extern fltk::CheckButton* c_public_button;
extern fltk::Input* c_name_input;
extern fltk::Input* c_subclass_input;
extern fltk::ReturnButton* c_panel_ok;
extern fltk::Button* c_panel_cancel;
fltk::Window* make_class_panel();
extern fltk::Window* namespace_panel;
extern fltk::Input* namespace_input;
extern fltk::ReturnButton* namespace_panel_ok;
extern fltk::Button* namespace_panel_cancel;
fltk::Window* make_namespace_panel();
#include <fltk/DoubleBufferWindow.h>
extern fltk::DoubleBufferWindow* comment_panel;
#include <fltk/MultiLineInput.h>
extern fltk::MultiLineInput* comment_input;
extern fltk::ReturnButton* comment_panel_ok;
extern fltk::Button* comment_panel_cancel;
#include <fltk/LightButton.h>
extern fltk::LightButton* comment_in_source;
extern fltk::LightButton* comment_in_header;
#include <fltk/PopupMenu.h>
extern fltk::PopupMenu* comment_predefined;
extern fltk::Button* comment_load;
fltk::DoubleBufferWindow* make_comment_panel();
extern void toggle_sourceview_cb(fltk::DoubleBufferWindow*, void*);
extern fltk::DoubleBufferWindow* sourceview_panel;
#include <fltk/TabGroup.h>
extern void update_sourceview_position_cb(fltk::TabGroup*, void*);
extern fltk::TabGroup* sv_tab;
extern CodeViewer* sv_source;
extern CodeViewer* sv_header;
extern void update_sourceview_cb(fltk::Button*, void*);
extern fltk::LightButton* sv_autorefresh;
extern fltk::LightButton* sv_autoposition;
extern void toggle_sourceview_b_cb(fltk::Button*, void*);
fltk::DoubleBufferWindow* make_sourceview();
#endif

//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#include "EditorIncl.h"
#include "EditorDef.h"

#include "Texture.h"
#include "TextureBrowser.h"

#include <IL/il.h>
#include <fltk/draw.h>
// ------------------------------------------------------------------------------------------------
// TextureBrowser::Item
// ------------------------------------------------------------------------------------------------

TextureBrowser::Item::Item(std::shared_ptr<Texture> par_texture) : fltk::Widget(0, 0, 50, 50), tex(par_texture), selected(false) {
  color((fltk::Color)0x1f1f1f00);
  tooltip(par_texture->name.c_str());
}

int TextureBrowser::Item::handle(int event) {
  switch (event) {
    case fltk::PUSH:
      ((TextureBrowser*)parent())->SelectItem(this);
      break;
  }

  return Widget::handle(event);
}

void TextureBrowser::Item::draw() {
  if (!tex || !tex->IsLoaded()) {
    fltk::Widget::draw();
    return;
  }

  auto img = tex->image;

  if (selected) {
    fltk::Rectangle selr(-3, -3, img.width() + 6, img.height() + 6);
    fltk::push_clip(selr);
    fltk::setcolor(fltk::BLUE);
    fltk::fillrect(selr);
    fltk::pop_clip();
  }

  fltk::Rectangle rect(0, 0, img.width(), img.height());
  if (img.has_alpha())
    fltk::drawimage(img.data(), fltk::RGBA, rect);
  else
    fltk::drawimage(img.data(), fltk::RGBA, rect);
}

// ------------------------------------------------------------------------------------------------
// TextureBrowser
// ------------------------------------------------------------------------------------------------

TextureBrowser::TextureBrowser(int X, int Y, int W, int H, const char* lbl)
    : fltk::ScrollGroup(X, Y, W, H, lbl) {
  UpdatePositions();
  prevWidth = W;
  selectCallback = 0;
  selectCallbackData = 0;
}

void TextureBrowser::layout() {
  if (prevWidth != w()) {
    UpdatePositions(false);
    prevWidth = w();
  }

  fltk::ScrollGroup::layout();
}

void TextureBrowser::AddTexture(std::shared_ptr<Texture> t) { add(new Item(t)); }

void TextureBrowser::RemoveTexture(std::shared_ptr<Texture> t) {
  int nc = children();
  for (int a = 0; a < nc; a++) {
    Item* item = (Item*)child(a);
    if (item->tex == t) {
      remove(a);
      break;
    }
  }
}

void TextureBrowser::SelectItem(Item* i) {
  if (!fltk::event_key_state(fltk::LeftShiftKey) && !fltk::event_key_state(fltk::RightShiftKey)) {
    int nc = children();
    for (int a = 0; a < nc; a++) {
      Item* item = (Item*)child(a);

      item->selected = false;
    }

    if (selectCallback) selectCallback(i->tex, selectCallbackData);
  }
  i->selected = true;
  redraw();
}

std::vector<std::shared_ptr<Texture>> TextureBrowser::GetSelection() {
  std::vector<std::shared_ptr<Texture>> sel;

  int nc = children();
  for (int a = 0; a < nc; a++) {
    Item* item = (Item*)child(a);

    if (item->selected) sel.push_back(item->tex);
  }
  return sel;
}

// calculates child widget positions
void TextureBrowser::UpdatePositions(bool /*bRedraw*/) {
  int x = 0, y = 0;
  int rowHeight = 0;
  int nc = children();

  for (int a = 0; a < nc; a++) {
    Item* item = (Item*)child(a);

    if (!item->tex->IsLoaded()) continue;

    int texWidth = item->tex->image.width();
    int texHeight = item->tex->image.height();

    const int space = 5;
    if (x + texWidth + space > w()) {  // reserve 5 pixels for vertical scrollbar
      item->x(0);
      y += rowHeight + space;
      item->y(y);
      rowHeight = 0;
      x = 0;
    } else {
      item->x(x);
      item->y(y);
    }
    item->w(texWidth);
    item->h(texHeight);
    x += texWidth + space;

    if (texHeight >= rowHeight) rowHeight = texHeight;
  }

  // redraw();
}

void TextureBrowser::SetSelectCallback(TextureSelectCallback cb, void* data) {
  selectCallback = cb;
  selectCallbackData = data;
}

void TextureBrowser::SelectAll() {
  int nc = children();
  for (int a = 0; a < nc; a++) ((Item*)child(a))->selected = true;
  redraw();
}

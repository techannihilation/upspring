//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------
#ifndef JC_TEXTURE_BROWSER_H
#define JC_TEXTURE_BROWSER_H

#include "Texture.h"

typedef void (*TextureSelectCallback)(std::shared_ptr<Texture> tex, void* data);

class TextureBrowser : public fltk::ScrollGroup {
 public:
  TextureBrowser(int X, int Y, int W, int H, const char* lbl = 0);

  // void resize(int x,int y,int w,int h);
  void layout();

  struct Item : public fltk::Widget {
    Item(std::shared_ptr<Texture> par_texture);
    std::shared_ptr<Texture> tex;
    bool selected;

    int handle(int event);
    void draw();
  };

  void AddTexture(std::shared_ptr<Texture> t);
  void RemoveTexture(std::shared_ptr<Texture> t);
  void UpdatePositions(bool bRedraw = true);
  std::vector<std::shared_ptr<Texture>> GetSelection();
  void SelectAll();
  void SetSelectCallback(TextureSelectCallback cb, void* data);

 protected:
  int prevWidth;
  TextureSelectCallback selectCallback;
  void* selectCallbackData;

  void SelectItem(Item* i);
};

#endif

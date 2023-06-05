//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#include <IL/il.h>
#include <IL/ilu.h>

#include "EditorDef.h"
#include "EditorIncl.h"
#include "EditorUI.h"

TexBuilderUI::TexBuilderUI(const char* tex1, const char* tex2) {
  CreateUI();

  if (tex1 != nullptr) {
    output1->value(tex1);
  }
  if (tex2 != nullptr) {
    output2->value(tex2);
  }
}

TexBuilderUI::~TexBuilderUI() { delete window; }

void TexBuilderUI::Show() const {
  window->set_non_modal();
  window->show();
}

void TexBuilderUI::Browse(fltk::Input* inputBox, bool isOutputTex) {
  static std::string fn;

  if (isOutputTex) {
    if (FileSaveDlg("Enter filename to save to:", ImageFileExt, fn)) {
      inputBox->value(fn.c_str());
    }
  } else {
    if (FileOpenDlg("Select input texture:", ImageFileExt, fn)) {
      inputBox->value(fn.c_str());
    }
  }
  inputBox->redraw();
}

unsigned int TexBuilderUI::LoadImg(const char* fn) {
  ILuint img = 0;

  ilGenImages(1, &img);
  ilBindImage(img);

  if (ilLoadImage(const_cast<ILstring>(fn)) == 0U) {
    ilDeleteImages(1, &img);
    return 0;
  }
  return img;
}

inline int ImgWidth(ILuint img) {
  ilBindImage(img);
  return ilGetInteger(IL_IMAGE_WIDTH);
}
inline int ImgHeight(ILuint img) {
  ilBindImage(img);
  return ilGetInteger(IL_IMAGE_HEIGHT);
}

// NOTE: kills the use of goto's below and fixes "jump to label crosses initialization of..."
void freeimgs(ILuint a, ILuint b) {
  if (a != 0U) {
    iluDeleteImage(a);
  }
  if (b != 0U) {
    iluDeleteImage(b);
  }
}

void TexBuilderUI::BuildTexture1() {
  ILuint color = 0;
  ILuint teamcol = 0;

  if ((colorTex->size() == 0) || (teamColorTex->size() == 0) || (output1->size() == 0)) {
    fltk::message("Not all required filenames given.");
    return;
  }

  if ((color = LoadImg(colorTex->value())) == 0 ||
      (ilConvertImage(IL_RGB, IL_UNSIGNED_BYTE) == 0U)) {
    fltk::message("Failed to load color texture %s", colorTex->value());
    freeimgs(color, teamcol);  // goto freeimgs;
    return;
  }
  if ((teamcol = LoadImg(teamColorTex->value())) == 0 ||
      (ilConvertImage(IL_LUMINANCE, IL_UNSIGNED_BYTE) == 0U)) {
    fltk::message("Failed to load team color texture %s", teamColorTex->value());
    freeimgs(color, teamcol);  // goto freeimgs;
    return;
  }

  int const w = ImgWidth(color);
  int const h = ImgHeight(color);

  if (ImgWidth(teamcol) != w || ImgHeight(teamcol) != h) {
    fltk::message("Team color texture must have the same dimensions as the color texture");
    freeimgs(color, teamcol);  // goto freeimgs;
    return;
  }

  // build the first texture (color + teamcolor)
  ilBindImage(color);
  uchar* colorSrc = ilGetData();
  ilBindImage(teamcol);
  uchar* teamcolSrc = ilGetData();
  bool const invert = invertTeamCol->value();
  auto* dst = new uchar[w * h * 4];
  uchar* dstp = dst;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      *(dstp++) = *(colorSrc++);
      *(dstp++) = *(colorSrc++);
      *(dstp++) = *(colorSrc++);
      uchar const tc = *(teamcolSrc++);
      *(dstp++) = invert ? (255 - tc) : tc;
    }
  }

  ILuint tex1 = 0;
  ilGenImages(1, &tex1);
  ilBindImage(tex1);
  ilTexImage(w, h, 1, 4, IL_RGBA, IL_UNSIGNED_BYTE, dst);

  if (ilSaveImage(const_cast<ILstring>(output1->value())) == 0U) {
    fltk::message("Failed to write texture 1 to:\n%s", output1->value());
  }

  iluDeleteImage(tex1);
  delete[] dst;

  fltk::message("Texture 1 succesfully generated and saved to: %s", output1->value());

  // freeimgs:
  //	if (color) iluDeleteImage(color);
  //	if (teamcol) iluDeleteImage(teamcol);
  freeimgs(color, teamcol);
}

void TexBuilderUI::BuildTexture2() {
  ILuint reflect = 0;
  ILuint selfillum = 0;

  if ((reflectTex->size() == 0) || (selfIllumTex->size() == 0) || (output2->size() == 0)) {
    fltk::message("Not all required filenames given.");
    return;
  }

  if ((reflect = LoadImg(reflectTex->value())) == 0 ||
      (ilConvertImage(IL_LUMINANCE, IL_UNSIGNED_BYTE) == 0U)) {
    fltk::message("Failed to load reflectiveness texture %s", reflectTex->value());
    freeimgs(reflect, selfillum);  // goto freeimgs;
    return;
  }
  if ((selfillum = LoadImg(selfIllumTex->value())) == 0 ||
      (ilConvertImage(IL_LUMINANCE, IL_UNSIGNED_BYTE) == 0U)) {
    fltk::message("Failed to load self-illumination texture %s", selfIllumTex->value());
    freeimgs(reflect, selfillum);  // goto freeimgs;
    return;
  }

  int w = 0;
  int h = 0;

  if (ImgWidth(reflect) != ImgWidth(selfillum) || ImgHeight(reflect) != ImgHeight(selfillum)) {
    fltk::message(
        "Reflectiveness texture should have the same dimensions as the self-illumination texture");
    freeimgs(reflect, selfillum);  // goto freeimgs;
    return;
  }

  // build the second texture (selfillum + reflectiveness)
  w = ImgWidth(reflect);
  h = ImgHeight(reflect);

  ilBindImage(reflect);
  uchar* reflectSrc = ilGetData();
  ilBindImage(selfillum);
  uchar* selfillumSrc = ilGetData();

  auto* dst = new uchar[w * h * 3];
  uchar* dstp = dst;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      *(dstp++) = *(selfillumSrc++);
      *(dstp++) = *(reflectSrc++);
      *(dstp++) = 0;
    }
  }

  ILuint tex2 = 0;
  ilGenImages(1, &tex2);
  ilBindImage(tex2);
  ilTexImage(w, h, 1, 3, IL_RGB, IL_UNSIGNED_BYTE, dst);

  if (ilSaveImage(const_cast<ILstring>(output2->value())) == 0U) {
    fltk::message("Failed to write texture 2 to:\n%s", output2->value());
  }

  iluDeleteImage(tex2);
  delete[] dst;

  fltk::message("Texture 2 succesfully generated to: %s", output2->value());

  // freeimgs:
  //	if (reflect) iluDeleteImage(reflect);
  //	if (selfillum) iluDeleteImage(selfillum);
  freeimgs(reflect, selfillum);
}

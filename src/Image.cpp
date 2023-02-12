#include "EditorIncl.h"
#include "EditorDef.h"

#include "Image.h"
#include "Util.h"

#include <IL/il.h>
#include <IL/ilu.h>

#include <iostream>
#include <filesystem>

#include <fstream>
#include <sstream>
#include <vector>
#include <string>

#include "spdlog/spdlog.h"

struct ILInitializer {
  ILInitializer() {
    ilInit();
    iluInit();
  }
  ~ILInitializer() { ilShutDown(); }
} static devilInstance;

// -------------------------------- Image ---------------------------------

Image::~Image() {
  if (mILID == 0) {
    return;
  }

  ilDeleteImage(mILID);
}

Image::Image(const std::vector<uchar>& pBuf) : Image() { LoadFromMemory_(pBuf); }

Image& Image::operator=(const Image& other) {
  if (this == &other) {
    return *this;
  }

  if (other.HasError()) {
    mHasError = true;
    mError = other.Error();
    return *this;
  }

  mILID = ilGenImage();
  ilBindImage(mILID);
  ilCopyImage(other.ID());
  mWidth = other.mWidth;
  mHeight = other.mHeight;
  mBytesPerPixel = other.mBytesPerPixel;
  mDeepth = other.mDeepth;
  mChannels = other.mChannels;

  return *this;
}

Image& Image::operator=(Image&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    if (other.HasError()) {
      mHasError = true;
      mError = other.Error();
      return *this;
    }

    mILID = other.mILID;
    mWidth = other.mWidth;
    mHeight = other.mHeight;
    mBytesPerPixel = other.mBytesPerPixel;
    mDeepth = other.mDeepth;
    mChannels = other.mChannels;
    return *this;
}

Image::Image(const std::string& pFile) : Image() {
  const std::basic_ifstream<uchar> file(pFile);
  std::basic_ostringstream<uchar> stream;
  stream << file.rdbuf();
  const std::vector<uchar> buf(stream.str().begin(), stream.str().end());

  LoadFromMemory_(buf);
}

Image::Image(int pWidth, int pHeight)
    : mILID(ilGenImage()),
      mHasError(false),
      mWidth(),
      mHeight(),
      mBytesPerPixel(),
      mDeepth(),
      mChannels() {
  ilBindImage(mILID);
  ilSetInteger(IL_IMAGE_WIDTH, pWidth);
  ilSetInteger(IL_IMAGE_HEIGHT, pHeight);
  ilSetInteger(IL_IMAGE_CHANNELS, 4);
  ilClearImage();

  // Read back image infos
  GetImageInfos_();
}

Image::Image(int pWidth, int pHeight, float pRed, float pGreen, float pBlue)
    : mILID(ilGenImage()),
      mHasError(false),
      mWidth(),
      mHeight(),
      mBytesPerPixel(),
      mDeepth(),
      mChannels() {
  ilBindImage(mILID);
  ilSetInteger(IL_IMAGE_WIDTH, pWidth);
  ilSetInteger(IL_IMAGE_HEIGHT, pHeight);
  ilSetInteger(IL_IMAGE_CHANNELS, 3);

  ClearColor(pRed / 255.0F, pGreen / 255.0F, pBlue / 255.0F, 1.0F);

  // Read back image infos
  GetImageInfos_();
}

bool Image::Save(const std::string& pFile) const {
  if (HasError()) {
    return false;
  }

  ilBindImage(mILID);
  ilEnable(IL_FILE_OVERWRITE);
  return ilSaveImage(static_cast<const ILstring>(pFile.c_str())) == IL_TRUE;
}

const std::uint8_t* Image::Data() const {
  if (HasError()) {
    return nullptr;
  }

  ilBindImage(mILID);
  return ilGetData();
}

bool Image::ClearColor(float pRed, float pGreen, float pBlue, float pAlpha) {
  if (HasError()) {
    return false;
  }

  ilBindImage(mILID);
  ilClearColour(pRed, pGreen, pBlue, pAlpha);
  ilClearImage();

  return true;
}

/*
Add/Remove alpha channel
*/
bool Image::AddAlpha() {
  if (HasError() or HasAlpha()) {
    return false;
  }

  ilBindImage(mILID);
  ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);
  return true;
}

bool Image::SetAlphaZero() {
  if (HasError() or !HasAlpha()) {
    return false;
  }

  return ClearColor(0.0F, 0.0F, 0.0F, 1.0F);
}

bool Image::SetNonAlphaZero() {
  if (HasError() or !HasAlpha()) {
    return false;
  }

  return ClearColor(1.0F, 1.0F, 1.0F, 0.0F);
}

bool Image::Mirror() {
  if (HasError()) {
    return false;
  }

  ilBindImage(mILID);
  iluMirror();

  return true;
}

bool Image::Flip() {
  if (HasError()) {
    return false;
  }

  ilBindImage(mILID);
  iluFlipImage();

  return true;
}

bool Image::FlipNonDDS(const std::string& path) {
  auto ext = std::filesystem::path(path).extension();
  if (ext != ".dds") {
    return Flip();
  }

  return false;
}

/*
This function is not intended to actually draw things (it doesn't do any clipping),
it is just a way to copy certain parts of an image.
The formats have to be exactly the same
*/
bool Image::Blit(const Image& pSrc, int pDx, int pDy, int pDz, int pSx, int pSy, int pSz, int pWidth,
                 int pHeight, int pDepth) {
  if (HasError() or pSrc.HasError()) {
    return false;
  }

  ilBindImage(mILID);
  return ilBlit(pSrc.ID(), pDx, pDy, pDz, pSx, pSy, pSz, pWidth, pHeight, pDepth) == IL_TRUE;
}

void Image::LoadFromMemory_(const std::vector<uchar>& pBuf) {
  if (mILID != 0) {
    ilDeleteImage(mILID);
  }

  ilGenImages(1, &mILID);
  ilBindImage(mILID);

  // /* Convert paletted to packed colors */
  // ilEnable(IL_CONV_PAL);
  // ilHint(IL_MEM_SPEED_HINT, IL_FASTEST);
  // ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
  // ilEnable(IL_ORIGIN_SET);

  auto ret = ilLoadL(IL_TYPE_UNKNOWN, pBuf.data(), pBuf.size());
  if (ret == IL_FALSE) {
    mHasError = true;
    mError = std::string(iluErrorString(ilGetError()));
    spdlog::error("Failed to read image, error was: {}", mError);
    return;
  }

  mHasError = false;
  GetImageInfos_();
}

void Image::GetImageInfos_()
{
  if (HasError()) {
    return;
  }

  ilGetIntegerv(IL_IMAGE_WIDTH, &mWidth);
  ilGetIntegerv(IL_IMAGE_HEIGHT, &mHeight);
  ilGetIntegerv(IL_IMAGE_BYTES_PER_PIXEL, &mBytesPerPixel);
  ilGetIntegerv(IL_IMAGE_DEPTH, &mDeepth);
  ilGetIntegerv(IL_IMAGE_CHANNELS, &mChannels);
}
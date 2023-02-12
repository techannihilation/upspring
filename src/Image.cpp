#include "Image.h"

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
  if (ilid_ == 0) {
    return;
  }

  ilDeleteImage(ilid_);
}

Image& Image::operator=(const Image& rhs) {
  if (this == &rhs) {
    return *this;
  }

  if (rhs.has_error()) {
    has_error_ = true;
    error_ = rhs.error();
    return *this;
  }

  ilid_ = ilGenImage();
  ilBindImage(ilid_);
  ilCopyImage(rhs.id());
  width_ = rhs.width_;
  height_ = rhs.height_;
  bpp_ = rhs.bpp_;
  deepth_ = rhs.deepth_;
  channels_ = rhs.channels_;

  return *this;
}

Image& Image::operator=(Image&& rhs) noexcept {
    if (this == &rhs) {
      return *this;
    }

    if (rhs.has_error()) {
      has_error_ = true;
      error_ = rhs.error();
      return *this;
    }

    ilid_ = rhs.ilid_;
    width_ = rhs.width_;
    height_ = rhs.height_;
    bpp_ = rhs.bpp_;
    deepth_ = rhs.deepth_;
    channels_ = rhs.channels_;
    return *this;
}

bool Image::load(const std::vector<std::uint8_t>& par_buffer) {
  return load_from_memory_(par_buffer);
}

bool Image::load(const std::string& par_file) {
  const std::basic_ifstream<std::uint8_t> file(par_file);
  std::basic_ostringstream<std::uint8_t> stream;
  stream << file.rdbuf();
  const std::vector<std::uint8_t> buf(stream.str().begin(), stream.str().end());

  return load_from_memory_(buf);
}

bool Image::create(int par_width, int par_height) {
  ilid_ = ilGenImage();
  ilBindImage(ilid_);
  ilSetInteger(IL_IMAGE_WIDTH, par_width);
  ilSetInteger(IL_IMAGE_HEIGHT, par_height);
  ilSetInteger(IL_IMAGE_CHANNELS, 4);
  ilClearImage();

  // Read back image infos
  has_error_ = false;
  image_infos_();

  return true;
}

bool Image::create(int par_width, int par_height, float par_red, float par_green, float par_blue) {
  ilid_ = ilGenImage();
  ilBindImage(ilid_);
  ilSetInteger(IL_IMAGE_WIDTH, par_width);
  ilSetInteger(IL_IMAGE_HEIGHT, par_height);
  ilSetInteger(IL_IMAGE_CHANNELS, 3);

  ClearColor(par_red / 255.0F, par_green / 255.0F, par_blue / 255.0F, 1.0F);

  // Read back image infos
  has_error_ = false;
  image_infos_();

  return true;
}

bool Image::Save(const std::string& par_file) const {
  if (has_error()) {
    return false;
  }

  ilBindImage(ilid_);
  ilEnable(IL_FILE_OVERWRITE);
  return ilSaveImage(static_cast<const ILstring>(par_file.c_str())) == IL_TRUE;
}

const std::uint8_t* Image::data() const {
  if (has_error()) {
    return nullptr;
  }

  ilBindImage(ilid_);
  return ilGetData();
}

bool Image::ClearColor(float pRed, float pGreen, float pBlue, float pAlpha) {
  if (has_error()) {
    return false;
  }

  ilBindImage(ilid_);
  ilClearColour(pRed, pGreen, pBlue, pAlpha);
  ilClearImage();

  return true;
}

/*
Add/Remove alpha channel
*/
bool Image::AddAlpha() {
  if (has_error() or has_alpha()) {
    return false;
  }

  ilBindImage(ilid_);
  ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);
  return true;
}

bool Image::SetAlphaZero() {
  if (has_error() or !has_alpha()) {
    return false;
  }

  return ClearColor(0.0F, 0.0F, 0.0F, 1.0F);
}

bool Image::SetNonAlphaZero() {
  if (has_error() or !has_alpha()) {
    return false;
  }

  return ClearColor(1.0F, 1.0F, 1.0F, 0.0F);
}

bool Image::Mirror() {
  if (has_error()) {
    return false;
  }

  ilBindImage(ilid_);
  iluMirror();

  return true;
}

bool Image::Flip() {
  if (has_error()) {
    return false;
  }

  ilBindImage(ilid_);
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
  if (has_error() or pSrc.has_error()) {
    return false;
  }

  ilBindImage(ilid_);
  return ilBlit(pSrc.id(), pDx, pDy, pDz, pSx, pSy, pSz, pWidth, pHeight, pDepth) == IL_TRUE;
}

bool Image::load_from_memory_(const std::vector<std::uint8_t>& pBuf) {
  if (ilid_ != 0) {
    ilDeleteImage(ilid_);
  }

  ilGenImages(1, &ilid_);
  ilBindImage(ilid_);

  // /* Convert paletted to packed colors */
  // ilEnable(IL_CONV_PAL);
  // ilHint(IL_MEM_SPEED_HINT, IL_FASTEST);
  // ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
  // ilEnable(IL_ORIGIN_SET);

  auto ret = ilLoadL(IL_TYPE_UNKNOWN, pBuf.data(), pBuf.size());
  if (ret == IL_FALSE) {
    has_error_ = true;
    error_ = std::string(iluErrorString(ilGetError()));
    spdlog::error("Failed to read image, error was: {}", error_);
    return false;
  }

  has_error_ = false;
  image_infos_();

  return true;
}

void Image::image_infos_()
{
  if (has_error()) {
    return;
  }

  ilGetIntegerv(IL_IMAGE_WIDTH, &width_);
  ilGetIntegerv(IL_IMAGE_HEIGHT, &height_);
  ilGetIntegerv(IL_IMAGE_BYTES_PER_PIXEL, &bpp_);
  ilGetIntegerv(IL_IMAGE_DEPTH, &deepth_);
  ilGetIntegerv(IL_IMAGE_CHANNELS, &channels_);
}
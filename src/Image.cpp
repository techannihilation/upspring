#include "Image.h"

#include <IL/il.h>
#include <IL/ilu.h>

#include <cstddef>
#include <vector>
#include <string>
#include <filesystem>

#include <iostream>

#include "spdlog/spdlog.h"

struct Imagelib {
  Imagelib() {
    ilInit();
    iluInit();
  }
  ~Imagelib() { ilShutDown(); }
} static imagelib;

// https://stackoverflow.com/a/108360/3368468
bool is_power_of_two(int n) { return (n > 0 && ((n & (n - 1)) == 0)); }

// https://stackoverflow.com/a/466242/3368468
int next_power_of_two(int n) {
  unsigned int var = n;  // compute the next highest power of 2 of 32-bit v

  var--;
  var |= var >> 1;
  var |= var >> 2;
  var |= var >> 4;
  var |= var >> 8;
  var |= var >> 16;
  var++;

  return var;
}

// -------------------------------- Image ---------------------------------

Image::~Image() {
  if (ilid_ == 0) {
    return;
  }

  ilDeleteImage(ilid_);
}

// Clone
std::shared_ptr<Image> Image::clone() const {
  auto clone = std::make_shared<Image>();

  if (has_error()) {
    clone->has_error_ = true;
    clone->error_ = error_;
    return clone;
  }

  if (!clone->create(width(), height(), channels())) {
    return clone;
  }

  if (ilCopyImage(ilid_) != IL_TRUE) {
    return clone;
  }

  clone->path_ = path_;
  clone->name_ = name_;

  return clone;
}

bool Image::load(const std::vector<std::uint8_t>& par_buffer) {
  return load_from_memory_(par_buffer);
}

bool Image::load(const std::string& par_file) {
  FILE* fp = fopen(par_file.c_str(), "rb");
  if (fp == nullptr) {
    has_error_ = true;
    error_ = "Failed to open file '" + par_file + "'";
    return false;
  }

  fseek(fp, 0, SEEK_END);
  int const len = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  auto buf = std::vector<std::uint8_t>(len);
  fread(buf.data(), len, 1, fp);
  fclose(fp);

  path_ = par_file;

  return load_from_memory_(buf);
}

bool Image::create(int par_width, int par_height, int par_channels) {
  ilGenImages(1, &ilid_);
  ilBindImage(ilid_);
  ilClearColor(0.0F, 0.0F, 0.0F, 1.0F);
  // iluEnlargeCanvas(par_width, par_height, 1);

  if (ilTexImage(par_width, par_height, 1, par_channels, par_channels == 3 ? IL_RGB : IL_RGBA,
                 IL_UNSIGNED_BYTE, nullptr) != IL_TRUE) {
    error_ = iluErrorString(ilGetError());
    has_error_ = true;
    return false;
  }

  // Read back image infos
  has_error_ = false;
  image_infos_();

  return true;
}

bool Image::create(int par_width, int par_height, float par_red, float par_green, float par_blue) {
  if (!create(par_width, par_height, 3)) {
    return false;
  }

  return clear_color(par_red, par_green, par_blue, 1.0F);
}

bool Image::save(const std::string& par_file) {
  if (has_error()) {
    return false;
  }

  ilBindImage(ilid_);
  ilEnable(IL_FILE_OVERWRITE);

  if (std::filesystem::path(par_file).extension() == ".dds") {
    ilSetInteger(IL_DXTC_FORMAT, IL_DXT5);
    ilEnable(IL_NVIDIA_COMPRESS);
  }

  if (ilSaveImage(static_cast<const ILstring>(par_file.c_str())) != IL_TRUE) {
    error_ = iluErrorString(ilGetError());
    has_error_ = true;
  }

  return true;
}

// trunk-ignore(clang-tidy/readability-make-member-function-const)
std::uint8_t* Image::data() {
  if (has_error()) {
    return nullptr;
  }

  ilBindImage(ilid_);
  return ilGetData();
}

bool Image::clear_color(float pRed, float pGreen, float pBlue, float pAlpha) {
  if (has_error()) {
    return false;
  }

  ilBindImage(ilid_);
  ilClearColour(pRed, pGreen, pBlue, pAlpha);
  if (ilClearImage() != IL_TRUE) {
    error_ = iluErrorString(ilGetError());
    has_error_ = true;
  }

  return true;
}

/*
Add/Remove alpha channel
*/
bool Image::add_alpha() {
  if (has_error() or has_alpha()) {
    return false;
  }

  ilBindImage(ilid_);
  if (ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE) != IL_TRUE) {
    error_ = iluErrorString(ilGetError());
    has_error_ = true;
    return false;
  }

  image_infos_();

  return true;
}

bool Image::threedo_to_s3o() {
  if (has_error()) {
    return false;
  }

  ilBindImage(ilid_);
  auto* data_ptr = ilGetData();
  auto** data_pptr = &data_ptr;

  for (std::size_t ph = 0; ph < height_; ph++) {
    for (std::size_t pw = 0; pw < width_; pw++) {
      // trunk-ignore(clang-tidy/cppcoreguidelines-pro-bounds-pointer-arithmetic)
      data_ptr[1] = data_ptr[0];

      // trunk-ignore(clang-tidy/cppcoreguidelines-pro-bounds-pointer-arithmetic)
      *data_pptr += bpp_;
    }
  }

  image_infos_();

  return true;
}

bool Image::to_power_of_two() {
  if (has_error()) {
    return false;
  }

  ilBindImage(ilid_);

  owidth_ = width_;
  if (!is_power_of_two(owidth_)) {
    width_ = next_power_of_two(owidth_);
    spdlog::debug("Enlarging '{}' cause the width '{}' is not power of two '{}'.", name_, owidth_,
                  width_);
  }

  oheight_ = height_;
  if (!is_power_of_two(oheight_)) {
    height_ = next_power_of_two(oheight_);
    spdlog::debug("Enlarging '{}' cause the height '{}' is not power of two '{}'.", name_, oheight_,
                  height_);
  }

  if (width_ != owidth_ || height_ != oheight_) {
    if (ilConvertImage(has_alpha() ? IL_RGBA : IL_RGB, IL_UNSIGNED_BYTE) != IL_TRUE) {
      has_error_ = true;
      error_ = iluErrorString(ilGetError());
      return false;
    }

    ilClearColor(0.0F, 0.0F, 0.0F, 0.0F);
    iluImageParameter(ILU_PLACEMENT, ILU_UPPER_LEFT);
    iluEnlargeCanvas(width_, height_, 1);

    image_infos_();
  }

  auto* data_ptr = ilGetData();

  std::size_t num_pixels = static_cast<std::size_t>(width_) * height_;

  // Copy to bottom
  if (height_ > oheight_) {
    for (int ih = 0; ih < height_ - oheight_; ih++) {
      for (int iw = 0; iw <= owidth_; iw++) {
        const int src = (height_ - oheight_ - ih + height_ - oheight_) * width_ + iw;
        const int dst = ih * width_ + iw;

        if (dst > num_pixels) {
          spdlog::warn("'{}' - 'row' can't write to destination: '{}', number of pixels is '{}'",
                       name_, dst, num_pixels);
          continue;
        }

        // trunk-ignore(clang-tidy/cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::memcpy(data_ptr + static_cast<ptrdiff_t>(dst * bpp_),
                    data_ptr + static_cast<ptrdiff_t>(src * bpp_), bpp_);
      }
    }
  }

  // Copy to the right side
  if (width_ > owidth_) {
    for (int ih = height_ - 1; ih >= 0; ih--) {
      for (int iw = 0; iw < (width_ - owidth_); iw++) {
        const int src = ih * width_ + owidth_ - 1 - iw;
        const int dst = ih * width_ + owidth_ + iw;

        if (dst > num_pixels) {
          spdlog::warn("'{}' - 'column' can't write to destination: '{}', number of pixels is '{}'",
                       name_, dst, num_pixels);
          continue;
        }

        // trunk-ignore(clang-tidy/cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::memcpy(data_ptr + static_cast<ptrdiff_t>(dst * bpp_),
                    data_ptr + static_cast<ptrdiff_t>(src * bpp_), bpp_);
      }
    }
  }

  // Update the image information
  image_infos_();

  return true;
}

bool Image::add_opaque_alpha() {
  if (has_error()) {
    return false;
  }

  ilBindImage(ilid_);
  if (ilSetAlpha(0.0F) != IL_TRUE) {
    has_error_ = true;
    error_ = iluErrorString(ilGetError());
    return false;
  }

  image_infos_();

  return true;
}

bool Image::mirror() {
  if (has_error()) {
    return false;
  }

  ilBindImage(ilid_);
  if (iluMirror() != IL_TRUE) {
    error_ = iluErrorString(ilGetError());
    has_error_ = true;
  }

  return true;
}

bool Image::flip() {
  if (has_error()) {
    return false;
  }

  ilBindImage(ilid_);
  if (iluFlipImage() != IL_TRUE) {
    error_ = iluErrorString(ilGetError());
    has_error_ = true;
  }

  return true;
}

/*
This function is not intended to actually draw things (it doesn't do any clipping),
it is just a way to copy certain parts of an image.
*/
bool Image::blit(const std::shared_ptr<Image> par_src, int par_dx, int par_dy, int par_dz,
                 int par_sx, int par_sy, int par_sz, int par_width, int par_height, int par_depth) {
  if (has_error() or par_src->has_error()) {
    error_ = "blit, either the destination or the source has an error";
    return false;
  }

  if (par_src->bpp() < 3) {
    error_ = "blit, bytesperpixel on source < 3";
    return false;
  }

  ilBindImage(ilid_);

  ilDisable(IL_BLIT_BLEND);
  if (ilBlit(par_src->id(), par_dx, par_dy, par_dz, par_sx, par_sy, par_sz, par_width, par_height,
             par_depth) != IL_TRUE) {
    ilEnable(IL_BLIT_BLEND);
    error_ = iluErrorString(ilGetError());
    has_error_ = true;
    return false;
  }
  ilEnable(IL_BLIT_BLEND);

  image_infos_();

  return true;
}

bool Image::load_from_memory_(const std::vector<std::uint8_t>& par_buffer) {
  if (ilid_ != 0) {
    ilDeleteImage(ilid_);
  }

  ilGenImages(1, &ilid_);
  ilBindImage(ilid_);

  // /* Convert paletted to packed colors */
  ilEnable(IL_CONV_PAL);
  ilHint(IL_MEM_SPEED_HINT, IL_FASTEST);

  // GL like origin.
  // ilOriginFunc(IL_ORIGIN_UPPER_LEFT);
  // ilEnable(IL_ORIGIN_SET);

  if (ilLoadL(IL_TYPE_UNKNOWN, par_buffer.data(), par_buffer.size()) != IL_TRUE) {
    has_error_ = true;
    error_ = std::string(iluErrorString(ilGetError()));
    spdlog::error("Failed to read image '{}', error was: {}", path_, error_);

    ilDeleteImage(ilid_);
    ilid_ = 0;

    return false;
  }

  has_error_ = false;
  image_infos_();

  return true;
}

void Image::image_infos_() {
  if (has_error()) {
    return;
  }

  ilBindImage(ilid_);
  ilGetIntegerv(IL_IMAGE_WIDTH, &width_);
  ilGetIntegerv(IL_IMAGE_HEIGHT, &height_);
  ilGetIntegerv(IL_IMAGE_BYTES_PER_PIXEL, &bpp_);
  ilGetIntegerv(IL_IMAGE_DEPTH, &deepth_);
  ilGetIntegerv(IL_IMAGE_CHANNELS, &channels_);
}
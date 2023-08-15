#pragma once

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include "../Image.h"
#include "../Texture.h"

#include <TXPK/Core/IPacker.hpp>

#define ATLAS_MARGIN 2

struct atlas_info_image {
  atlas_info_image() {};
  atlas_info_image(std::string& par_name, std::uint32_t par_width, std::uint32_t par_height,
                   std::uint32_t par_orig_width, std::uint32_t par_orig_height,
                   std::uint32_t par_left, std::uint32_t par_top)
      : name(par_name), width(par_width), height(par_height), orig_width(par_orig_width), orig_height(par_orig_height), left(par_left), top(par_top) {}

  std::string name;
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t orig_width;
  std::uint32_t orig_height;
  std::uint32_t left;
  std::uint32_t top;

  inline float U(std::uint32_t par_width, float par_u) {
    float result = par_u * float(orig_width) / float(par_width) + (float(left) + float(ATLAS_MARGIN) + 0.5F) / float(par_width);
    if (result > 1.0f) {
      return 1.0f;
    }
    // if (result < 0.0f) {
    //   return 0.0f;
    // }
    return result;
  }

  inline float V(std::uint32_t par_height, float par_v) {
    float result = par_v * float(orig_height) / float(par_height) + (float(top) + float(ATLAS_MARGIN) + 0.5F) / float(par_height);
    if (result > 1.0f) {
      return 1.0f;
    }
    // if (result < 0.0f) {
    //   return 0.0f;
    // }
    return result;
  }
};

struct atlas_info {
  std::string color_image;
  std::string other_image;
  std::string normal_image;
  std::uint32_t width;
  std::uint32_t height;
  std::vector<atlas_info_image> textures;

  static std::optional<atlas_info> load(const std::string& par_path);
  bool save(const std::string& par_path);
};

class atlas {
 public:
  atlas();
  ~atlas() = default;

  atlas(const atlas& rhs) {
    packer_ = rhs.packer_;
    textures_ = rhs.textures_;
    bin_ = rhs.bin_;
  }

  bool load_yaml(const std::string& par_path);

  static atlas make_from_archive(const std::string& par_archive, const std::string& par_savepath,
                                 bool par_power_of_two);

  bool add_3do_textures(std::vector<ImagePtr> par_images, bool par_power_of_two);
  bool add_textures(std::vector<ImagePtr> par_images);

  bool pack();
  void info(atlas_info& par_info);
  const atlas_info& info() const;
  bool save(const std::string& par_savepath);

 private:
  std::shared_ptr<txpk::IPacker> packer_;
  txpk::TexturePtrs textures_;

  atlas_info info_;
  txpk::Bin bin_;
  bool packed_;
};
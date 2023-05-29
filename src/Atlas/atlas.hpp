#pragma once

#include <vector>
#include <string>
#include <memory>
#include "../Image.h"

#include <TXPK/Core/IPacker.hpp>

class atlas {
 public:
  atlas();
  ~atlas() = default;

  atlas(const atlas& rhs) {
    packer_ = rhs.packer_;
    textures_ = rhs.textures_;
    rectangles_ = rhs.rectangles_;
  }

  static atlas make_from_archive(const std::string& par_archive, const std::string& par_savepath,
                                 bool par_power_of_two);

  bool add_3do_textures(std::vector<ImagePtr> par_images, bool par_power_of_two);
  bool add_textures(std::vector<ImagePtr> par_images);

  bool save(const std::string& par_savepath);

 private:
  std::shared_ptr<txpk::IPacker> packer_;
  txpk::TexturePtrs textures_;
  txpk::RectanglePtrs rectangles_;
};
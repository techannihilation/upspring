#include "atlas.hpp"

#include "../Texture.h"
#include "spdlog/spdlog.h"

#include "TXPK/Core/Texture.hpp"
#include "TXPK/Core/Rectangle.hpp"
#include "TXPK/Packers/BlackspawnPacker.hpp"

atlas::atlas() : packer_(std::make_shared<txpk::BlackspawnPacker>()) {}

atlas atlas::make_from_archive(const std::string& par_archive, const std::string& par_savepath, bool par_power_of_two) {
  TextureHandler texture_handler = TextureHandler();

  auto d3doFilter = [](const std::string& pName) -> bool {
    return pName.rfind("unittextures/tatex/", 0) == 0;
  };

  if (!texture_handler.LoadFiltered(par_archive, [](const std::string& par_path) -> std::string {
        if (par_path.rfind("unittextures/tatex/", 0) == 0) {
          auto result = std::filesystem::path(par_path).replace_extension("").string();
          result = result.substr(std::string("unittextures/tatex/").length());

          return result;
        }
        return "";
      })) {
    spdlog::error("Failed to load archive {}", par_archive);
    return atlas();
  }

  std::vector<ImagePtr> images;
  for (const auto& mapE : texture_handler.textures()) {
    if (mapE.second->image->channels() < 3) {
      spdlog::warn("Skippping: '{}'", mapE.first);
      continue;
    }

    spdlog::debug("Loading texture: {}", mapE.first);

    ImagePtr img = mapE.second->image;

    // On teamcolor we copy the red channel to green, on normal textures we add a opaque alpha.
    if (texture_handler.has_team_color(img->name())) {
      if (!img->threedo_to_s3o()) {
        spdlog::error("image->threedo_to_s3o failed, error was: {}", img->error());
        continue;
      }
    } else if (!img->add_opaque_alpha()) {
      spdlog::error("image->add_opaque_alpha failed, error was: {}", img->error());
      continue;
    }

    if (par_power_of_two) {
      if (!img->to_power_of_two()) {
        spdlog::error("Power of two failed");
        continue;
      }
    }

    images.push_back(img);
  }

  auto a = atlas();
  a.add_images(images);

  return a;
}

bool atlas::add_images(std::vector<ImagePtr> par_images) {
  for (const auto& img : par_images) {
    auto txTexture = std::make_shared<txpk::Texture>();

    if (!txTexture->loadFromIL(img->id())) {
      continue;
    }

    textures_.push_back(txTexture);
  }

  rectangles_.reserve(textures_.size());
  for (auto& txTexture : textures_) {
    rectangles_.push_back(txTexture->getBounds());
  }

  if (!packer_->validate(rectangles_)) {
    spdlog::error("TXPK packer failed to validate.");
    return false;
  }

  return true;
}

bool atlas::save(const std::string& par_savepath) {
  txpk::Color<4U> black;
  auto txBin = packer_->pack(rectangles_, 0, txpk::SizeContraintType::None, false);
  if (!txBin.save(textures_, black, par_savepath, false)) {
    return false;
  }

  return true;
}
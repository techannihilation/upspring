#include "atlas.hpp"

#include <iostream>
#include <fstream>

#include "../Texture.h"
#include "../string_util.h"

#include "yaml-cpp/yaml.h"
#include "spdlog/spdlog.h"

#include "TXPK/Core/Texture.hpp"
#include "TXPK/Core/Rectangle.hpp"
#include "TXPK/Packers/BlackspawnPacker.hpp"

atlas::atlas() : packer_(std::make_shared<txpk::BlackspawnPacker>()) {}

bool atlas::load_yaml(const std::string& par_path) {
  auto path = std::filesystem::path(par_path).replace_extension(".yaml").string();

  auto ainfoOpt = atlas_info::load(path);
  if (!ainfoOpt) {
    return false;
  }

  info(ainfoOpt.value());

  return true;
}

atlas atlas::make_from_archive(const std::string& par_archive, const std::string& /*par_savepath*/,
                               bool par_power_of_two) {
  TextureHandler texture_handler = TextureHandler();

  if (!texture_handler.LoadFiltered(par_archive, [](const std::string& par_path) -> std::string {
        if (par_path.rfind("unittextures/tatex/", 0) == 0) {
          return std::filesystem::path(par_path).replace_extension("").string().substr(
              std::string("unittextures/tatex/").length());
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

    images.push_back(mapE.second->image);
  }

  auto atl = atlas();
  atl.add_3do_textures(images, par_power_of_two);

  return atl;
}

bool atlas::add_3do_textures(std::vector<ImagePtr> par_images, bool par_power_of_two) {
  std::vector<ImagePtr> add;
  for (const auto& img : par_images) {
    if (auto it_textures =
            std::find_if(textures_.begin(), textures_.end(),
                         [img](txpk::TexturePtr par_tex) { return par_tex->name == img->name(); });
        it_textures != std::end(textures_)) {
      continue;
    }

    if (par_power_of_two) {
      if (!img->to_power_of_two()) {
        spdlog::error("Power of two failed");
        continue;
      }
    }

    // On teamcolor we copy the red channel to green, on normal textures we add a opaque alpha.
    if (img->is_team_color()) {
      if (!img->threedo_to_s3o()) {
        spdlog::error("image->threedo_to_s3o failed, error was: {}", img->error());
        continue;
      }
    } else if (!img->add_opaque_alpha()) {
      spdlog::error("image->add_opaque_alpha failed, error was: {}", img->error());
      continue;
    }

    auto marginImage = std::make_shared<Image>();
    
    // Copy attributes.
    marginImage->name(img->name());
    marginImage->owidth(img->width());
    marginImage->oheight(img->height());

    marginImage->create(img->width() + (ATLAS_MARGIN * 2), img->height() + (ATLAS_MARGIN*2), 4);
    marginImage->clear_color(0.0f, 0.0f, 0.0f, 0.0f);
  
    if (!marginImage->blit(img, ATLAS_MARGIN, ATLAS_MARGIN, 0, 0, 0, 0, img->width(), img->height(), 1)) {
      spdlog::error("image->blit failed, error was: {}", img->error());
      continue;
    }

    add.push_back(marginImage);
  }

  add_textures(add);

  return true;
}

bool atlas::add_textures(std::vector<ImagePtr> par_images) {
  for (const auto& img : par_images) {
    auto txTexture = std::make_shared<txpk::Texture>();

    if (!txTexture->loadFromIL(img->id())) {
      continue;
    }

    txTexture->name = img->name();
    txTexture->orig_width = img->owidth();
    txTexture->orig_height = img->oheight();

    textures_.push_back(txTexture);
  }

  return true;
}

bool atlas::pack() {
  txpk::RectanglePtrs rectangles;
  rectangles.reserve(textures_.size());
  for (auto& txTexture : textures_) {
    rectangles.push_back(txTexture->getBounds());
  }

  if (!packer_->validate(rectangles)) {
    spdlog::error("TXPK packer failed to validate.");
    return false;
  }


  spdlog::debug("found '{}' rectangles", rectangles.size());
  bin_ = packer_->pack(rectangles, 0, txpk::SizeContraintType::None, false);

  std::vector<atlas_info_image> info_textures;

  for (const auto& txTexture : textures_) {
    txpk::RectanglePtr const bounds = txTexture->getBounds();

    info_textures.emplace_back(txTexture->name, bounds->width, bounds->height,
                               txTexture->orig_width, txTexture->orig_height, bounds->left,
                               bounds->top);
  }

  info_.width = bin_.width;
  info_.height = bin_.height;
  info_.textures = info_textures;

  packed_ = true;
  return true;
}

void atlas::info(atlas_info& par_info) {
  info_ = par_info;
}

const atlas_info& atlas::info() const {
  return info_;
}

bool atlas::save(const std::string& par_savepath) {
  if (!packed_ && !pack()) {
    return false;
  }

  const txpk::Color<4U> black{};
  std::filesystem::path yaml_path(par_savepath);
  auto color_path = (yaml_path.parent_path() / (yaml_path.stem().string() + "_tex1.dds")).string();
  bool const result = bin_.save(textures_, black, color_path, false);
  if (!result) {
    return result;
  }

  info_.color_image = color_path;

  // Generate fake other.dds
  Image other_image;
  if (!other_image.create(bin_.width, bin_.height, 4)) {
    spdlog::error("other.dds create failed: %s", other_image.error());
    return false;
  }
  other_image.clear_color(0.0f / 255.0f, 129.0f / 255.0f, 79.0f / 255.0f, 1.0f);
  auto other_save_path = (yaml_path.parent_path() / (yaml_path.stem().string() + "_tex2.dds")).string();
  if (!other_image.save(other_save_path)) {
    spdlog::error("normals.dds save failed: %s", other_image.error());
    return false;
  }
  info_.other_image = other_save_path;

  // Generate fake normals.dds
  Image normal_image;
  if (!normal_image.create(bin_.width, bin_.height, 4)) {
    spdlog::error("normals.dds create failed: %s", normal_image.error());
    return false;
  } 
  normal_image.clear_color(128.0f / 255.0f, 129.0f / 255.0f, 255.0f / 255.0f, 1.0f);
  auto normal_save_path = (yaml_path.parent_path() / (yaml_path.stem().string() + "_normals.dds")).string();
  if (!normal_image.save(normal_save_path)) {
    spdlog::error("normals.dds save failed: %s", normal_image.error());
    return false;
  }
  info_.normal_image = normal_save_path;

  // Save yaml.
  if (!info_.save(yaml_path.string())) {
    spdlog::error("Failed to save the yaml file");
  }

  return result;
}

std::optional<atlas_info> atlas_info::load(const std::string& par_path) {
  YAML::Node config;
  try {
    config = YAML::LoadFile(par_path);
    return config.as<atlas_info>();
  } catch (const std::exception& err) {
    spdlog::error("Failed to load '{}', error was: '{}'", par_path, err.what());
    return std::nullopt;
  }
}

bool atlas_info::save(const std::string& par_path) {
  YAML::Emitter emitter;
  emitter << YAML::Node(*this);

  std::ofstream yamlFile;
  yamlFile.open(par_path, std::ios::out | std::ios::trunc);
  if (!yamlFile.is_open()) {
    return false;
  }

  yamlFile << emitter.c_str();
  yamlFile.close();

  return true;
}

namespace YAML {
template <>
struct convert<atlas_info> {
  static Node encode(const atlas_info& rhs) {
    Node node;
    node["color_image"] = rhs.color_image;
    node["other_image"] = rhs.other_image;
    node["normal_image"] = rhs.normal_image;
    node["width"] = rhs.width;
    node["height"] = rhs.height;
    node["textures"] = rhs.textures;
    return node;
  }

  static bool decode(const Node& node, atlas_info& rhs) {
    if (!node.IsMap()) {
      return false;
    }

    if (node["color_image"]) {
      rhs.color_image = node["color_image"].as<std::string>();
    }
    if (node["other_image"]) {
      rhs.other_image = node["other_image"].as<std::string>();
    }
    if (node["normal_image"]) {
      rhs.normal_image = node["normal_image"].as<std::string>();
    }
    if (node["width"]) {
      rhs.width = node["width"].as<std::uint32_t>();
    }
    if (node["height"]) {
      rhs.height = node["height"].as<std::uint32_t>();
    }
    if (node["textures"]) {
      rhs.textures = node["textures"].as<std::vector<atlas_info_image>>();
    }

    return true;
  }
};

template <>
struct convert<atlas_info_image> {
  static Node encode(const atlas_info_image& rhs) {
    Node node;
    node["name"] = rhs.name;
    node["width"] = rhs.width;
    node["height"] = rhs.height;
    node["orig_width"] = rhs.orig_width;
    node["orig_height"] = rhs.orig_height;
    node["left"] = rhs.left;
    node["top"] = rhs.top;
    return node;
  }

  static bool decode(const Node& node, atlas_info_image& rhs) {
    if (!node.IsMap()) {
      return false;
    }

    if (node["name"]) {
      rhs.name = node["name"].as<std::string>();
    }

    if (node["width"]) {
      rhs.width = node["width"].as<std::uint32_t>();
    }
    if (node["height"]) {
      rhs.height = node["height"].as<std::uint32_t>();
    }
    if (node["orig_width"]) {
      rhs.orig_width = node["orig_width"].as<std::uint32_t>();
    }
    if (node["orig_height"]) {
      rhs.orig_height = node["orig_height"].as<std::uint32_t>();
    }
    if (node["left"]) {
      rhs.left = node["left"].as<std::uint32_t>();
    }
    if (node["top"]) {
      rhs.top = node["top"].as<std::uint32_t>();
    }

    return true;
  }
};

}  // namespace YAML
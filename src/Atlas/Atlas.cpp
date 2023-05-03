#include "Atlas.hpp"

#include "../Texture.h"
#include "spdlog/spdlog.h"

#include "TXPK/Core/Texture.hpp"
#include "TXPK/Core/Rectangle.hpp"
#include "TXPK/Packers/BlackspawnPacker.hpp"

Atlas::Atlas() : mTxPacker(std::make_unique<txpk::BlackspawnPacker>()) {}

Atlas::Atlas(const std::vector<Image>& pImages) : Atlas() {
  
}

Atlas Atlas::make_from_archive(const std::string& archive_par, const std::string& savepath_par) {
  TextureHandler textureHandler = TextureHandler();

  auto d3doFilter = [](const std::string& pName) -> bool {
    return pName.rfind("unittextures/tatex/", 0) == 0;
  };

  if (!textureHandler.LoadFiltered(archive_par, [](const std::string& par_path) -> std::string {
      if (par_path.rfind("unittextures/tatex/", 0) == 0) {
        auto result = std::filesystem::path(par_path).replace_extension("").string();
        result = result.substr(std::string("unittextures/tatex/").length());

        return result;
      }
      return "";
    })) {
    
    spdlog::error("Failed to load archive {}", archive_par);
    return Atlas();
  }

  txpk::TexturePtrs txTextures;
  for (const auto& mapE : textureHandler.textures()) {
    auto txTexture = std::make_shared<txpk::Texture>();

    spdlog::debug("Loading texture: {}", mapE.first);

    if (mapE.second->image->channels() < 3) {
      continue;
    }

    if (!txTexture->loadFromIL(mapE.second->image->id())) {
      continue;
    }

    txTextures.push_back(txTexture);
  }

  txpk::RectanglePtrs txRectangles;
  txRectangles.reserve(txTextures.size());
  for (auto& txTexture : txTextures) {
    txRectangles.push_back(txTexture->getBounds());
  }

  auto txPacker = std::make_unique<txpk::BlackspawnPacker>();

  // `sizeConstraint`: constrains the size of either width or height of the packed image.
  // `constraintType`: `None=0`, `Width=1`, `Height=2`. The `sizeConstraint` will be ignored if the
  // type is `None`.
  if (!txPacker->validate(txRectangles)) {
    spdlog::error("TXPK packer failed to validate.");
    return Atlas();
  }

  txpk::Color<4U> black;
  auto txBin = txPacker->pack(txRectangles, 0, txpk::SizeContraintType::None, false);
  if (!txBin.save(txTextures, black, savepath_par, false)) {
    return Atlas();
  }

  return Atlas();
}
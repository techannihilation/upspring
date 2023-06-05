//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------
#pragma once

#include <memory>
#include <set>
#include <unordered_map>
#include <functional>
#include <filesystem>

#include "FileSystem/IArchive.h"

#include "Referenced.h"
#include "Image.h"

class CfgList;

class Texture {
 public:
  Texture();
  Texture(const std::string& filename);
  Texture(const std::string& filename, const std::string& hintpath);
  Texture(std::vector<std::uint8_t>& par_data, const std::string& par_path,
          const std::string& par_name, bool par_is_teamcolor);
  Texture(std::shared_ptr<Image> par_image, const std::string& par_name)
      : glIdent(), name(par_name) {
    image = par_image;
  };
  ~Texture();

  bool Load(const std::string& filename, const std::string& hintpath);
  bool HasError() const { return image == nullptr or image->has_error(); }
  bool VideoInit();

  std::shared_ptr<Image> GetImage() { return image; };
  void SetImage(std::shared_ptr<Image> img);

  inline int Width() const { return image->width(); }
  inline int Height() const { return image->height(); }

  std::uint32_t glIdent;
  std::string name;
  std::shared_ptr<Image> image;

  static std::string textureLoadDir;
};

// manages 3do textures
class TextureHandler {
  std::set<std::string> supported_extensions_ = {".bmp", ".jpg", ".tga", ".png", ".dds",
                                                 ".pcx", ".pic", ".gif", ".ico"};
  std::unordered_map<std::string, std::shared_ptr<Texture>> textures_;
  std::set<std::string> teamcolors_;

 public:
  TextureHandler() : textures_(), teamcolors_() {}
  virtual ~TextureHandler() = default;

  TextureHandler(const TextureHandler& rhs) = delete;
  TextureHandler(TextureHandler&& rhs) = delete;
  TextureHandler operator=(const TextureHandler& rhs) = delete;
  TextureHandler operator=(TextureHandler&& rhs) = delete;

  bool Load3DO(const std::string& par_archive_path) {
    return LoadFiltered(par_archive_path, [](const std::string& par_path) -> const std::string {
      if (par_path.rfind("unittextures/tatex/", 0) == 0) {
        auto result = std::filesystem::path(par_path).replace_extension("").string();
        result = result.substr(std::string("unittextures/tatex/").length());

        return result;
      }
      return "";
    });
  };
  bool LoadFiltered(const std::string& par_archive_path,
                    std::function<const std::string(const std::string&)>&& par_filter);
  std::shared_ptr<Texture> texture(const std::string& name);

  bool has_team_color(const std::string& texture_name);

 public:
  const std::unordered_map<std::string, std::shared_ptr<Texture>>& textures() { return textures_; }
};

class TextureGroup {
 public:
  std::string name;
  std::set<std::shared_ptr<Texture>> textures;
};

class TextureGroupHandler {
 public:
  TextureGroupHandler(std::shared_ptr<TextureHandler> par_handler);
  ~TextureGroupHandler();

  bool Load(const std::string& par_filename);
  bool Save(const std::string& par_filename);

  static CfgList* MakeConfig(TextureGroup* tg);
  TextureGroup* LoadGroup(CfgList* gc);

  std::vector<TextureGroup*> groups;
  std::shared_ptr<TextureHandler> textureHandler;
};

/*
Collects a set of small textures and creates a big one out of it
used for lightmap packing
*/
class TextureBinTree {
 public:
  struct Node {
    Node();
    Node(int X, int Y, int W, int H);
    ~Node();

    int x, y, w, h;
    int img_w, img_h;
    Node* child[2]{};
  };

  TextureBinTree() : render_id(), tree(), image_(std::make_shared<Image>()){};
  TextureBinTree(int par_width, int par_height);
  virtual ~TextureBinTree();

  Node* AddNode(const std::shared_ptr<Image> par_subtex);

  bool IsEmpty() { return !tree; }

  inline float GetU(Node* n, float u) {
    return u * (float)n->img_w / float(image_->width()) + (n->x + 0.5F) / image_->width();
  }
  inline float GetV(Node* n, float v) {
    return v * (float)n->img_h / float(image_->height()) + (n->y + 0.5F) / image_->height();
  }

  // Unused by class: uint for storing texture rendering id
  std::uint32_t render_id;

  std::shared_ptr<Image> GetResult() { return image_; }

 protected:
  void StoreNode(Node* n, const std::shared_ptr<Image> par_tex);
  Node* InsertNode(Node* n, int w, int h);

  Node* tree;
  std::shared_ptr<Image> image_;
};

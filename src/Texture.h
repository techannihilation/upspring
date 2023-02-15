//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------
#pragma once

#include <memory>
#include <unordered_set>
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
  Texture(std::vector<uchar>& par_data, const std::string& par_path, const std::string& par_name);
  Texture(std::shared_ptr<Image> par_image, const std::string& par_name) : glIdent(), name(par_name) {
    image = par_image;
  };
  static Texture LoadTexture(std::shared_ptr<IArchive>& pArchive, const uint& id,
                             const std::string& pName);
  ~Texture();

  bool Load(const std::string& filename, const std::string& hintpath);
  bool HasError() const { return image == nullptr or image->has_error(); }
  bool VideoInit();

  std::shared_ptr<Image> GetImage() { return image; };
  void SetImage(std::shared_ptr<Image> img);

  inline int Width() const { return image->width(); }
  inline int Height() const { return image->height(); }

  uint glIdent;
  std::string name;
  std::shared_ptr<Image> image;

  static std::string textureLoadDir;
};

// manages 3do textures
class TextureHandler {
 public:
  TextureHandler();
  ~TextureHandler();

  bool Load3DO(const std::string& pArchivePath) {
    return LoadFiltered(pArchivePath, [](const std::string& par_path) -> const std::string {
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
  std::shared_ptr<Texture> GetTexture(const std::string& name);

 protected:
  std::unordered_set<std::string> mSupportedExtensions = {".bmp", ".jpg", ".tga", ".png", ".dds",
                                                          ".pcx", ".pic", ".gif", ".ico"};

  std::unordered_map<std::string, std::shared_ptr<Texture>> textures;

 public:
  const std::unordered_map<std::string, std::shared_ptr<Texture>>& Textures() { return textures; }
};

class TextureGroup {
 public:
  std::string name;
  std::set<std::shared_ptr<Texture>> textures;
};

class TextureGroupHandler {
 public:
  TextureGroupHandler(TextureHandler* th);
  ~TextureGroupHandler();

  bool Load(const char* fname);
  bool Save(const char* fname);

  static CfgList* MakeConfig(TextureGroup* tg);
  TextureGroup* LoadGroup(CfgList* cfg);

  std::vector<TextureGroup*> groups;
  TextureHandler* textureHandler;
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
    Node* child[2];
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
  uint render_id;

  std::shared_ptr<Image> GetResult() { return image_; }

 protected:
  void StoreNode(Node* pm, const std::shared_ptr<Image> par_tex);
  Node* InsertNode(Node* n, int w, int h);

  Node* tree;
  std::shared_ptr<Image> image_;
};

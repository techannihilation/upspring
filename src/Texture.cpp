//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------
#include "EditorIncl.h"
#include "EditorDef.h"
#include "Texture.h"
#include "Util.h"
#include "Image.h"
#include "CfgParser.h"

#include "FileSystem/CSevenZipArchive.h"
#include "FileSystem/CZipArchive.h"

#include <IL/il.h>
#include <IL/ilu.h>
#include <IL/ilut.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include "string_util.h"
#include "spdlog/spdlog.h"

#include <filesystem>
#include <utility>

// ------------------------------------------------------------------------------------------------
// Texture
// ------------------------------------------------------------------------------------------------

std::string Texture::textureLoadDir;

Texture::Texture() : glIdent() {}

Texture::Texture(const std::string& par_filename) : glIdent()
{
  Load(par_filename, std::string());
}

Texture::Texture(const std::string& par_filename, const std::string& hintpath) : glIdent()
{
  Load(par_filename, hintpath);
}


Texture::Texture(std::vector<uchar>& par_data, const std::string& par_path, const std::string& par_name) : name(par_name), glIdent(0) {
  auto img = std::make_shared<Image>();
  img->path(par_path);
  img->name(par_name);
  if (!img->load(par_data)) {
    spdlog::error("Failed to create an image from '{}'", par_name);
    return;
  }
  image = img;
}

bool Texture::Load(const std::string& par_filename, const std::string& par_hintpath) {
  name = fltk::filename_name(par_filename.c_str());
  glIdent = 0;

  bool found = false;
  std::filesystem::path filename;
  if (!par_hintpath.empty()) {
    // Direct
    filename = std::filesystem::path(par_hintpath) / par_filename;
    if (std::filesystem::exists(filename)) {
      found = true;
    } else {
      std::filesystem::path my_hp(par_hintpath);
      for (int i = 0; i <= 3; ++i) {
        filename = my_hp / "unittextures" / par_filename;
        if (std::filesystem::exists(filename)) {
          found = true;
          break;
        }

        // Search on parent_path for "unittextures/<filename>".
        my_hp = my_hp.parent_path();
      }
    }
  }
  if (!found && !textureLoadDir.empty()) {
    filename = std::filesystem::path(textureLoadDir).append(par_filename);
    if (std::filesystem::exists(filename)) {
      found = true;
    }
  }

  if (!found) {
    return false;
  }

  auto img = std::make_shared<Image>();
  spdlog::debug("Loading {}", filename.string());
  if (!img->load(filename)) {
    return false;
  }

  image = img;
  return true;
}

void Texture::SetImage(std::shared_ptr<Image> img) { image = img; }

Texture::~Texture() {
  if (glIdent != 0) {
    glDeleteTextures(1, &glIdent);
    glIdent = 0;
  }
}

bool Texture::VideoInit() {
  if (image == nullptr or image->has_error()) {
    return false;
  }

  glGenTextures(1, &glIdent);
  glBindTexture(GL_TEXTURE_2D, glIdent);

  GLint const format = image->has_alpha() ? GL_RGBA : GL_RGB;

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  gluBuild2DMipmaps(GL_TEXTURE_2D, format, image->width(), image->height(), format, GL_UNSIGNED_BYTE, image->data());
  return true;
}

// ------------------------------------------------------------------------------------------------
// TextureHandler
// ------------------------------------------------------------------------------------------------

std::shared_ptr<Texture> TextureHandler::texture(const std::string& name) {
  std::string tmp = to_lower(name);

  auto ti_it = textures_.find(tmp);
  if (ti_it == textures_.end()) {
    tmp += "00";
    ti_it = textures_.find(tmp);
    if (ti_it == textures_.end()) {
      spdlog::warn("Texture '{}' not found", tmp);
      return nullptr;
    }
    return ti_it->second;
  }

  return ti_it->second;
}

bool TextureHandler::has_team_color(const std::string& texture_name) {
  auto tmp = to_lower(texture_name);

  // Trim 00 suffix if needed.
  if (tmp.substr(tmp.length() - 2) == "00") {
    tmp = tmp.substr(0, tmp.length() - 2);
  }

  return teamcolors_.find(tmp) != teamcolors_.end();
}

bool TextureHandler::LoadFiltered(const std::string& par_archive_path,
                                  std::function<const std::string(const std::string&)>&& par_filter) {
  auto ext = std::filesystem::path(par_archive_path).extension();

  std::shared_ptr<IArchive> archive;
  if (ext == ".7z" or ext == ".sd7") {
    archive = std::make_shared<CSevenZipArchive>(par_archive_path);
  } else if (ext == ".zip") {
    archive = std::make_shared<CZipArchive>(par_archive_path);
  } else {
    spdlog::error("Unknown archive '{}'", par_archive_path);
  }

  if (archive->NumFiles() == 0) {
    return false;
  }

  for (uint i = 0; i < archive->NumFiles(); i++) {
    std::string name;
    int size = 0;
    int mode = 0;

    archive->FileInfo(i, name, size, mode);

    if (size < 1) {
      continue;
    }

    // Extension checking
    auto myExt = std::filesystem::path(name).extension();
    if (myExt.empty()) {
      continue;
    }
    if (supported_extensions_.find(myExt) == supported_extensions_.end()) {
      // Not a supported file.
      continue;
    }

    // Apply filter
    auto internal_name = par_filter(name);
    if (internal_name.empty()) {
      continue;
    }

    if (textures_.find(internal_name) != textures_.end()) {
      spdlog::debug("Skipping texture '{}' its already known", internal_name);
      continue;
    }

    // Load from archive
    std::vector<std::uint8_t> buf;
    archive->GetFile(i, buf);

    if (buf.empty()) {
      spdlog::debug("Failed to read texture file '{}' from the archive", internal_name);
      continue;
    }

    auto tex = std::make_shared<Texture>(buf, name, internal_name);
    if (tex->HasError()) {
      continue;
    }

    // Assign to map
    textures_[tex->name] = tex;
  }

  // Read teamcolors.
  if (archive->FileExists("unittextures/tatex/teamtex.txt")) {
    std::vector<std::uint8_t> buff;
    archive->GetFileByName("unittextures/tatex/teamtex.txt", buff);

    std::stringstream stringstream(std::string((char *)buff.data(), buff.size()));

    // teamcolors_.reserve(32);
    std::string line;
    while (std::getline(stringstream, line)) {
      line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
      line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
      teamcolors_.insert(to_lower(line));
    }
  }

  spdlog::debug("Loaded '{}' textures and '{}' teamcolors", textures_.size(), teamcolors_.size());

  return true;
}

// ------------------------------------------------------------------------------------------------
// TextureGroupHandler
// ------------------------------------------------------------------------------------------------

TextureGroupHandler::TextureGroupHandler(std::shared_ptr<TextureHandler> par_handler) : textureHandler(par_handler) {}

TextureGroupHandler::~TextureGroupHandler() {
  for (auto& group : groups) {
    delete group;
  }
  groups.clear();
}

bool TextureGroupHandler::Load(const char* fn) {
  CfgList* cfg = CfgValue::LoadFile(fn);

  if (cfg == nullptr) {
    return false;
  }

  for (auto& child : cfg->childs) {
    auto* gc = dynamic_cast<CfgList*>(child.value);
    if (gc == nullptr) {
      continue;
    }

    LoadGroup(gc);
  }

  delete cfg;
  return true;
}

bool TextureGroupHandler::Save(const char* fn) {
  // open a cfg writer
  CfgWriter writer(fn);
  if (writer.IsFailed()) {
    fltk::message("Unable to save texture groups to %s\n", fn);
    return false;
  }

  // create a config list and save it
  CfgList cfg;

  for (uint a = 0; a < groups.size(); a++) {
    CfgList* gc = MakeConfig(groups[a]);

    char n[10];
    sprintf(n, "group%d", a);
    cfg.AddValue(n, gc);
  }

  cfg.Write(writer, true);
  return true;
}

TextureGroup* TextureGroupHandler::LoadGroup(CfgList* gc) {
  auto* texlist = dynamic_cast<CfgList*>(gc->GetValue("textures"));
  if (texlist == nullptr) {
    return nullptr;
  }

  auto* texGroup = new TextureGroup;
  texGroup->name = gc->GetLiteral("name", "unnamed");

  for (auto& child : texlist->childs) {
    auto* l = dynamic_cast<CfgLiteral*>(child.value);
    if ((l != nullptr) && !l->value.empty()) {
      auto texture = textureHandler->texture(l->value);
      if (texture != nullptr) {
        texGroup->textures.emplace(texture);
      } else {
        spdlog::debug("Discarded texture name: {}", l->value);
      }
    }
  }
  groups.push_back(texGroup);
  return texGroup;
}

CfgList* TextureGroupHandler::MakeConfig(TextureGroup* tg) {
  auto* gc = new CfgList;
  char n[10];

  auto* texlist = new CfgList;
  int index = 0;
  for (auto texture : tg->textures) {
    sprintf(n, "tex%d", index++);
    texlist->AddLiteral(n, texture->name.c_str());
  }
  gc->AddValue("textures", texlist);
  gc->AddLiteral("name", tg->name.c_str());
  return gc;
}

// ------------------------------------------------------------------------------------------------
// Texture storage class based on a binary tree
// ------------------------------------------------------------------------------------------------

TextureBinTree::Node::Node() {
  x = y = w = h = 0;
  img_w = img_h = 0;
  child[0] = child[1] = 0;
}

TextureBinTree::Node::Node(int X, int Y, int W, int H) {
  x = X;
  y = Y;
  w = W;
  h = H;
  img_w = img_h = 0;
  child[0] = child[1] = 0;
}

TextureBinTree::Node::~Node() {
  SAFE_DELETE(child[0]);
  SAFE_DELETE(child[1]);
}

TextureBinTree::TextureBinTree(int par_width, int par_height) : TextureBinTree() {
  if (!image_->create(par_width, par_height)) {
    spdlog::error("Failed to create the atlas image with size '{}/{}', error was: {}", par_width, par_height, image_->error());
  }
};

TextureBinTree::~TextureBinTree() { SAFE_DELETE(tree); }

void TextureBinTree::StoreNode(Node* n, const std::shared_ptr<Image> par_tex) {
  n->img_w = par_tex->width();
  n->img_h = par_tex->height();

  if (!image_->blit(par_tex, n->x, n->y, 0, 0, 0, 0, par_tex->width(), par_tex->height(), 1)) {
    spdlog::error("TextureBinTree blit failed {}/{}, error was: {}", n->x, n->y, image_->error());
    return;
  }
}

TextureBinTree::Node* TextureBinTree::InsertNode(Node* n, int w, int h) {
  if (n->child[0] || n->child[1])  // not a leaf node ?
  {
    Node* r = 0;
    if (n->child[0]) r = InsertNode(n->child[0], w, h);
    if (r) return r;
    if (n->child[1]) r = InsertNode(n->child[1], w, h);
    return r;
  } else {
    // Occupied
    if (n->img_w) return 0;

    // Does it fit ?
    if (n->w < w || n->h < h) return 0;

    if (n->w == w && n->h == h) return n;

    int ow = n->w - w, oh = n->h - h;
    if (ow > oh) {
      // Split vertically
      if (ow) n->child[0] = new Node(n->x + w, n->y, ow, n->h);
      if (oh) n->child[1] = new Node(n->x, n->y + h, w, oh);
    } else {
      // Split horizontally
      if (ow) n->child[0] = new Node(n->x + w, n->y, ow, h);
      if (oh) n->child[1] = new Node(n->x, n->y + h, n->w, oh);
    }

    return n;
  }

  return 0;
}

TextureBinTree::Node* TextureBinTree::AddNode(const std::shared_ptr<Image> par_subtex) {
  if (tree == nullptr) {
    // create root node
    if (par_subtex->width() > image_->width() || par_subtex->height() > image_->height()) {
      return nullptr;
    }

    tree = new Node(0, 0, image_->width(), image_->height());
  }

  auto *pn = InsertNode(tree, par_subtex->width(), par_subtex->height());
  if (pn == nullptr) {
    return nullptr;
  }

  StoreNode(pn, par_subtex);
  return pn;
}
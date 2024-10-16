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

#include "FileSystem/CDirectoryArchive.h"
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

Texture::Texture(const std::string& par_filename) : glIdent() { Load(par_filename, std::string()); }

Texture::Texture(const std::string& par_filename, const std::string& hintpath) : glIdent() {
  Load(par_filename, hintpath);
}

Texture::Texture(std::vector<std::uint8_t>& par_data, const std::string& par_path,
                 const std::string& par_name, bool par_is_teamcolor)
    : name(par_name), glIdent(0) {
  auto img = std::make_shared<Image>();
  img->path(par_path);
  img->name(par_name);
  if (!img->load(par_data)) {
    spdlog::error("Failed to create an image from '{}'", par_name);
    return;
  }
  img->is_team_color(par_is_teamcolor);
  image = img;
}

bool Texture::Load(const std::string& par_filename, const std::string& par_hintpath) {
  bool found = false;
  name = std::filesystem::path(par_filename).filename().string();
  std::filesystem::path filename;
  if (!name.empty()) {
    // Direct
    filename = std::filesystem::path(par_hintpath) / name;
    if (std::filesystem::exists(filename)) {
      found = true;
    } else {
      std::filesystem::path my_hp(par_hintpath);
      for (int i = 0; i <= 3; ++i) {
        filename = my_hp / "unittextures" / name;
        if (std::filesystem::exists(filename)) {
          found = true;
          break;
        }

        // Search on parent_path for "unittextures/<filename>".
        my_hp = my_hp.parent_path();
      }
    }
  }
  if (!found && !textureLoadDir.empty() && !name.empty()) {
    filename = std::filesystem::path(textureLoadDir) / name;
    if (std::filesystem::exists(filename)) {
      found = true;
    }
  }

  if (!found) {
    return false;
  }

  auto img = std::make_shared<Image>();
  spdlog::debug("Loading {}", filename.string());
  if (!img->load(filename.string())) {
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
  gluBuild2DMipmaps(GL_TEXTURE_2D, format, image->width(), image->height(), format,
                    GL_UNSIGNED_BYTE, image->data());
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
      spdlog::warn("Texture '{}' not found", to_lower(name));
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
    if (teamcolors_.find(tmp) != teamcolors_.end()) {
      return true;
    }

    tmp = tmp.substr(0, tmp.length() - 2);
  }

  return teamcolors_.find(tmp) != teamcolors_.end();
}

bool TextureHandler::LoadFiltered(
    const std::string& par_archive_path,
    std::function<const std::string(const std::string&)>&& par_filter) {
  std::shared_ptr<IArchive> archive;

  auto path = std::filesystem::absolute(par_archive_path);
  if (std::filesystem::is_directory(path)) {
    archive = std::make_shared<CDirectoryArchive>(par_archive_path);
  } else {
    auto ext = path.extension();
    if (ext == ".7z" or ext == ".sd7") {
      archive = std::make_shared<CSevenZipArchive>(par_archive_path);
    } else if (ext == ".zip") {
      archive = std::make_shared<CZipArchive>(par_archive_path);
    } else {
      spdlog::error("Unknown archive '{}'", path.string());
      return false;
    }
  }

  if (archive->NumFiles() == 0) {
    return false;
  }

  // Read teamcolors.
  if (archive->FileExists("unittextures/tatex/teamtex.txt")) {
    std::vector<std::uint8_t> buff;
    archive->GetFileByName("unittextures/tatex/teamtex.txt", buff);

    std::stringstream stringstream(std::string(reinterpret_cast<char*>(buff.data()), buff.size()));

    // teamcolors_.reserve(32);
    std::string line;
    while (std::getline(stringstream, line)) {
      line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
      line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
      teamcolors_.insert(to_lower(line));
    }
  } else {
    spdlog::error("no file 'unittextures/tatex/teamtex.txt' in archive");
  }

  for (std::uint32_t i = 0; i < archive->NumFiles(); i++) {
    std::string name;
    int size = 0;
    int mode = 0;

    archive->FileInfo(i, name, size, mode);

    if (size < 1 || name.empty()) {
      continue;
    }

    // Extension checking
    std::string const myExt = std::filesystem::path(name).extension().string().c_str();
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

    // spdlog::debug("loading {} as {}", name, internal_name);

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

    auto tex = std::make_shared<Texture>(buf, name, internal_name, has_team_color(internal_name));
    if (tex->HasError()) {
      continue;
    }

    // Assign to map
    textures_[tex->name] = tex;
  }

  spdlog::debug("Loaded '{}' textures and '{}' teamcolors", textures_.size(), teamcolors_.size());

  return true;
}

// ------------------------------------------------------------------------------------------------
// TextureGroupHandler
// ------------------------------------------------------------------------------------------------

TextureGroupHandler::TextureGroupHandler(std::shared_ptr<TextureHandler> par_handler)
    : textureHandler(std::move(par_handler)) {}

TextureGroupHandler::~TextureGroupHandler() {
  for (auto& group : groups) {
    delete group;
  }
  groups.clear();
}

bool TextureGroupHandler::Load(const std::string& par_filename) {
  CfgList* cfg = CfgValue::LoadFile(par_filename.c_str());

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

bool TextureGroupHandler::Save(const std::string& par_filename) {
  // open a cfg writer
  CfgWriter writer(par_filename.c_str());
  if (writer.IsFailed()) {
    fltk::message("Unable to save texture groups to %s\n", par_filename.c_str());
    return false;
  }

  // create a config list and save it
  CfgList cfg;

  for (std::uint32_t a = 0; a < groups.size(); a++) {
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
  for (const auto& texture : tg->textures) {
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
  child[0] = child[1] = nullptr;
}

TextureBinTree::Node::Node(int X, int Y, int W, int H) : x(X), y(Y), w(W), h(H) {
  img_w = img_h = 0;
  child[0] = child[1] = nullptr;
}

TextureBinTree::Node::~Node() {
  SAFE_DELETE(child[0]);
  SAFE_DELETE(child[1]);
}

TextureBinTree::TextureBinTree(int par_width, int par_height) : TextureBinTree() {
  if (!image_->create(par_width, par_height)) {
    spdlog::error("Failed to create the atlas image with size '{}/{}', error was: {}", par_width,
                  par_height, image_->error());
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
  if ((n->child[0] != nullptr) || (n->child[1] != nullptr))  // not a leaf node ?
  {
    Node* r = nullptr;
    if (n->child[0] != nullptr) {
      r = InsertNode(n->child[0], w, h);
    }
    if (r != nullptr) {
      return r;
    }
    if (n->child[1] != nullptr) {
      r = InsertNode(n->child[1], w, h);
    }
    return r;
  } else {
    // Occupied
    if (n->img_w != 0) {
      return nullptr;
    }

    // Does it fit ?
    if (n->w < w || n->h < h) {
      return nullptr;
    }

    if (n->w == w && n->h == h) {
      return n;
    }

    int const ow = n->w - w;
    int const oh = n->h - h;
    if (ow > oh) {
      // Split vertically
      if (ow != 0) {
        n->child[0] = new Node(n->x + w, n->y, ow, n->h);
      }
      if (oh != 0) {
        n->child[1] = new Node(n->x, n->y + h, w, oh);
      }
    } else {
      // Split horizontally
      if (ow != 0) {
        n->child[0] = new Node(n->x + w, n->y, ow, h);
      }
      if (oh != 0) {
        n->child[1] = new Node(n->x, n->y + h, n->w, oh);
      }
    }

    return n;
  }

  return nullptr;
}

TextureBinTree::Node* TextureBinTree::AddNode(const std::shared_ptr<Image> par_subtex) {
  if (tree == nullptr) {
    // create root node
    if (par_subtex->width() > image_->width() || par_subtex->height() > image_->height()) {
      return nullptr;
    }

    tree = new Node(0, 0, image_->width(), image_->height());
  }

  auto* pn = InsertNode(tree, par_subtex->width(), par_subtex->height());
  if (pn == nullptr) {
    return nullptr;
  }

  StoreNode(pn, par_subtex);
  return pn;
}
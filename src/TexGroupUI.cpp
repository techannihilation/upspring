//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

#include "EditorIncl.h"
#include "EditorDef.h"

#include "EditorUI.h"
#include "CfgParser.h"

TexGroupUI::TexGroupUI(TextureGroupHandler* tgh, std::shared_ptr<TextureHandler> th)
    : current(nullptr), texGroupHandler(tgh), textureHandler(th) {
  CreateUI();

  auto textures = th->textures();
  for (auto& texture : textures) {
    texBrowser->AddTexture(texture.second);
  }

  texBrowser->UpdatePositions();

  UpdateGroupList();

  window->exec();
}

TexGroupUI::~TexGroupUI() { delete window; }

void TexGroupUI::SelectGroup() {
  int const index = groups->value();
  if (groups->children() == 0) {
    return;
  }

  fltk::Widget* w = groups->child(index);
  current = static_cast<TextureGroup*>(w->user_data());
  InitGroupTexBrowser();
}

void TexGroupUI::InitGroupTexBrowser() const {
  groupTexBrowser->clear();
  if (current == nullptr) {
    return;
  }
  for (const auto& texture : current->textures) {
    groupTexBrowser->AddTexture(texture);
  }
  groupTexBrowser->UpdatePositions();
  groupTexBrowser->redraw();
}

void TexGroupUI::RemoveFromGroup() const {
  auto sel = groupTexBrowser->GetSelection();

  for (auto& a : sel) {
    groupTexBrowser->RemoveTexture(a);
    current->textures.erase(a);
  }
  groupTexBrowser->UpdatePositions();
}

void TexGroupUI::SetGroupName() {
  if (current == nullptr) {
    return;
  }

  const char* str = fltk::input("Give name for new texture group:", nullptr);
  if (str != nullptr) {
    current->name = str;
    UpdateGroupList();
  }
}

void TexGroupUI::RemoveGroup() {
  if ((current != nullptr) && (groups->children() != 0)) {
    texGroupHandler->groups.erase(
        find(texGroupHandler->groups.begin(), texGroupHandler->groups.end(), current));
    current = nullptr;
    UpdateGroupList();
  }
}

void TexGroupUI::AddGroup() {
  const char* str = fltk::input("Give name for new texture group:", nullptr);
  if (str != nullptr) {
    auto* tg = new TextureGroup;
    tg->name = str;

    texGroupHandler->groups.push_back(tg);
    UpdateGroupList();
  }
}

void TexGroupUI::AddToGroup() const {
  if (current == nullptr) {
    return;
  }

  auto sel = texBrowser->GetSelection();
  for (auto& a : sel) {
    if (current->textures.find(a) == current->textures.end()) {
      current->textures.insert(a);
      groupTexBrowser->AddTexture(a);
    }
  }

  if (!sel.empty()) {
    groupTexBrowser->UpdatePositions();
  }
}

void TexGroupUI::UpdateGroupList() {
  groups->clear();
  int curval = -1;

  if ((current == nullptr) && !texGroupHandler->groups.empty()) {
    current = texGroupHandler->groups.front();
  }

  for (std::uint32_t a = 0; a < texGroupHandler->groups.size(); a++) {
    TextureGroup* gr = texGroupHandler->groups[a];
    groups->add(gr->name.c_str(), 0, nullptr, gr);
    if (current == gr) {
      curval = a;
    }
  }

  if (curval >= 0) {
    groups->value(curval);
  }
  InitGroupTexBrowser();
  groups->redraw();
}

const char* GroupConfigPattern = "Group config file(*.gcf)\0gcf\0";

void TexGroupUI::SaveGroup() const {
  static std::string fn;
  if (current == nullptr) {
    return;
  }

  if (FileSaveDlg("Save file:", GroupConfigPattern, fn)) {
    CfgList* cfg = TextureGroupHandler::MakeConfig(current);
    CfgWriter writer(fn.c_str());
    if (writer.IsFailed()) {
      fltk::message("Failed to write %s\n", fn.c_str());
      return;
    }

    cfg->Write(writer, true);
  }
}

void TexGroupUI::LoadGroup() {
  static std::string fn;
  if (current == nullptr) {
    return;
  }

  if (FileOpenDlg("Load file:", GroupConfigPattern, fn)) {
    CfgList* cfg = CfgValue::LoadFile(fn.c_str());
    if (cfg == nullptr) {
      fltk::message("Failed to read %s\n", fn.c_str());
      return;
    }
    TextureGroup* tg = texGroupHandler->LoadGroup(cfg);
    if (tg != nullptr) {
      tg->name = fltk::filename_name(fn.c_str());
      UpdateGroupList();
    }
  }
}

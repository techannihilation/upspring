//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

public:
TexGroupUI(TextureGroupHandler* tgh, std::shared_ptr<TextureHandler> th);
~TexGroupUI();

private:
// UI callbacks
void SelectGroup();
void SetGroupName();
void RemoveGroup();
void RemoveFromGroup() const;
void AddGroup();
void AddToGroup() const;
void UpdateGroupList();
void InitGroupTexBrowser() const;
void SaveGroup() const;
void LoadGroup();

public:
TextureGroup* current;
TextureGroupHandler* texGroupHandler;
std::shared_ptr<TextureHandler> textureHandler;

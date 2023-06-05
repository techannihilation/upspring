//-----------------------------------------------------------------------
//  Upspring model editor
//  Copyright 2005 Jelmer Cnossen
//  This code is released under GPL license, see LICENSE.HTML for info.
//-----------------------------------------------------------------------

// All this code is included in the FLTK-generated EditorUI.h, and will be inside the EditorUI class
public:
~EditorUI();

// Menu callbacks
void menuFileLoad();
void menuFileSave();
void menuFileSaveAs();
void menuFileNew();
void menuFileExit() const;
static void menuHelpAbout();
void menuObjectLoad();
void menuObjectSave() const;
void menuObjectReplace();
void menuObjectMerge();
void menuObjectLoad3DS();
void menuObjectSave3DS();
void menuObjectApproxOffset();
void menuObjectResetPos();
void menuObjectResetTransform();
void menuObjectResetScaleRot();
void menuObjectFlipPolygons();
void menuObjectRecalcNormals();
void menuObjectShowAnimWindows() const;
void menuObjectGenCSurf() const;
void menuEditOptimizeAll() const;
void menuEditOptimizeSelected() const;
void menuEditUndo();
void menuEditRedo();
void menuEditShowUndoBufferViewer();
void menuMappingImportUV();
void menuMappingExportUV() const;
void menuMappingShow() const;
void menuMappingShowTexBuilder() const;
void menuSettingsShowArchiveList();
void menuSettingsTextureGroups() const;
void menuSettingsRestoreViews();
void SaveSettings();
void menuSettingsSetBgColor() const;
static void menuSetSpringDir();
void menuScriptLoad() const;

// Function callbacks for the UI components
void uiAddObject();
void uiCloneObject();
void uiCopy();
void uiCut();
void uiPaste();
void uiSwapObjects();
void uiDeleteSelection();
void uiApplyTransform();
void uiUniformScale();
void uiCalculateMidPos();
void uiCalculateRadius();
void uiSetRenderMethod(RenderMethod rm) const;

void uiRotate3DOTex();  // rotate tool button

#define DegreesToRadians (3.141593f / 180.0f)
void uiObjectStateChanged(Vector3 MdlObject::*state, float Vector3::*axis, fltk::Input* o,
                          float scale = 1.0f);
void uiModelStateChanged(float* prop, fltk::Input* o);
void uiObjectRotationChanged(int axis, fltk::Input* o);
void uiObjectPositionChanged(int axis, fltk::Input* o);
void browserSetObjectName(MdlObject* obj) const;
void uiAddUnitTextures() const;

void Show(bool initial);
void Update();
void Initialize();
void UpdateTitle();
bool Save();
bool Load(const std::string& fn);
void SetModel(Model* mdl);
void SetTool(Tool* t);
void RenderScene(IView* view) const;
void SetMapping(int mapping) const;
void BrowseForTexture(int index);
void SetModelTexture(int index, std::shared_ptr<Texture> par_texture);
void ReloadTexture(int index);
void ConvertToS3O();
MdlObject* GetSingleSel() const;
void SelectionUpdated();
// Texture groups
void UpdateTextureGroups() const;
void SelectTextureGroup(fltk::Widget* w, void* d) const;
void InitTexBrowser() const;
TextureGroup* GetCurrentTexGroup() const;
fltk::Color SetTeamColor();
void LoadSettings();
void LoadToolWindowSettings() const;
void SerializeConfig(CfgList& cfg, bool store);

fltk::Color teamColor{};
std::string filename, windowTitle;
Model* model{};
ModelDrawer* modelDrawer{};
Tool* currentTool{};

Tools tools;

class ObjectView;
ObjectView* objectViewer{};

class EditorCB : public IEditor {
 public:
  EditorUI* ui;

  void RedrawViews() { ui->Update(); }
  std::vector<EditorViewWindow*> GetViews();
  void SelectionUpdated() { ui->SelectionUpdated(); }
  Model* GetMdl() { return ui->model; }
  Tool* GetTool();
  void RenderScene(IView* view) { ui->RenderScene(view); }
  std::shared_ptr<TextureHandler> GetTextureHandler() { return ui->textureHandler; }
  void SetTextureSelectCallback(void (*cb)(std::shared_ptr<Texture>, void*), void* d) {
    ui->texBrowser->SetSelectCallback(cb, d);
  }
  float GetTime() { return ui->uiTimeline->GetTime(); }
  void MergeView(EditorViewWindow* own, EditorViewWindow* other);
  void AddView(EditorViewWindow* v);
  void SetModel(Model* mdl) { ui->SetModel(mdl); }
};

EditorCB callback;
CopyBuffer copyBuffer;
ArchiveList archives;

std::shared_ptr<TextureHandler> textureHandler;
TextureGroupHandler* textureGroupHandler{};

BackupViewerUI* uiBackupViewer{};
RotatorUI* uiRotator{};
IK_UI* uiIK{};
TimelineUI* uiTimeline{};
MappingUI* uiMapping{};
TexBuilderUI* uiTexBuilder{};
AnimTrackEditorUI* uiAnimTrackEditor{};
bool optimizeOnLoad{};

lua_State* luaState{};
std::vector<ScriptedMenuItem*> scripts;

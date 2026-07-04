#pragma once
#include <memory>
#include <string>
#include <vector>

class Entity;
class Window;
class RenderContext;

#include "../Asset/AssetDatabase.h"
#include "../Asset/Types/Material.h"
#include "Log.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"

struct GuiLog
{
    ImGuiTextBuffer Buf;
    ImGuiTextFilter Filter;
    ImVector<int> LineOffsets;
    ImVector<int> LineLevels;
    bool AutoScroll;

    GuiLog();

    void Clear();
    void AddLog(const char* fmt, ...) IM_FMTARGS(2);
    void AppendLog(const std::string& line, LogLevel level = LogLevel::INFO);
    void Draw(const char* title, bool* p_open = NULL);
};

class Gui
{
  public:
    Gui();
    ~Gui();
    void Init(Window* win);
    void BeginFrame(bool gameInputExclusive);
    ImGuiID BuildWidgets(RenderContext& context);
    void EndFrame();
    void Clean();

  private:
    void ShowMenu(RenderContext& context);
    void ShowPreferences(RenderContext& context);
    void ShowScenePathModals(RenderContext& context);
    void LoadPreferencesFromConfig();
    void ApplyPreferences(bool saveToDisk);
    void ShowLog();
    void ShowDetail();
    void ShowAssetDetail();
    void ShowOutline();
    void ShowAssetBrowser(RenderContext& context);
    void OpenAssetDetail(const std::string& path);
    void ClearAssetDetail();
    void ReloadAssetDetailMaterial();
    void SaveAssetDetailMaterial();
    void RefreshAssetBrowserListing();
    void SyncAssetDetailFromBrowserSelection();
    void ClearAssetBrowserSelection();
    void ShowScene();
#if GLE_ENABLE_RENDERDOC
    void ShowRenderDoc();
#endif
    void SelectEntity(const std::shared_ptr<Entity>& entity);
    void DragFloat(float& value, const char* label, const float speed = 0.1, const float min = 0.0,
                   const float max = 10.0);

    enum class PendingSceneActionKind
    {
        None,
        NewScene,
        LoadScenePath,
        Exit,
    };
    struct PendingSceneAction
    {
        PendingSceneActionKind kind = PendingSceneActionKind::None;
        std::string path;
    };

    void RequestSceneAction(const PendingSceneAction& action, RenderContext& context);
    void ExecuteSceneAction(const PendingSceneAction& action, RenderContext& context);
    void ShowDiscardSceneModal(RenderContext& context);

  private:
    GuiLog logWindow;
    int item_selected_idx = 0;
    int m_detailLastSelectedIdx = -1;
    char m_detailNameBuf[256]{};
    enum class DetailViewMode
    {
        Entity,
        Asset,
    };
    DetailViewMode m_detailViewMode = DetailViewMode::Entity;
    std::string m_assetDetailPath;
    AssetMeta m_assetDetailMeta{};
    std::shared_ptr<Material> m_assetDetailEditMaterial;
    bool m_assetDetailMaterialFileShowInUi = true;
    char m_assetBrowserPath[512] = "Content/Project";
    ImGuiSelectionBasicStorage m_assetBrowserSelection;
    std::vector<int> m_assetBrowserSelectedIndices;
    std::vector<int> m_assetBrowserLastSyncedDetailIndices;
    std::string m_assetBrowserStatus;
    std::vector<std::string> m_assetBrowserNames;
    std::vector<std::string> m_assetBrowserPaths;
    std::vector<int> m_assetBrowserKinds;
    int m_createEntityType = 0;
    bool m_createEntityAddTransform = true;
    int m_createEntityModelIdx = 0;
    int m_createEntityCubemapIdx = 0;
    char m_createEntityName[128] = "New Entity";
    std::string m_createEntityStatus;
    char m_materialCreateName[128] = "NewMaterial";
    bool m_openCreateMaterialModal = false;
    char m_folderCreateName[128] = "NewFolder";
    bool m_openCreateFolderModal = false;
    char m_scenePathInput[512] = "Content/Project/scenes/Untitled.scene";
    bool m_openSaveSceneAsModal = false;
    char m_sceneCreateName[128] = "NewScene";
    bool m_openCreateSceneModal = false;
    PendingSceneAction m_pendingSceneAction;
    bool m_openDiscardSceneModal = false;
    bool m_openPreferences = false;
    int m_prefRenderPipelineIdx = 0;
    char m_prefRenderPipelineName[64] = "Deferred";
    int m_prefOpenGLMajor = 4;
    int m_prefOpenGLMinor = 6;
    bool m_prefVsync = true;
    bool m_prefEnableRenderDoc = false;
    bool m_prefShowRenderDocOverlay = true;
    char m_prefRenderDocPath[512]{};
    int m_prefInitialWidth = 1280;
    int m_prefInitialHeight = 720;
    char m_prefProjectRoot[512]{};
    char m_prefProjectContentRoot[256] = "Content/Project";
    char m_prefEngineContentRoot[256] = "Content/Engine";
    char m_prefStartupScene[512]{};
    char m_prefHiddenExtensions[512] = ".meta";
    bool m_prefNeedsRestart = false;
    std::string m_prefStatus;
    RenderContext* m_frameContext = nullptr;
    Window* m_window = nullptr;
};

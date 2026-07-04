#include "Gui.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "../Entity/Components/Camera.h"
#include "../Entity/Components/Light.h"
#include "../Entity/Components/MeshRender.h"
#include "../Entity/Components/SkyBox.h"
#include "../Entity/Components/Transform.h"
#include "../Entity/Entity.h"
#include "../Entity/EntityManager.h"
#include "../Renderer/RenderContext.h"
#include "../Renderer/RenderPipeline/RenderPipelineRegistry.h"
#include "../Scene/SceneSerializer.h"
#include "../Scene/SceneSession.h"
#include "LightingConvention.h"
#include "Log.h"
#include "RenderDocSupport.h"
#include "Window.h"

#include "../Asset/AssetDatabase.h"
#include "../Asset/AssetPaths.h"
#include "../Asset/AssetManager.h"
#include "../Asset/LoaderManager.h"
#include "../Asset/Types/Model.h"
#include "../Asset/Types/Material.h"
#include "../Asset/Types/Model.h"
#include "../Asset/Types/Shader.h"
#include "../Asset/Types/Texture/Texture2D.h"
#include "../Asset/Types/Texture/IBLImage.h"
#include "Config.h"
#include "imgui_impl_opengl3.h"
#include "util.h"

#define MODULE "Gui"

GuiLog::GuiLog()
{
    AutoScroll = true;
    Clear();
}

Gui::Gui() = default;

Gui::~Gui() = default;

void GuiLog::Clear()
{
    Buf.clear();
    LineOffsets.clear();
    LineLevels.clear();
    LineOffsets.push_back(0);
}

namespace
{
ImVec4 LogLevelTextColor(LogLevel level)
{
    switch (level)
    {
    case LogLevel::ERROR:
        return ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
    case LogLevel::WARNING:
        return ImVec4(1.0f, 0.85f, 0.2f, 1.0f);
    default:
        return ImGui::GetStyle().Colors[ImGuiCol_Text];
    }
}

void DrawLogLine(const char* line_start, const char* line_end, LogLevel level)
{
    ImGui::PushStyleColor(ImGuiCol_Text, LogLevelTextColor(level));
    ImGui::TextUnformatted(line_start, line_end);
    ImGui::PopStyleColor();
}

LogLevel LineLevelAt(const ImVector<int>& lineLevels, int lineNo)
{
    if (lineNo >= 0 && lineNo < lineLevels.Size)
        return static_cast<LogLevel>(lineLevels[lineNo]);
    return LogLevel::INFO;
}

void AppendLogLines(GuiLog& log, const std::string& line, LogLevel level)
{
    int old_size = log.Buf.size();
    log.Buf.append(line.c_str(), line.c_str() + line.size());
    for (int new_size = log.Buf.size(); old_size < new_size; old_size++)
    {
        if (log.Buf[old_size] == '\n')
        {
            log.LineOffsets.push_back(old_size + 1);
            log.LineLevels.push_back(static_cast<int>(level));
        }
    }
}
} // namespace

void GuiLog::AddLog(const char* fmt, ...)
{
    int old_size = Buf.size();
    va_list args;
    va_start(args, fmt);
    Buf.appendfv(fmt, args);
    va_end(args);
    for (int new_size = Buf.size(); old_size < new_size; old_size++)
    {
        if (Buf[old_size] == '\n')
        {
            LineOffsets.push_back(old_size + 1);
            LineLevels.push_back(static_cast<int>(LogLevel::INFO));
        }
    }
}

void GuiLog::AppendLog(const std::string& line, LogLevel level)
{
    AppendLogLines(*this, line, level);
}

void GuiLog::Draw(const char* title, bool* p_open)
{
    if (!ImGui::Begin(title, p_open))
    {
        ImGui::End();
        return;
    }
    if (ImGui::BeginPopup("Options"))
    {
        ImGui::Checkbox("Auto-scroll", &AutoScroll);
        ImGui::EndPopup();
    }

    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    bool clear = ImGui::Button("Clear");
    ImGui::SameLine();
    bool copy = ImGui::Button("Copy");
    ImGui::SameLine();
    Filter.Draw("Filter", -100.0f);

    ImGui::Separator();

    if (ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (clear)
            Clear();
        if (copy)
            ImGui::LogToClipboard();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        const char* buf = Buf.begin();
        const char* buf_end = Buf.end();
        if (Filter.IsActive())
        {
            for (int line_no = 0; line_no < LineOffsets.Size; line_no++)
            {
                const char* line_start = buf + LineOffsets[line_no];
                const char* line_end =
                    (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                if (Filter.PassFilter(line_start, line_end))
                    DrawLogLine(line_start, line_end, LineLevelAt(LineLevels, line_no));
            }
        }
        else
        {
            ImGuiListClipper clipper;
            clipper.Begin(LineOffsets.Size);
            while (clipper.Step())
            {
                for (int line_no = clipper.DisplayStart; line_no < clipper.DisplayEnd; line_no++)
                {
                    const char* line_start = buf + LineOffsets[line_no];
                    const char* line_end =
                        (line_no + 1 < LineOffsets.Size) ? (buf + LineOffsets[line_no + 1] - 1) : buf_end;
                    DrawLogLine(line_start, line_end, LineLevelAt(LineLevels, line_no));
                }
            }
            clipper.End();
        }
        ImGui::PopStyleVar();
        if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}

namespace
{

enum class AssetEntryKind : int
{
    Unknown = 0,
    Folder,
    Shader,
    Texture,
    Model,
    Material,
    Scene,
};

static std::string NormalizeAssetPath(std::string path)
{
    for (char& c : path)
    {
        if (c == '\\')
            c = '/';
    }
    while (path.size() > 1 && path.back() == '/')
        path.pop_back();
    return path;
}

static std::string ToLowerExtension(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

static AssetEntryKind ClassifyAssetEntry(const std::filesystem::path& path)
{
    std::error_code ec;
    if (std::filesystem::is_directory(path, ec))
        return AssetEntryKind::Folder;

    const std::string ext = ToLowerExtension(path);
    if (ext == ".glsl" || ext == ".frag" || ext == ".vert" || ext == ".comp")
        return AssetEntryKind::Shader;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr")
        return AssetEntryKind::Texture;
    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".dae" || ext == ".3ds")
        return AssetEntryKind::Model;
    if (ext == ".mat" || ext == ".material")
        return AssetEntryKind::Material;
    if (ext == ".scene")
        return AssetEntryKind::Scene;
    return AssetEntryKind::Unknown;
}

static const char* AssetEntryKindLabel(AssetEntryKind kind)
{
    switch (kind)
    {
    case AssetEntryKind::Folder:
        return "Folder";
    case AssetEntryKind::Shader:
        return "Shader";
    case AssetEntryKind::Texture:
        return "Texture";
    case AssetEntryKind::Model:
        return "Model";
    case AssetEntryKind::Material:
        return "Material";
    case AssetEntryKind::Scene:
        return "Scene";
    default:
        return "Unknown";
    }
}

static bool CanLoadAssetEntry(AssetEntryKind kind)
{
    switch (kind)
    {
    case AssetEntryKind::Shader:
    case AssetEntryKind::Texture:
    case AssetEntryKind::Model:
    case AssetEntryKind::Material:
    case AssetEntryKind::Scene:
        return true;
    default:
        return false;
    }
}

static bool IsAssetPickerSentinelKey(const std::string& key)
{
    return key.empty() || key.front() == '(';
}

static std::string StripContentRootPrefixForDisplay(const std::string& path)
{
    static constexpr const char* kPrefixes[] = {
        "Content/Project/",
        "Content/Engine/",
        "engine://",
        "project://",
    };
    for (const char* prefix : kPrefixes)
    {
        const size_t len = std::strlen(prefix);
        if (path.size() >= len && path.compare(0, len, prefix) == 0)
            return path.substr(len);
    }
    return path;
}

static std::string GetPathTail(const std::string& path, int componentCount)
{
    std::string normalized = path;
    for (char& c : normalized)
    {
        if (c == '\\')
            c = '/';
    }
    if (componentCount <= 0)
        return normalized;

    int found = 0;
    for (size_t pos = normalized.size(); pos > 0; --pos)
    {
        if (normalized[pos - 1] != '/')
            continue;
        ++found;
        if (found == componentCount)
            return normalized.substr(pos);
    }
    return normalized;
}

static std::vector<std::string> BuildUniqueAssetDisplayLabels(const std::vector<std::string>& keys)
{
    const size_t count = keys.size();
    std::vector<std::string> relative(count);
    std::vector<std::string> labels(count);
    std::vector<bool> isSentinel(count);

    for (size_t i = 0; i < count; ++i)
    {
        isSentinel[i] = IsAssetPickerSentinelKey(keys[i]);
        if (isSentinel[i])
        {
            labels[i] = keys[i];
            continue;
        }
        relative[i] = StripContentRootPrefixForDisplay(keys[i]);
        labels[i] = std::filesystem::path(relative[i]).filename().string();
    }

    for (int depth = 2; depth <= 8; ++depth)
    {
        std::unordered_map<std::string, int> freq;
        for (size_t i = 0; i < count; ++i)
        {
            if (isSentinel[i])
                continue;
            ++freq[labels[i]];
        }

        bool anyDup = false;
        for (const auto& [label, n] : freq)
        {
            if (n > 1)
            {
                anyDup = true;
                break;
            }
        }
        if (!anyDup)
            break;

        for (size_t i = 0; i < count; ++i)
        {
            if (isSentinel[i])
                continue;
            labels[i] = GetPathTail(relative[i], depth);
        }
    }

    return labels;
}

struct AssetPickerItems
{
    std::vector<std::string> keys;
    std::vector<std::string> labels;

    void SetKeys(std::vector<std::string>&& inKeys)
    {
        keys = std::move(inKeys);
        RebuildLabels();
    }

    void RebuildLabels()
    {
        labels = BuildUniqueAssetDisplayLabels(keys);
    }

    size_t size() const
    {
        return keys.size();
    }
};

static bool StringContainsInsensitive(const std::string& haystack, const char* needle)
{
    if (!needle || !needle[0])
        return true;

    auto toLower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
    std::string h = haystack;
    std::string n = needle;
    for (char& c : h)
        c = toLower(static_cast<unsigned char>(c));
    for (char& c : n)
        c = toLower(static_cast<unsigned char>(c));
    return h.find(n) != std::string::npos;
}

static bool DrawAssetPathCombo(const char* label, int* currentItem, const AssetPickerItems& items)
{
    if (!currentItem || items.size() == 0)
        return false;

    if (*currentItem < 0 || static_cast<size_t>(*currentItem) >= items.size())
        *currentItem = 0;

    const char* preview = items.labels[static_cast<size_t>(*currentItem)].c_str();

    ImGui::PushID(label);
    bool changed = false;

    if (ImGui::BeginCombo(label, preview))
    {
        static std::unordered_map<ImGuiID, std::array<char, 128>> sFilters;
        const ImGuiID filterId = ImGui::GetID("##AssetComboFilter");
        std::array<char, 128>& filterBuf = sFilters[filterId];

        if (ImGui::IsWindowAppearing())
            filterBuf.front() = '\0';

        ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
        ImGui::InputTextWithHint("##AssetComboFilter", "Filter (Ctrl+F)...", filterBuf.data(), filterBuf.size());

        for (int i = 0; i < static_cast<int>(items.size()); ++i)
        {
            const size_t idx = static_cast<size_t>(i);
            const bool passFilter = StringContainsInsensitive(items.labels[idx], filterBuf.data()) ||
                                    StringContainsInsensitive(items.keys[idx], filterBuf.data());
            if (!passFilter)
                continue;

            const bool selected = (*currentItem == i);
            if (ImGui::Selectable(items.labels[idx].c_str(), selected))
            {
                *currentItem = i;
                changed = true;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", items.keys[idx].c_str());
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        const size_t idx = static_cast<size_t>(*currentItem);
        if (idx < items.size() && !IsAssetPickerSentinelKey(items.keys[idx]))
            ImGui::SetTooltip("%s", items.keys[idx].c_str());
    }

    ImGui::PopID();
    return changed;
}

static void CollectLoadedCubemapPickerItems(AssetPickerItems& out)
{
    std::vector<std::string> keys;
    AssetManager::CollectUiVisibleKeys(AssetManager::Get().m_iblImages, keys, true);
    out.SetKeys(std::move(keys));
}

static int FindCubemapIndex(const std::vector<std::string>& keys, const IBLImage* current)
{
    if (!current || keys.empty())
        return 0;
    for (size_t i = 0; i < keys.size(); ++i)
    {
        if (keys[i] == current->m_path)
            return static_cast<int>(i);
    }
    return 0;
}

static bool LoadAssetEntry(AssetEntryKind kind, const std::string& path, std::string& outMessage,
                           RenderContext* context = nullptr)
{
    AssetManager& assets = AssetManager::Get();
    switch (kind)
    {
    case AssetEntryKind::Scene:
        if (!context)
        {
            outMessage = "Cannot open scene: render context unavailable.";
            return false;
        }
        return SceneSerializer::LoadScene(path, *context, outMessage);
    case AssetEntryKind::Shader:
        if (std::shared_ptr<Shader> shader = assets.LoadShader(path))
        {
            outMessage = "Shader loaded: " + shader->m_name;
            LogA(LogLevel::INFO, "Asset browser: {}", outMessage);
            return true;
        }
        outMessage = "Shader load failed: " + path;
        LogA(LogLevel::ERROR, "Asset browser: {}", outMessage);
        return false;
    case AssetEntryKind::Texture:
    {
        const std::string resolvedPath = AssetDataBase::Get().ResolveAssetRef(path);
        const AssetMeta meta = AssetDataBase::Get().LoadOrCreateMeta(resolvedPath.empty() ? path : resolvedPath);
        if (meta.type == AssetType::TextureCube)
        {
            if (std::shared_ptr<IBLImage> ibl = assets.LoadIBL(path, true))
            {
                outMessage = "IBL loaded: " + ibl->m_name;
                LogA(LogLevel::INFO, "Asset browser: {}", outMessage);
                return true;
            }
            outMessage = "IBL load failed: " + path;
            LogA(LogLevel::ERROR, "Asset browser: {}", outMessage);
            return false;
        }
        if (std::shared_ptr<Texture2D> tex = assets.LoadTexture(path, true))
        {
            outMessage = "Texture loaded: " + tex->m_name;
            LogA(LogLevel::INFO, "Asset browser: {}", outMessage);
            return true;
        }
        outMessage = "Texture load failed: " + path;
        LogA(LogLevel::ERROR, "Asset browser: {}", outMessage);
        return false;
    }
    case AssetEntryKind::Model:
        if (std::shared_ptr<Model> model = assets.LoadModel(path))
        {
            outMessage = "Model loaded: " + model->m_name;
            LogA(LogLevel::INFO, "Asset browser: {}", outMessage);
            return true;
        }
        outMessage = "Model load failed: " + path;
        LogA(LogLevel::ERROR, "Asset browser: {}", outMessage);
        return false;
    case AssetEntryKind::Material:
        if (std::shared_ptr<Material> mat = assets.LoadMaterial(path))
        {
            outMessage = "Material loaded: " + mat->m_name + " (queue=" + std::to_string(mat->GetRenderQueue()) + ")";
            LogA(LogLevel::INFO, "Asset browser: {}", outMessage);
            return true;
        }
        outMessage = "Material load failed: " + path;
        LogA(LogLevel::ERROR, "Asset browser: {}", outMessage);
        return false;
    default:
        outMessage = "Cannot load entry type: " + std::string(AssetEntryKindLabel(kind));
        LogA(LogLevel::WARNING, "Asset browser: {}", outMessage);
        return false;
    }
}

static void CollectAssetBrowserSelectionIndices(ImGuiSelectionBasicStorage& selection, std::vector<int>& outIndices)
{
    outIndices.clear();
    void* it = nullptr;
    ImGuiID id = 0;
    while (selection.GetNextSelectedItem(&it, &id))
        outIndices.push_back(static_cast<int>(id));
}

static int CountLoadableAssetBrowserIndices(const std::vector<int>& indices, const std::vector<int>& kinds,
                                            const size_t entryCount)
{
    int count = 0;
    for (const int idx : indices)
    {
        if (idx < 0 || static_cast<size_t>(idx) >= entryCount)
            continue;
        if (CanLoadAssetEntry(static_cast<AssetEntryKind>(kinds[static_cast<size_t>(idx)])))
            ++count;
    }
    return count;
}

static void LoadAssetBrowserEntriesByIndices(const std::vector<int>& indices, const std::vector<std::string>& paths,
                                             const std::vector<int>& kinds, std::string& outStatus)
{
    int ok = 0;
    int fail = 0;
    std::string lastError;

    for (const int idx : indices)
    {
        if (idx < 0 || static_cast<size_t>(idx) >= paths.size())
            continue;

        const AssetEntryKind kind = static_cast<AssetEntryKind>(kinds[static_cast<size_t>(idx)]);
        if (!CanLoadAssetEntry(kind) || kind == AssetEntryKind::Scene)
            continue;

        std::string msg;
        if (LoadAssetEntry(kind, paths[static_cast<size_t>(idx)], msg))
            ++ok;
        else
        {
            ++fail;
            lastError = std::move(msg);
        }
    }

    if (ok == 0 && fail == 0)
        outStatus = "No loadable assets in selection.";
    else if (fail == 0)
        outStatus = "Loaded " + std::to_string(ok) + " asset(s).";
    else
        outStatus = "Loaded " + std::to_string(ok) + ", failed " + std::to_string(fail) + ". " + lastError;
}

static void PruneAssetBrowserSelection(ImGuiSelectionBasicStorage& selection, size_t entryCount)
{
    void* it = nullptr;
    ImGuiID id = 0;
    while (selection.GetNextSelectedItem(&it, &id))
    {
        if (static_cast<size_t>(id) >= entryCount)
            selection.SetItemSelected(id, false);
    }
}

static void CollectAssetBrowserEntries(const std::string& dirPath, std::vector<std::string>& names,
                                       std::vector<std::string>& paths, std::vector<int>& kinds)
{
    names.clear();
    paths.clear();
    kinds.clear();

    const std::filesystem::path dir(dirPath);
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
        return;

    std::vector<std::filesystem::directory_entry> items;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
    {
        if (ec)
            break;
        items.push_back(entry);
    }

    std::sort(items.begin(), items.end(),
              [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b)
              {
                  const bool aDir = a.is_directory();
                  const bool bDir = b.is_directory();
                  if (aDir != bDir)
                      return aDir > bDir;
                  return a.path().filename().string() < b.path().filename().string();
              });

    for (const auto& entry : items)
    {
        if (!entry.is_directory() && Config::Get().IsAssetBrowserHiddenFile(entry.path()))
            continue;

        const std::string name = entry.path().filename().string();
        if (name.empty())
            continue;

        names.push_back(name);
        std::string entryPath = NormalizeAssetPath(entry.path().generic_string());
        if (AssetPaths::IsInitialized())
        {
            entryPath = AssetPaths::Get().ToRelativeSourcePath(entry.path());
        }
        paths.push_back(std::move(entryPath));
        kinds.push_back(static_cast<int>(ClassifyAssetEntry(entry.path())));
    }
}

static std::string TrimAssetName(std::string name)
{
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front())))
        name.erase(name.begin());
    while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back())))
        name.pop_back();
    return name;
}

static std::string StripMaterialExtension(std::string name)
{
    name = TrimAssetName(std::move(name));
    if (name.size() >= 4)
    {
        std::string ext = name.substr(name.size() - 4);
        for (char& c : ext)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext == ".mat")
            name.erase(name.size() - 4);
    }
    return TrimAssetName(std::move(name));
}

static std::string MakeUniqueMaterialFileName(const std::string& dirPath, const std::string& baseName)
{
    std::vector<std::string> existing;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dirPath, ec))
    {
        if (ec)
            break;
        if (!entry.is_regular_file(ec))
            continue;
        if (ToLowerExtension(entry.path()) == ".mat")
            existing.push_back(entry.path().stem().string());
    }

    const std::string uniqueStem =
        Util::GetUniqueName(baseName.empty() ? std::string("NewMaterial") : baseName, existing);
    return uniqueStem + ".mat";
}

static bool IsValidFolderName(const std::string& name)
{
    if (name.empty())
        return false;
    for (const char c : name)
    {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            return false;
    }
    return true;
}

static std::string MakeUniqueFolderName(const std::string& dirPath, const std::string& baseName)
{
    std::vector<std::string> existing;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dirPath, ec))
    {
        if (ec)
            break;
        if (entry.is_directory(ec))
            existing.push_back(entry.path().filename().string());
    }

    return Util::GetUniqueName(baseName.empty() ? std::string("NewFolder") : baseName, existing);
}

static bool CreateFolderInDirectory(const std::string& dirPath, const char* nameInput, std::string& outFolderPath,
                                    std::string& outMessage)
{
    std::error_code ec;
    if (!std::filesystem::exists(dirPath, ec) || !std::filesystem::is_directory(dirPath, ec))
    {
        outMessage = "Target directory does not exist.";
        return false;
    }

    const std::string baseName = TrimAssetName(nameInput);
    if (!IsValidFolderName(baseName))
    {
        outMessage = "Invalid folder name.";
        return false;
    }

    const std::string folderName = MakeUniqueFolderName(dirPath, baseName);
    outFolderPath = NormalizeAssetPath((std::filesystem::path(dirPath) / folderName).generic_string());

    if (!std::filesystem::create_directory(outFolderPath, ec))
    {
        outMessage = "Failed to create folder: " + outFolderPath;
        LogA(LogLevel::ERROR, "Asset browser: {}", outMessage);
        return false;
    }

    outMessage = "Created folder: " + outFolderPath;
    LogA(LogLevel::INFO, "Asset browser: {}", outMessage);
    return true;
}

static bool CreateMaterialFileInDirectory(const std::string& dirPath, const char* nameInput, std::string& outFilePath,
                                          std::string& outMessage)
{
    std::error_code ec;
    if (!std::filesystem::exists(dirPath, ec) || !std::filesystem::is_directory(dirPath, ec))
    {
        outMessage = "Target directory does not exist.";
        return false;
    }

    const std::string baseName = StripMaterialExtension(nameInput);
    const std::string fileName = MakeUniqueMaterialFileName(dirPath, baseName);
    outFilePath = NormalizeAssetPath((std::filesystem::path(dirPath) / fileName).generic_string());

    MaterialFileDesc desc;
    desc.materialName = std::filesystem::path(fileName).stem().string();
    desc.shaderPath = "engine://shaders/DefaultLit.glsl";
    desc.showInUi = true;

    if (!LoaderManager::Get().WriteMaterialFile(outFilePath, desc))
    {
        outMessage = "Failed to create material file: " + outFilePath;
        return false;
    }

    AssetDataBase::Get().LoadOrCreateMeta(outFilePath);
    outMessage = "Created material file: " + outFilePath;
    LogA(LogLevel::INFO, "Asset browser: {}", outMessage);
    return true;
}

static bool DeleteAssetFile(const std::string& path, std::string& outMessage)
{
    const std::string normalizedPath = AssetDataBase::NormalizeSourcePath(path);
    std::error_code ec;
    if (!std::filesystem::exists(normalizedPath, ec) || std::filesystem::is_directory(normalizedPath, ec))
    {
        outMessage = "Cannot delete: path is not a file.";
        return false;
    }

    if (AssetManager::Get().IsAssetLoadedAtPath(normalizedPath))
    {
        outMessage = "Cannot delete: asset is loaded in memory. Unload is not supported yet.";
        LogA(LogLevel::WARNING, "Asset browser: {}", outMessage);
        return false;
    }

    if (!std::filesystem::remove(normalizedPath, ec))
    {
        outMessage = "Failed to delete file: " + normalizedPath;
        LogA(LogLevel::ERROR, "Asset browser: {}", outMessage);
        return false;
    }

    const std::string metaPath = normalizedPath + ".meta";
    if (std::filesystem::exists(metaPath, ec))
    {
        std::filesystem::remove(metaPath, ec);
        if (ec)
            LogA(LogLevel::WARNING, "Asset browser: failed to delete meta '{}'", metaPath);
    }

    AssetDataBase::Get().RemoveMeta(normalizedPath);
    outMessage = "Deleted: " + normalizedPath;
    LogA(LogLevel::INFO, "Asset browser: {}", outMessage);
    return true;
}

static bool IsMaterialAssetPath(const std::string& path)
{
    const std::string ext = ToLowerExtension(std::filesystem::path(path));
    return ext == ".mat" || ext == ".material";
}

static void DrawMaterialProperties(Material* mat, const std::function<void()>& onChanged = {})
{
    if (!mat)
        return;

    const auto& textures = AssetManager::Get().m_textures;
    AssetPickerItems textureItems;
    {
        std::vector<std::string> textureKeys;
        textureKeys.emplace_back("(None)");
        std::vector<std::string> visible;
        AssetManager::CollectUiVisibleKeys(textures, visible, true);
        textureKeys.insert(textureKeys.end(), visible.begin(), visible.end());
        textureItems.SetKeys(std::move(textureKeys));
    }

    const auto& properties = mat->GetProperties();
    const std::vector<std::string>& propertyOrder = mat->GetPropertyOrder();
    auto drawOneProperty = [&](const std::string& propName)
    {
        const auto propIt = properties.find(propName);
        if (propIt == properties.end())
            return;
        const MaterialProperty& prop = propIt->second;

        ImGui::PushID(propName.c_str());
        switch (prop.type)
        {
        case MaterialPropertyType::Float:
        {
            float v = std::get<float>(prop.value);
            if (ImGui::DragFloat(propName.c_str(), &v, 0.01f))
            {
                mat->SetFloatProperty(propName, v);
                if (onChanged)
                    onChanged();
            }
            break;
        }
        case MaterialPropertyType::Int:
        {
            int v = std::get<int>(prop.value);
            if (ImGui::DragInt(propName.c_str(), &v, 1))
            {
                mat->SetIntProperty(propName, v);
                if (onChanged)
                    onChanged();
            }
            break;
        }
        case MaterialPropertyType::Bool:
        {
            bool v = std::get<bool>(prop.value);
            if (ImGui::Checkbox(propName.c_str(), &v))
            {
                mat->SetBoolProperty(propName, v);
                if (onChanged)
                    onChanged();
            }
            break;
        }
        case MaterialPropertyType::Vec2:
        {
            glm::vec2 v = std::get<glm::vec2>(prop.value);
            if (ImGui::DragFloat2(propName.c_str(), &v.x, 0.01f))
            {
                mat->SetVec2Property(propName, v);
                if (onChanged)
                    onChanged();
            }
            break;
        }
        case MaterialPropertyType::Vec3:
        {
            glm::vec3 v = std::get<glm::vec3>(prop.value);
            if (ImGui::DragFloat3(propName.c_str(), &v.x, 0.01f))
            {
                mat->SetVec3Property(propName, v);
                if (onChanged)
                    onChanged();
            }
            break;
        }
        case MaterialPropertyType::Vec4:
        {
            glm::vec4 v = std::get<glm::vec4>(prop.value);
            if (ImGui::DragFloat4(propName.c_str(), &v.x, 0.01f))
            {
                mat->SetVec4Property(propName, v);
                if (onChanged)
                    onChanged();
            }
            break;
        }
        case MaterialPropertyType::Texture2D:
        case MaterialPropertyType::TextureCube:
        {
            std::shared_ptr<Texture> currentTex;
            if (std::holds_alternative<std::shared_ptr<Texture>>(prop.value))
                currentTex = std::get<std::shared_ptr<Texture>>(prop.value);
            int texItem = 0;
            for (size_t i = 1; i < textureItems.size(); ++i)
            {
                const auto it = textures.find(textureItems.keys[i]);
                if (it != textures.end() && it->second == currentTex)
                {
                    texItem = static_cast<int>(i);
                    break;
                }
            }
            if (DrawAssetPathCombo(propName.c_str(), &texItem, textureItems))
            {
                std::shared_ptr<Texture> newTex;
                if (texItem > 0 && static_cast<size_t>(texItem) < textureItems.size())
                    newTex = AssetManager::Get().GetAsset<Texture>(textureItems.keys[static_cast<size_t>(texItem)]);
                if (prop.type == MaterialPropertyType::Texture2D)
                    mat->SetTexture2DProperty(propName, newTex);
                else
                    mat->SetTextureCubeProperty(propName, newTex);
                if (onChanged)
                    onChanged();
            }
            break;
        }
        default:
            ImGui::TextUnformatted(propName.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("(unsupported type)");
            break;
        }
        ImGui::PopID();
    };

    for (const std::string& propName : propertyOrder)
        drawOneProperty(propName);
}

static void DrawMaterialShaderSelector(Material* mat, const std::function<void()>& onChanged = {})
{
    if (!mat)
        return;

    const auto& shaders = AssetManager::Get().m_shaders;
    AssetPickerItems shaderItems;
    {
        std::vector<std::string> shaderKeys;
        AssetManager::CollectUiVisibleKeys(shaders, shaderKeys, true);
        shaderItems.SetKeys(std::move(shaderKeys));
    }

    if (shaderItems.size() == 0)
    {
        ImGui::TextUnformatted("Shader: (no shaders loaded)");
        return;
    }

    std::shared_ptr<Shader> currentShader = mat->GetShader();
    int shaderItem = 0;
    if (currentShader)
    {
        for (size_t k = 0; k < shaderItems.size(); ++k)
        {
            if (shaderItems.keys[k] == currentShader->m_path)
            {
                shaderItem = static_cast<int>(k);
                break;
            }
        }
    }

    if (DrawAssetPathCombo("Shader", &shaderItem, shaderItems))
    {
        if (static_cast<size_t>(shaderItem) < shaderItems.size())
        {
            if (std::shared_ptr<Shader> newShader =
                    AssetManager::Get().GetAsset<Shader>(shaderItems.keys[static_cast<size_t>(shaderItem)]))
            {
                if (!currentShader || newShader.get() != currentShader.get())
                {
                    mat->SetShader(newShader);
                    if (onChanged)
                        onChanged();
                }
            }
        }
    }

    currentShader = mat->GetShader();
    if (currentShader)
    {
        ImGui::TextDisabled("Shader path: %s", currentShader->m_path.c_str());
        ImGui::Text("Shader queue: %d  |  Material queue: %d", currentShader->renderQueue, mat->GetRenderQueue());
    }
}

static bool SaveMaterialToMatFile(Material* mat, std::string& outMessage, const std::string& filePath = {});

static void DrawMeshRenderMaterialOverrides(MeshRender* mr, const std::shared_ptr<Model>& model)
{
    const auto& materials = AssetManager::Get().m_materials;

    AssetPickerItems materialItems;
    {
        std::vector<std::string> materialKeys;
        materialKeys.emplace_back("(Use Model Default)");
        std::vector<std::string> visible;
        AssetManager::CollectUiVisibleKeys(materials, visible, true);
        materialKeys.insert(materialKeys.end(), visible.begin(), visible.end());
        materialItems.SetKeys(std::move(materialKeys));
    }

    for (size_t i = 0; i < model->m_meshSections.size(); ++i)
    {
        const MeshSection& section = model->m_meshSections[i];
        const std::shared_ptr<Material> resolved = mr->GetMaterial(static_cast<int>(i));

        std::string headerLabel =
            section.name.empty() ? ("Section " + std::to_string(i)) : section.name;
        if (resolved && !resolved->m_name.empty() && headerLabel != resolved->m_name)
            headerLabel += " [" + resolved->m_name + "]";
        else if (resolved && !resolved->m_name.empty() && section.name.empty())
            headerLabel = resolved->m_name;

        const std::string header = headerLabel + " ##mat" + std::to_string(i);

        if (!ImGui::TreeNode(header.c_str()))
            continue;

        const std::shared_ptr<Material> overrideMat = mr->GetMaterialOverride(static_cast<int>(i));
        int matItem = 0;
        if (overrideMat)
        {
            for (size_t k = 1; k < materialItems.size(); ++k)
            {
                if (materialItems.keys[k] == overrideMat->m_sourcePath)
                {
                    matItem = static_cast<int>(k);
                    break;
                }
            }
        }

        if (DrawAssetPathCombo("Material Override", &matItem, materialItems))
        {
            if (matItem == 0)
                mr->ClearMaterialOverride(static_cast<int>(i));
            else if (matItem > 0 && static_cast<size_t>(matItem) < materialItems.size())
            {
                if (std::shared_ptr<Material> newMat =
                        AssetManager::Get().GetAsset<Material>(materialItems.keys[static_cast<size_t>(matItem)]))
                    mr->SetMaterial(static_cast<int>(i), newMat);
            }
            SceneSession::Get().MarkDirty();
        }

        if (resolved)
            ImGui::Text("Resolved: %s", resolved->m_name.c_str());

        Material* editableMat = overrideMat ? overrideMat.get() : resolved.get();
        if (editableMat)
        {
            ImGui::Separator();
            if (!overrideMat)
                ImGui::TextDisabled("No material override: shader change applies to shared/model material.");
            DrawMaterialShaderSelector(editableMat);
        }

        if (overrideMat)
        {
            ImGui::Separator();
            ImGui::TextUnformatted("Override Properties");
            DrawMaterialProperties(overrideMat.get());
        }
        else if (resolved)
        {
            ImGui::Separator();
            ImGui::TextUnformatted("Material Properties");
            DrawMaterialProperties(resolved.get());
        }

        Material* saveTarget = overrideMat ? overrideMat.get() : resolved.get();
        if (saveTarget)
        {
            ImGui::Separator();
            const std::string matPath = AssetDataBase::NormalizeSourcePath(saveTarget->m_sourcePath);
            const bool canSave = !matPath.empty() && IsMaterialAssetPath(matPath);
            ImGui::BeginDisabled(!canSave);
            if (ImGui::Button("Save to .mat", ImVec2(-FLT_MIN, 0.f)))
            {
                std::string msg;
                SaveMaterialToMatFile(saveTarget, msg);
            }
            ImGui::EndDisabled();
            if (canSave)
                ImGui::TextDisabled("%s", matPath.c_str());
            else
                ImGui::TextDisabled("Load material from a .mat file to enable saving.");
        }

        ImGui::TreePop();
    }
}

static bool SaveMaterialToMatFile(Material* mat, std::string& outMessage, const std::string& filePath)
{
    if (!mat)
    {
        outMessage = "No material to save.";
        return false;
    }

    const std::string path = filePath.empty() ? AssetDataBase::NormalizeSourcePath(mat->m_sourcePath)
                                              : AssetDataBase::NormalizeSourcePath(filePath);
    if (path.empty() || !IsMaterialAssetPath(path))
    {
        outMessage = "Material has no .mat file path.";
        return false;
    }

    MaterialFileDesc desc;
    if (!LoaderManager::Get().BuildMaterialFileDescFromMaterial(*mat, desc))
    {
        outMessage = "Failed to build material description.";
        return false;
    }

    desc.showInUi = mat->m_showInUi;
    if (!LoaderManager::Get().WriteMaterialFile(path, desc))
    {
        outMessage = "Failed to write: " + path;
        return false;
    }

    std::vector<std::string> dependencies;
    dependencies.push_back(desc.shaderPath);
    for (const auto& [propName, prop] : desc.properties)
    {
        (void)propName;
        if (prop.type != MaterialPropertyType::Texture2D && prop.type != MaterialPropertyType::TextureCube)
            continue;
        if (!prop.defaultTexturePath.empty())
            dependencies.push_back(prop.defaultTexturePath);
    }
    AssetDataBase::Get().UpdateDependencies(path, dependencies);
    AssetDataBase::Get().UpdateImportBool(path, "showInUi", desc.showInUi);

    outMessage = "Saved material: " + path;
    LogA(LogLevel::INFO, "{}", outMessage);
    return true;
}

} // namespace

void Gui::Init(Window* win)
{
    m_window = win;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

    // Install after window callbacks; ImGui chains into existing GLFW callbacks.
    ImGui_ImplGlfw_InitForOpenGL(win->win, true);
    ImGui_ImplOpenGL3_Init(Config::Get().GetGlslVersionString().c_str());
    LogA(LogLevel::INFO, "ImGui initialized (OpenGL3 + GLFW, docking enabled)");
}

void Gui::BeginFrame(bool gameInputExclusive)
{

    ImGuiIO& io = ImGui::GetIO();
    const ImGuiConfigFlags mask =
        ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoKeyboard | ImGuiConfigFlags_NoMouseCursorChange;
    if (gameInputExclusive)
        io.ConfigFlags |= mask;
    else
        io.ConfigFlags &= ~mask;

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

ImGuiID Gui::BuildWidgets(RenderContext& context)
{
    m_frameContext = &context;

    // Full-viewport dock; central node passthrough for 3D scene underneath.
    const ImGuiID dockId = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

    ShowMenu(context);
    ShowPreferences(context);
    ShowScenePathModals(context);
    ShowLog();
    ShowDetail();
    ShowOutline();
    ShowAssetBrowser(context);
    ShowScene();
#if GLE_ENABLE_RENDERDOC
    if (Config::Get().enableRenderDoc)
        ShowRenderDoc();
#endif

    m_frameContext = nullptr;
    return dockId;
}

void Gui::EndFrame()
{
    glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Pass_UI");
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glPopDebugGroup();
}

void Gui::Clean()
{
    LogA(LogLevel::INFO, "ImGui shutting down");
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Gui::ExecuteSceneAction(const PendingSceneAction& action, RenderContext& context)
{
    switch (action.kind)
    {
    case PendingSceneActionKind::NewScene:
        EntityManager::Get().NewEmptyScene();
        SceneSession::Get().Reset();
        item_selected_idx = -1;
        m_detailLastSelectedIdx = -1;
        ClearAssetDetail();
        LogA(LogLevel::INFO, "New empty scene.");
        break;
    case PendingSceneActionKind::LoadScenePath:
    {
        std::string msg;
        if (SceneSerializer::LoadScene(action.path, context, msg))
        {
            item_selected_idx = -1;
            m_detailLastSelectedIdx = -1;
            ClearAssetDetail();
            LogA(LogLevel::INFO, "{}", msg);
        }
        else
        {
            LogA(LogLevel::ERROR, "{}", msg);
        }
        break;
    }
    case PendingSceneActionKind::Exit:
        if (m_window)
            m_window->Close();
        break;
    case PendingSceneActionKind::None:
        break;
    }
}

void Gui::RequestSceneAction(const PendingSceneAction& action, RenderContext& context)
{
    if (SceneSession::Get().IsDirty())
    {
        m_pendingSceneAction = action;
        m_openDiscardSceneModal = true;
        return;
    }
    ExecuteSceneAction(action, context);
}

void Gui::ShowDiscardSceneModal(RenderContext& context)
{
    if (m_openDiscardSceneModal)
        ImGui::OpenPopup("DiscardSceneChangesModal");

    ImGui::SetNextWindowSize(ImVec2(420.f, 0.f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("DiscardSceneChangesModal", &m_openDiscardSceneModal,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Current scene has unsaved changes.");
        ImGui::TextUnformatted("Discard changes and continue?");
        if (ImGui::Button("Discard", ImVec2(120.f, 0.f)))
        {
            const PendingSceneAction action = m_pendingSceneAction;
            m_pendingSceneAction = {};
            m_openDiscardSceneModal = false;
            ExecuteSceneAction(action, context);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
        {
            m_pendingSceneAction = {};
            m_openDiscardSceneModal = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void Gui::ShowMenu(RenderContext& context)
{
    if (!ImGui::BeginMainMenuBar())
        return;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("New Scene"))
            RequestSceneAction({PendingSceneActionKind::NewScene}, context);
        if (ImGui::MenuItem("Save Scene"))
        {
            const std::string& currentPath = SceneSession::Get().GetCurrentPath();
            if (currentPath.empty())
            {
                std::snprintf(m_scenePathInput, sizeof(m_scenePathInput), "%s",
                              "Content/Project/scenes/Untitled.scene");
                m_openSaveSceneAsModal = true;
            }
            else
            {
                std::string msg;
                if (SceneSerializer::SaveScene(currentPath, context, msg))
                {
                    item_selected_idx = -1;
                    m_detailLastSelectedIdx = -1;
                    ClearAssetDetail();
                    LogA(LogLevel::INFO, "{}", msg);
                }
                else
                {
                    LogA(LogLevel::ERROR, "{}", msg);
                }
            }
        }
        if (ImGui::MenuItem("Save Scene As..."))
            m_openSaveSceneAsModal = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))
            RequestSceneAction({PendingSceneActionKind::Exit}, context);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Preferences..."))
        {
            LoadPreferencesFromConfig();
            m_openPreferences = true;
        }
        ImGui::EndMenu();
    }

    const std::string& scenePath = SceneSession::Get().GetCurrentPath();
    if (!scenePath.empty() || SceneSession::Get().IsDirty())
    {
        ImGui::Separator();
        if (scenePath.empty())
            ImGui::TextUnformatted("Scene: Untitled");
        else
            ImGui::Text("Scene: %s%s", scenePath.c_str(), SceneSession::Get().IsDirty() ? "*" : "");
    }

    ImGui::EndMainMenuBar();
}

namespace
{
std::string JoinHiddenExtensions(const std::vector<std::string>& extensions)
{
    std::string joined;
    for (size_t i = 0; i < extensions.size(); ++i)
    {
        if (i > 0)
            joined += ", ";
        joined += extensions[i];
    }
    return joined;
}

std::vector<std::string> ParseHiddenExtensions(const char* text)
{
    std::vector<std::string> result;
    std::string token;
    auto flush = [&]()
    {
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front())))
            token.erase(token.begin());
        while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back())))
            token.pop_back();
        if (token.empty())
            return;
        if (token.front() != '.')
            token.insert(token.begin(), '.');
        for (char& c : token)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        result.push_back(std::move(token));
        token.clear();
    };

    for (const char* p = text; *p; ++p)
    {
        if (*p == ',' || *p == ';' || *p == '\n' || *p == '\r')
        {
            flush();
            continue;
        }
        token.push_back(*p);
    }
    flush();
    if (result.empty())
        result.push_back(".meta");
    return result;
}
} // namespace

namespace
{
const char* RenderPipelineNameComboGetter(void* data, int idx)
{
    const auto* names = static_cast<const std::vector<std::string>*>(data);
    if (!names || idx < 0 || idx >= static_cast<int>(names->size()))
        return "";
    return (*names)[static_cast<size_t>(idx)].c_str();
}
} // namespace

void Gui::LoadPreferencesFromConfig()
{
    const Config& config = Config::Get();
    std::snprintf(m_prefRenderPipelineName, sizeof(m_prefRenderPipelineName), "%s", config.renderPipeline.c_str());
    m_prefRenderPipelineIdx = 0;
    const std::vector<std::string> pipelineNames = RenderPipelineRegistry::GetRegisteredNames();
    for (int i = 0; i < static_cast<int>(pipelineNames.size()); ++i)
    {
        if (pipelineNames[static_cast<size_t>(i)] == config.renderPipeline)
        {
            m_prefRenderPipelineIdx = i;
            break;
        }
    }
    m_prefOpenGLMajor = config.openGLMajor;
    m_prefOpenGLMinor = config.openGLMinor;
    m_prefVsync = config.vsync;
    m_prefEnableRenderDoc = config.enableRenderDoc;
    m_prefShowRenderDocOverlay = config.showRenderDocOverlay;
    std::snprintf(m_prefRenderDocPath, sizeof(m_prefRenderDocPath), "%s", config.renderDocPath.c_str());
    m_prefInitialWidth = config.initialWidth;
    m_prefInitialHeight = config.initialHeight;

    std::snprintf(m_prefProjectRoot, sizeof(m_prefProjectRoot), "%s", config.projectRoot.c_str());
    std::snprintf(m_prefProjectContentRoot, sizeof(m_prefProjectContentRoot), "%s",
                  config.projectContentRoot.c_str());
    std::snprintf(m_prefEngineContentRoot, sizeof(m_prefEngineContentRoot), "%s",
                  config.engineContentRoot.c_str());
    std::snprintf(m_prefStartupScene, sizeof(m_prefStartupScene), "%s", config.startupScene.c_str());
    std::snprintf(m_prefHiddenExtensions, sizeof(m_prefHiddenExtensions), "%s",
                  JoinHiddenExtensions(config.assetBrowserHiddenExtensions).c_str());

    m_prefNeedsRestart = false;
    m_prefStatus.clear();
}

void Gui::ApplyPreferences(bool saveToDisk)
{
    Config& config = Config::Get();
    const std::string beforeRenderPipeline = config.renderPipeline;
    const int beforeOpenGLMajor = config.openGLMajor;
    const int beforeOpenGLMinor = config.openGLMinor;
    const bool beforeEnableRenderDoc = config.enableRenderDoc;
    const std::string beforeRenderDocPath = config.renderDocPath;
    const int beforeInitialWidth = config.initialWidth;
    const int beforeInitialHeight = config.initialHeight;
    const std::string beforeProjectRoot = config.projectRoot;
    const std::string beforeProjectContentRoot = config.projectContentRoot;
    const std::string beforeEngineContentRoot = config.engineContentRoot;
    const std::vector<std::string> beforeHiddenExtensions = config.assetBrowserHiddenExtensions;

    m_prefInitialWidth = std::max(320, m_prefInitialWidth);
    m_prefInitialHeight = std::max(240, m_prefInitialHeight);
    m_prefOpenGLMajor = std::max(3, m_prefOpenGLMajor);
    m_prefOpenGLMinor = std::clamp(m_prefOpenGLMinor, 0, 9);

    config.renderPipeline = RenderPipelineRegistry::ResolveName(m_prefRenderPipelineName);
    std::snprintf(m_prefRenderPipelineName, sizeof(m_prefRenderPipelineName), "%s", config.renderPipeline.c_str());
    config.openGLMajor = m_prefOpenGLMajor;
    config.openGLMinor = m_prefOpenGLMinor;
    config.vsync = m_prefVsync;
    config.enableRenderDoc = m_prefEnableRenderDoc;
    config.showRenderDocOverlay = m_prefShowRenderDocOverlay;
    config.renderDocPath = m_prefRenderDocPath;
    config.initialWidth = m_prefInitialWidth;
    config.initialHeight = m_prefInitialHeight;
    config.projectRoot = m_prefProjectRoot;
    config.projectContentRoot = m_prefProjectContentRoot;
    config.engineContentRoot = m_prefEngineContentRoot;
    config.startupScene = m_prefStartupScene;
    config.assetBrowserHiddenExtensions = ParseHiddenExtensions(m_prefHiddenExtensions);

    if (saveToDisk)
        config.SaveConfig();

    if (m_window && m_window->win)
        glfwSwapInterval(config.vsync ? 1 : 0);

    const bool pathsChanged = beforeProjectRoot != config.projectRoot ||
                              beforeProjectContentRoot != config.projectContentRoot ||
                              beforeEngineContentRoot != config.engineContentRoot;
    if (pathsChanged)
    {
        AssetPaths::Init();
        std::snprintf(m_assetBrowserPath, sizeof(m_assetBrowserPath), "%s",
                      AssetPaths::Get().GetProjectContentRoot().c_str());
        RefreshAssetBrowserListing();
    }
    else if (beforeHiddenExtensions != config.assetBrowserHiddenExtensions)
    {
        RefreshAssetBrowserListing();
    }

    if (beforeInitialWidth != config.initialWidth || beforeInitialHeight != config.initialHeight)
    {
        if (m_window && m_window->win)
        {
            glfwSetWindowSize(m_window->win, config.initialWidth, config.initialHeight);
            m_window->vWidth = config.initialWidth;
            m_window->vHeight = config.initialHeight;
        }
    }

#if GLE_ENABLE_RENDERDOC
    if (beforeEnableRenderDoc != config.enableRenderDoc || beforeRenderDocPath != config.renderDocPath)
    {
        RenderDocSupport::Reload();
        if (config.enableRenderDoc && m_window && m_window->win)
            RenderDocSupport::SetActiveWindow(m_window->win);
    }
    else
    {
        RenderDocSupport::ApplyOverlaySettings();
    }
#endif

    m_prefNeedsRestart = beforeRenderPipeline != config.renderPipeline ||
                         beforeOpenGLMajor != config.openGLMajor || beforeOpenGLMinor != config.openGLMinor;

    if (m_prefNeedsRestart)
        m_prefStatus =
            "Some settings were saved; restart the editor for render pipeline or OpenGL changes.";
    else if (saveToDisk)
        m_prefStatus = "Settings saved.";
    else
        m_prefStatus = "Settings applied.";

    LogA(LogLevel::INFO, "{}", m_prefStatus);
}

void Gui::ShowPreferences(RenderContext& context)
{
    (void)context;

    if (m_openPreferences)
        ImGui::OpenPopup("PreferencesModal");

    ImGui::SetNextWindowSize(ImVec2(520.f, 0.f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("PreferencesModal", &m_openPreferences))
        return;

    constexpr float kPrefContentWidth = 480.f;
    ImGui::PushItemWidth(kPrefContentWidth);

    if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
    {
        const std::vector<std::string> pipelineNames = RenderPipelineRegistry::GetRegisteredNames();
        if (!pipelineNames.empty())
        {
            m_prefRenderPipelineIdx =
                std::clamp(m_prefRenderPipelineIdx, 0, static_cast<int>(pipelineNames.size()) - 1);
            if (ImGui::Combo("Render Pipeline", &m_prefRenderPipelineIdx, RenderPipelineNameComboGetter,
                             const_cast<void*>(static_cast<const void*>(&pipelineNames)),
                             static_cast<int>(pipelineNames.size())))
            {
                std::snprintf(m_prefRenderPipelineName, sizeof(m_prefRenderPipelineName), "%s",
                              pipelineNames[static_cast<size_t>(m_prefRenderPipelineIdx)].c_str());
            }
        }
        else
        {
            ImGui::InputText("Render Pipeline", m_prefRenderPipelineName, IM_ARRAYSIZE(m_prefRenderPipelineName));
        }
        ImGui::InputInt("OpenGL Major", &m_prefOpenGLMajor);
        ImGui::InputInt("OpenGL Minor", &m_prefOpenGLMinor);
        ImGui::Checkbox("VSync", &m_prefVsync);
#if GLE_ENABLE_RENDERDOC
        ImGui::Checkbox("Enable RenderDoc", &m_prefEnableRenderDoc);
        ImGui::Checkbox("Show RenderDoc Overlay", &m_prefShowRenderDocOverlay);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("FPS, frame number, and capture list in the top-left when RenderDoc is attached.");
        ImGui::InputText("RenderDoc Path", m_prefRenderDocPath, IM_ARRAYSIZE(m_prefRenderDocPath));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Path to renderdoc.dll or RenderDoc install folder.\n"
                              "Leave empty to use default install locations.");
#else
        ImGui::BeginDisabled();
        bool renderDocDisabled = m_prefEnableRenderDoc;
        ImGui::Checkbox("Enable RenderDoc", &renderDocDisabled);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("RenderDoc support is not enabled in this build.");
#endif
    }

    if (ImGui::CollapsingHeader("Window", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::InputInt("Initial Width", &m_prefInitialWidth);
        ImGui::InputInt("Initial Height", &m_prefInitialHeight);
    }

    if (ImGui::CollapsingHeader("Project", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::InputText("Project Root", m_prefProjectRoot, IM_ARRAYSIZE(m_prefProjectRoot));
        ImGui::TextDisabled("Leave empty to use the engine root directory.");
        ImGui::InputText("Project Content Root", m_prefProjectContentRoot,
                         IM_ARRAYSIZE(m_prefProjectContentRoot));
        ImGui::InputText("Engine Content Root", m_prefEngineContentRoot, IM_ARRAYSIZE(m_prefEngineContentRoot));
        ImGui::InputText("Startup Scene", m_prefStartupScene, IM_ARRAYSIZE(m_prefStartupScene));
        ImGui::TextDisabled("Relative to projectRoot; empty loads Default.scene on startup.");
    }

    if (ImGui::CollapsingHeader("Asset Browser", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::InputText("Hidden Extensions", m_prefHiddenExtensions, IM_ARRAYSIZE(m_prefHiddenExtensions));
        ImGui::TextDisabled("Comma or newline separated, e.g. .meta, .tmp");
    }

    ImGui::PopItemWidth();

    if (!m_prefStatus.empty())
    {
        ImGui::PushTextWrapPos(ImGui::GetCursorStartPos().x + kPrefContentWidth);
        if (m_prefNeedsRestart)
            ImGui::TextColored(ImVec4(1.f, 0.75f, 0.2f, 1.f), "%s", m_prefStatus.c_str());
        else
            ImGui::TextColored(ImVec4(0.4f, 1.f, 0.5f, 1.f), "%s", m_prefStatus.c_str());
        ImGui::PopTextWrapPos();
    }

    if (ImGui::Button("Apply", ImVec2(100.f, 0.f)))
        ApplyPreferences(true);
    ImGui::SameLine();
    if (ImGui::Button("Save", ImVec2(100.f, 0.f)))
    {
        ApplyPreferences(true);
        m_openPreferences = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.f, 0.f)))
    {
        LoadPreferencesFromConfig();
        m_openPreferences = false;
    }

    ImGui::EndPopup();
}

void Gui::ShowScenePathModals(RenderContext& context)
{
    ShowDiscardSceneModal(context);

    if (m_openSaveSceneAsModal)
        ImGui::OpenPopup("SaveSceneAsModal");
    if (m_openCreateSceneModal)
        ImGui::OpenPopup("CreateSceneModal");

    ImGui::SetNextWindowSize(ImVec2(420.f, 0.f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("SaveSceneAsModal", &m_openSaveSceneAsModal, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Scene Path", m_scenePathInput, IM_ARRAYSIZE(m_scenePathInput));
        if (ImGui::Button("Save", ImVec2(120.f, 0.f)))
        {
            std::string msg;
            if (SceneSerializer::SaveScene(m_scenePathInput, context, msg))
            {
                item_selected_idx = -1;
                m_detailLastSelectedIdx = -1;
                ClearAssetDetail();
                m_openSaveSceneAsModal = false;
                LogA(LogLevel::INFO, "{}", msg);
            }
            else
            {
                LogA(LogLevel::ERROR, "{}", msg);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
            m_openSaveSceneAsModal = false;
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(360.f, 0.f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("CreateSceneModal", &m_openCreateSceneModal, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Create scene in:");
        ImGui::TextWrapped("%s", m_assetBrowserPath);
        ImGui::InputText("Scene Name", m_sceneCreateName, IM_ARRAYSIZE(m_sceneCreateName));

        if (ImGui::Button("Create", ImVec2(120.f, 0.f)))
        {
            std::string createdPath;
            std::string msg;
            if (SceneSerializer::CreateEmptySceneFile(m_assetBrowserPath, m_sceneCreateName, createdPath, msg))
            {
                RefreshAssetBrowserListing();
                ClearAssetBrowserSelection();
                for (int i = 0; i < static_cast<int>(m_assetBrowserPaths.size()); ++i)
                {
                    if (m_assetBrowserPaths[static_cast<size_t>(i)] == createdPath)
                    {
                        m_assetBrowserSelection.SetItemSelected(static_cast<ImGuiID>(i), true);
                        OpenAssetDetail(createdPath);
                        break;
                    }
                }
                m_assetBrowserStatus = std::move(msg);
                m_openCreateSceneModal = false;
            }
            else
            {
                m_assetBrowserStatus = std::move(msg);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
            m_openCreateSceneModal = false;
        ImGui::EndPopup();
    }
}
void Gui::ShowLog()
{
    LogEntry logOut;
    while (LogManager::Get().GetALog(logOut))
        logWindow.AppendLog(logOut.text, logOut.level);

    logWindow.Draw("Log");
}
void Gui::OpenAssetDetail(const std::string& path)
{
    m_assetDetailPath = AssetDataBase::Get().ResolveAssetRef(path);
    if (m_assetDetailPath.empty())
        m_assetDetailPath = AssetDataBase::NormalizeSourcePath(path);
    AssetDataBase::Get().RefreshMetaFromDisk(m_assetDetailPath);
    m_assetDetailMeta = AssetDataBase::Get().LoadOrCreateMeta(m_assetDetailPath);
    if (m_assetDetailMeta.type == AssetType::TextureCube)
    {
        if (AssetDataBase::EnsureCubemapImportSettings(m_assetDetailMeta, m_assetDetailPath))
            AssetDataBase::Get().SaveMeta(m_assetDetailMeta);
    }
    else if (m_assetDetailMeta.type == AssetType::Texture2D)
    {
        if (AssetDataBase::EnsureTextureImportSettings(m_assetDetailMeta, m_assetDetailPath))
            AssetDataBase::Get().SaveMeta(m_assetDetailMeta);
    }
    m_detailViewMode = DetailViewMode::Asset;

    for (size_t i = 0; i < m_assetBrowserPaths.size(); ++i)
    {
        if (AssetDataBase::NormalizeSourcePath(m_assetBrowserPaths[i]) ==
            AssetDataBase::NormalizeSourcePath(m_assetDetailPath))
        {
            m_assetBrowserLastSyncedDetailIndices = {static_cast<int>(i)};
            break;
        }
    }

    ReloadAssetDetailMaterial();
}

void Gui::ClearAssetDetail()
{
    m_assetDetailPath.clear();
    m_assetDetailMeta = {};
    m_assetDetailEditMaterial.reset();
    m_assetDetailMaterialFileShowInUi = true;
    if (m_detailViewMode == DetailViewMode::Asset)
        m_detailViewMode = DetailViewMode::Entity;
}

void Gui::ReloadAssetDetailMaterial()
{
    m_assetDetailEditMaterial.reset();
    m_assetDetailMaterialFileShowInUi = true;
    if (m_assetDetailPath.empty() || !IsMaterialAssetPath(m_assetDetailPath))
        return;

    MaterialFileDesc desc;
    if (LoaderManager::Get().ParseMaterialFile(m_assetDetailPath, desc))
        m_assetDetailMaterialFileShowInUi = desc.showInUi;

    if (AssetManager::Get().FindLoadedMaterialBySourcePath(m_assetDetailPath))
        return;

    m_assetDetailEditMaterial = AssetManager::Get().BuildMaterialFromFile(m_assetDetailPath);
    if (m_assetDetailEditMaterial)
        m_assetDetailEditMaterial->m_sourcePath = AssetDataBase::NormalizeSourcePath(m_assetDetailPath);
}

void Gui::SaveAssetDetailMaterial()
{
    if (m_assetDetailPath.empty() || !IsMaterialAssetPath(m_assetDetailPath))
        return;

    Material* mat = nullptr;
    if (std::shared_ptr<Material> loaded = AssetManager::Get().FindLoadedMaterialBySourcePath(m_assetDetailPath))
        mat = loaded.get();
    else if (m_assetDetailEditMaterial)
        mat = m_assetDetailEditMaterial.get();
    if (!mat)
        return;

    std::string msg;
    if (!SaveMaterialToMatFile(mat, msg, m_assetDetailPath))
        return;

    m_assetDetailMaterialFileShowInUi = mat->m_showInUi;
    AssetDataBase::Get().TryGetMeta(m_assetDetailPath, m_assetDetailMeta);
}

static void SyncLoadedAssetShowInUi(const std::string& sourcePath, bool showInUi)
{
    const std::string path = AssetDataBase::NormalizeSourcePath(sourcePath);
    AssetManager& assets = AssetManager::Get();
    const auto syncMap = [&](const auto& map)
    {
        for (const auto& [name, asset] : map)
        {
            if (asset && AssetDataBase::NormalizeSourcePath(asset->m_path) == path)
                asset->m_showInUi = showInUi;
        }
    };
    syncMap(assets.m_shaders);
    syncMap(assets.m_textures);
    syncMap(assets.m_models);
    syncMap(assets.m_iblImages);
}

static AssetPickerItems CollectMaterialPickerItems()
{
    AssetPickerItems items;
    std::vector<std::string> keys;
    keys.emplace_back("(None)");
    std::vector<std::string> visible;
    AssetManager::CollectUiVisibleKeys(AssetManager::Get().m_materials, visible, true);
    keys.insert(keys.end(), visible.begin(), visible.end());
    items.SetKeys(std::move(keys));
    return items;
}

static std::string MaterialPathFromPickerItem(const AssetPickerItems& items, int item)
{
    if (item <= 0 || static_cast<size_t>(item) >= items.keys.size())
        return {};
    return items.keys[static_cast<size_t>(item)];
}

static int FindMaterialPickerIndex(const AssetPickerItems& items, const std::string& path)
{
    if (path.empty())
        return 0;

    const std::string normalized =
        AssetDataBase::NormalizeSourcePath(AssetManager::ResolveAssetPath(path));
    for (size_t k = 0; k < items.keys.size(); ++k)
    {
        if (!items.keys[k].empty() &&
            AssetDataBase::NormalizeSourcePath(items.keys[k]) == normalized)
            return static_cast<int>(k);
    }
    return 0;
}

static std::string GetModelMaterialSlotPath(const AssetMeta& meta, int slot, const std::string& slotName)
{
    if (meta.importSettings.is_object() && meta.importSettings.contains("materialSlots") &&
        meta.importSettings["materialSlots"].is_array())
    {
        for (const nlohmann::json& entry : meta.importSettings["materialSlots"])
        {
            if (!entry.is_object() || !entry.contains("material") || !entry["material"].is_string())
                continue;
            if (entry.contains("slot") && entry["slot"].is_number_integer() && entry["slot"].get<int>() == slot)
                return entry["material"].get<std::string>();
            if (entry.contains("name") && entry["name"].is_string() && entry["name"].get<std::string>() == slotName)
                return entry["material"].get<std::string>();
        }
    }
    return AssetDataBase::GetImportString(meta, "defaultMaterial");
}

static void SetModelMaterialSlot(AssetMeta& meta, int slot, const std::string& slotName, const std::string& materialPath)
{
    if (!meta.importSettings.is_object())
        meta.importSettings = nlohmann::json::object();

    nlohmann::json newSlots = nlohmann::json::array();
    if (meta.importSettings.contains("materialSlots") && meta.importSettings["materialSlots"].is_array())
    {
        for (const nlohmann::json& entry : meta.importSettings["materialSlots"])
        {
            if (!entry.is_object())
                continue;
            if (entry.contains("slot") && entry["slot"].is_number_integer() && entry["slot"].get<int>() == slot)
                continue;
            if (entry.contains("name") && entry["name"].is_string() && entry["name"].get<std::string>() == slotName &&
                !slotName.empty())
                continue;
            newSlots.push_back(entry);
        }
    }

    if (!materialPath.empty())
    {
        nlohmann::json entry;
        entry["slot"] = slot;
        if (!slotName.empty())
            entry["name"] = slotName;
        entry["material"] = materialPath;
        newSlots.push_back(entry);
    }

    if (newSlots.empty())
        meta.importSettings.erase("materialSlots");
    else
        meta.importSettings["materialSlots"] = newSlots;
}

static void DrawModelImportMaterials(AssetMeta& meta, const std::string& assetPath)
{
    const AssetPickerItems materialItems = CollectMaterialPickerItems();

    const std::string defaultMat = AssetDataBase::GetImportString(meta, "defaultMaterial");
    int defaultItem = FindMaterialPickerIndex(materialItems, defaultMat);
    if (DrawAssetPathCombo("Default Material", &defaultItem, materialItems))
    {
        AssetDataBase::SetImportString(meta, "defaultMaterial",
                                         MaterialPathFromPickerItem(materialItems, defaultItem));
        AssetDataBase::Get().SaveMeta(meta);
        if (std::shared_ptr<Model> model = AssetManager::Get().GetAsset<Model>(assetPath))
            AssetManager::Get().ApplyModelMaterials(model);
    }

    const std::shared_ptr<Model> model = AssetManager::Get().GetAsset<Model>(assetPath);
    if (!model)
    {
        ImGui::TextDisabled("Load model to configure per-slot materials.");
        return;
    }

    if (model->m_sourceMaterials.empty())
        return;

    ImGui::Separator();
    ImGui::TextUnformatted("Material Slots");
    for (size_t i = 0; i < model->m_sourceMaterials.size(); ++i)
    {
        const SourceMaterialInfo& src = model->m_sourceMaterials[i];
        const std::string slotPath = GetModelMaterialSlotPath(meta, static_cast<int>(i), src.name);
        int slotItem = FindMaterialPickerIndex(materialItems, slotPath);

        const std::string label = src.name.empty() ? ("Slot " + std::to_string(i)) : src.name;
        if (DrawAssetPathCombo(label.c_str(), &slotItem, materialItems))
        {
            SetModelMaterialSlot(meta, static_cast<int>(i), src.name,
                                 MaterialPathFromPickerItem(materialItems, slotItem));
            AssetDataBase::Get().SaveMeta(meta);
            AssetManager::Get().ApplyModelMaterials(model);
        }
        if (!src.diffusePath.empty())
            ImGui::TextDisabled("FBX source: %s", src.diffusePath.c_str());
    }
}

static void ReloadTextureFromImportSettings(const std::string& path, AssetType type, bool forceReload = true)
{
    if (type == AssetType::TextureCube)
        AssetManager::Get().LoadIBL(path, forceReload);
    else
        AssetManager::Get().LoadTexture(path, forceReload);
}

void Gui::ShowAssetDetail()
{
    ImGui::Text("Source: %s", m_assetDetailMeta.sourcePath.c_str());
    ImGui::Text("GUID: %s", m_assetDetailMeta.guid.c_str());

    const char* typeLabels[] = {"Model", "Texture2D", "TextureCube", "Material", "Shader", "Scene"};
    int typeIdx = static_cast<int>(m_assetDetailMeta.type);
    if (typeIdx < 0 || typeIdx >= IM_ARRAYSIZE(typeLabels))
        typeIdx = static_cast<int>(AssetType::Texture2D);
    if (ImGui::Combo("Asset Type", &typeIdx, typeLabels, IM_ARRAYSIZE(typeLabels)))
    {
        m_assetDetailMeta.type = static_cast<AssetType>(typeIdx);
        if (m_assetDetailMeta.type == AssetType::TextureCube)
            AssetDataBase::EnsureCubemapImportSettings(m_assetDetailMeta, m_assetDetailPath);
        else if (m_assetDetailMeta.type == AssetType::Texture2D)
            AssetDataBase::EnsureTextureImportSettings(m_assetDetailMeta, m_assetDetailPath);
        AssetDataBase::Get().SaveMeta(m_assetDetailMeta);
    }

    bool showInUi = AssetDataBase::GetImportBool(m_assetDetailMeta, "showInUi", true);
    if (ImGui::Checkbox("Show In UI", &showInUi))
    {
        AssetDataBase::SetImportBool(m_assetDetailMeta, "showInUi", showInUi);
        AssetDataBase::Get().SaveMeta(m_assetDetailMeta);
        SyncLoadedAssetShowInUi(m_assetDetailPath, showInUi);
    }

    const bool textureLike =
        m_assetDetailMeta.type == AssetType::Texture2D || m_assetDetailMeta.type == AssetType::TextureCube;
    if (textureLike)
    {
        bool srgb = AssetDataBase::GetImportBool(m_assetDetailMeta, "srgb", true);
        if (ImGui::Checkbox("sRGB", &srgb))
        {
            AssetDataBase::SetImportBool(m_assetDetailMeta, "srgb", srgb);
            AssetDataBase::Get().SaveMeta(m_assetDetailMeta);
            ReloadTextureFromImportSettings(m_assetDetailPath, m_assetDetailMeta.type);
        }

        const bool defaultGenerateMips =
            AssetDataBase::DefaultTextureGenerateMips(m_assetDetailMeta.type, m_assetDetailPath);
        bool generateMips =
            AssetDataBase::GetImportBool(m_assetDetailMeta, "generateMips", defaultGenerateMips);
        const char* mipLabel =
            m_assetDetailMeta.type == AssetType::TextureCube ? "Prefilter Mip Chain" : "Generate Mips";
        if (ImGui::Checkbox(mipLabel, &generateMips))
        {
            AssetDataBase::SetImportBool(m_assetDetailMeta, "generateMips", generateMips);
            AssetDataBase::Get().SaveMeta(m_assetDetailMeta);
            ReloadTextureFromImportSettings(m_assetDetailPath, m_assetDetailMeta.type);
        }
        if (m_assetDetailMeta.type == AssetType::TextureCube && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("为镜面 IBL 烘焙 prefilter cubemap mip 链");

        if (m_assetDetailMeta.type == AssetType::TextureCube)
        {
            ImGui::Separator();
            ImGui::TextUnformatted("IBL Bake");

            int phiSamples = AssetDataBase::GetImportInt(m_assetDetailMeta, "kPhiSamples", 64);
            int thetaSamples = AssetDataBase::GetImportInt(m_assetDetailMeta, "kThetaSamples", 32);
            int irradianceFaceSize = AssetDataBase::GetImportInt(m_assetDetailMeta, "kIrradianceFaceSize", 32);
            int prefilterFaceSize = AssetDataBase::GetImportInt(m_assetDetailMeta, "kPrefilterFaceSize", 0);
            int minPrefilterFaceSize = AssetDataBase::GetImportInt(m_assetDetailMeta, "kMinPrefilterFaceSize", 512);

            if (ImGui::DragInt("Irradiance Phi Samples", &phiSamples, 1.0f, 1, 4096))
            {
                AssetDataBase::SetImportInt(m_assetDetailMeta, "kPhiSamples", phiSamples);
                AssetDataBase::Get().SaveMeta(m_assetDetailMeta);
                ReloadTextureFromImportSettings(m_assetDetailPath, m_assetDetailMeta.type);
            }
            if (ImGui::DragInt("Irradiance Theta Samples", &thetaSamples, 1.0f, 1, 4096))
            {
                AssetDataBase::SetImportInt(m_assetDetailMeta, "kThetaSamples", thetaSamples);
                AssetDataBase::Get().SaveMeta(m_assetDetailMeta);
                ReloadTextureFromImportSettings(m_assetDetailPath, m_assetDetailMeta.type);
            }
            if (ImGui::DragInt("Irradiance Face Size", &irradianceFaceSize, 1.0f, 8, 512))
            {
                AssetDataBase::SetImportInt(m_assetDetailMeta, "kIrradianceFaceSize", irradianceFaceSize);
                AssetDataBase::Get().SaveMeta(m_assetDetailMeta);
                ReloadTextureFromImportSettings(m_assetDetailPath, m_assetDetailMeta.type);
            }
            if (ImGui::DragInt("Prefilter Face Size", &prefilterFaceSize, 1.0f, 0, 4096))
            {
                AssetDataBase::SetImportInt(m_assetDetailMeta, "kPrefilterFaceSize", prefilterFaceSize);
                AssetDataBase::Get().SaveMeta(m_assetDetailMeta);
                ReloadTextureFromImportSettings(m_assetDetailPath, m_assetDetailMeta.type);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("0 = 根据 HDR 尺寸自动推算");
            if (ImGui::DragInt("Min Prefilter Face Size", &minPrefilterFaceSize, 1.0f, 64, 4096))
            {
                AssetDataBase::SetImportInt(m_assetDetailMeta, "kMinPrefilterFaceSize", minPrefilterFaceSize);
                AssetDataBase::Get().SaveMeta(m_assetDetailMeta);
                ReloadTextureFromImportSettings(m_assetDetailPath, m_assetDetailMeta.type);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("自动推算 prefilter 边长时的下限（2 的幂）");
        }
    }

    if (m_assetDetailMeta.type == AssetType::Model)
    {
        ImGui::Separator();
        ImGui::TextUnformatted("Model Import");
        DrawModelImportMaterials(m_assetDetailMeta, m_assetDetailPath);
    }

    if (!m_assetDetailMeta.dependencies.empty())
    {
        ImGui::Separator();
        ImGui::Text("Dependencies (%zu)", m_assetDetailMeta.dependencies.size());
        if (ImGui::BeginChild("AssetMetaDeps", ImVec2(0, 80), ImGuiChildFlags_Borders))
        {
            for (const std::string& dep : m_assetDetailMeta.dependencies)
                ImGui::BulletText("%s", dep.c_str());
        }
        ImGui::EndChild();
    }

    if (IsMaterialAssetPath(m_assetDetailPath))
    {
        Material* mat = nullptr;
        if (std::shared_ptr<Material> loaded = AssetManager::Get().FindLoadedMaterialBySourcePath(m_assetDetailPath))
            mat = loaded.get();
        else if (m_assetDetailEditMaterial)
            mat = m_assetDetailEditMaterial.get();

        if (mat)
        {
            const std::function<void()> saveMaterial = [this]() { SaveAssetDetailMaterial(); };

            ImGui::Separator();
            ImGui::TextUnformatted("Material");
            DrawMaterialShaderSelector(mat, saveMaterial);

            bool overrideQueue = mat->HasRenderQueueOverride();
            if (ImGui::Checkbox("Override Render Queue", &overrideQueue))
            {
                if (overrideQueue)
                {
                    if (std::shared_ptr<Shader> shader = mat->GetShader())
                        mat->SetRenderQueue(shader->renderQueue);
                }
                else
                    mat->ClearRenderQueueOverride();
                saveMaterial();
            }
            if (mat->HasRenderQueueOverride())
            {
                int queue = mat->GetRenderQueue();
                if (ImGui::InputInt("Render Queue", &queue))
                {
                    mat->SetRenderQueue(queue);
                    saveMaterial();
                }
            }

            ImGui::Separator();
            ImGui::TextUnformatted("Material Properties");
            DrawMaterialProperties(mat, saveMaterial);
        }
        else
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Failed to load material for editing.");
        }
    }

    const AssetEntryKind kind = ClassifyAssetEntry(std::filesystem::path(m_assetDetailPath));
    const int batchLoadable =
        CountLoadableAssetBrowserIndices(m_assetBrowserSelectedIndices, m_assetBrowserKinds, m_assetBrowserPaths.size());
    const bool batchLoad = m_assetBrowserSelectedIndices.size() > 1 && batchLoadable > 0;

    ImGui::Separator();
    ImGui::BeginDisabled(batchLoad ? batchLoadable == 0 : !CanLoadAssetEntry(kind));
    const std::string loadButtonLabel =
        batchLoad ? ("Load Selected (" + std::to_string(batchLoadable) + ")") : "Load";
    if (ImGui::Button(loadButtonLabel.c_str(), ImVec2(-FLT_MIN, 0.f)))
    {
        std::string msg;
        if (batchLoad)
            LoadAssetBrowserEntriesByIndices(m_assetBrowserSelectedIndices, m_assetBrowserPaths, m_assetBrowserKinds,
                                             msg);
        else if (kind == AssetEntryKind::Scene && m_frameContext)
        {
            PendingSceneAction action;
            action.kind = PendingSceneActionKind::LoadScenePath;
            action.path = m_assetDetailPath;
            RequestSceneAction(action, *m_frameContext);
        }
        else
        {
            LoadAssetEntry(kind, m_assetDetailPath, msg, m_frameContext);
            AssetDataBase::Get().TryGetMeta(m_assetDetailPath, m_assetDetailMeta);
            if (kind == AssetEntryKind::Material)
                ReloadAssetDetailMaterial();
        }
        if (!msg.empty())
            m_assetBrowserStatus = std::move(msg);
    }
    ImGui::EndDisabled();
    if (!m_assetBrowserStatus.empty())
        ImGui::TextWrapped("%s", m_assetBrowserStatus.c_str());
}

void Gui::ShowDetail()
{
    if (!ImGui::Begin("detail"))
    {
        ImGui::End();
        return;
    }

    if (m_detailViewMode == DetailViewMode::Asset && !m_assetDetailPath.empty())
    {
        ShowAssetDetail();
        ImGui::End();
        return;
    }

    const auto& entities = EntityManager::Get().GetEntities();
    const size_t count = entities.size();
    if (count == 0)
    {
        ImGui::TextUnformatted("No entities in scene.");
        ImGui::End();
        return;
    }

    int idx = item_selected_idx;
    if (idx < 0 || static_cast<size_t>(idx) >= count)
        idx = static_cast<int>(count - 1);
    item_selected_idx = idx;

    const std::shared_ptr<Entity>& ent = entities[static_cast<size_t>(idx)];
    if (!ent)
    {
        ImGui::TextUnformatted("Invalid entity pointer.");
        ImGui::End();
        return;
    }

    if (m_detailLastSelectedIdx != item_selected_idx)
    {
        m_detailLastSelectedIdx = item_selected_idx;
        std::snprintf(m_detailNameBuf, sizeof(m_detailNameBuf), "%s", ent->m_name.c_str());
    }

    ImGui::InputText("Name", m_detailNameBuf, IM_ARRAYSIZE(m_detailNameBuf));
    if (ent->m_name != m_detailNameBuf)
    {
        ent->m_name = m_detailNameBuf;
        SceneSession::Get().MarkDirty();
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Components:");

    if (Transform* tr = ent->GetComponent<Transform>())
    {
        ImGui::Separator();
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            glm::vec3 pos = tr->GetPosition();
            if (ImGui::DragFloat3("Position", &pos.x, 0.05f))
            {
                tr->SetPosition(pos);
                SceneSession::Get().MarkDirty();
            }
            glm::vec3 rot = tr->GetRotation();
            if (ImGui::DragFloat3("Rotation (deg)", &rot.x, 0.5f))
            {
                tr->SetRotation(rot);
                SceneSession::Get().MarkDirty();
            }
            glm::vec3 scale = tr->GetScale();
            if (ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.001f, 1000.f))
            {
                tr->SetScale(scale);
                SceneSession::Get().MarkDirty();
            }
        }
    }

    if (Camera* cam = ent->GetComponent<Camera>())
    {
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
        {
            float fov = cam->GetFov();
            if (ImGui::SliderFloat("FOV", &fov, 10.f, 120.f))
            {
                cam->SetFov(fov);
                SceneSession::Get().MarkDirty();
            }
            ImGui::Text("Clip near %.2f  far %.2f", cam->GetNear(), cam->GetFar());
        }
        if (ImGui::CollapsingHeader("Post Process", ImGuiTreeNodeFlags_DefaultOpen))
        {
            PostProcessSetting& setting = cam->postSetting;
            CameraImagingSettings& imaging = cam->imaging;
            bool imagingChanged = false;

            if (ImGui::CollapsingHeader("Physical Camera", ImGuiTreeNodeFlags_DefaultOpen))
            {
                float aperture = imaging.aperture;
                if (ImGui::DragFloat("Aperture (f-stop)", &aperture, 0.1f,
                                     LightingConvention::CameraImagingBaseline::kMinAperture,
                                     LightingConvention::CameraImagingBaseline::kMaxAperture, "%.1f"))
                {
                    imaging.aperture = aperture;
                    imagingChanged = true;
                }

                float shutterSpeed = imaging.shutterSpeed;
                if (ImGui::DragFloat("Shutter (seconds)", &shutterSpeed, 0.001f,
                                     LightingConvention::CameraImagingBaseline::kMinShutterSpeed,
                                     LightingConvention::CameraImagingBaseline::kMaxShutterSpeed, "%.4f"))
                {
                    imaging.shutterSpeed = shutterSpeed;
                    imagingChanged = true;
                }
                if (imaging.shutterSpeed > 1e-6f)
                    ImGui::TextDisabled("Approx 1/%.0f s", 1.f / imaging.shutterSpeed);

                float iso = imaging.iso;
                if (ImGui::DragFloat("ISO", &iso, 10.f, LightingConvention::CameraImagingBaseline::kMinIso,
                                     LightingConvention::CameraImagingBaseline::kMaxIso, "%.0f"))
                {
                    imaging.iso = iso;
                    imagingChanged = true;
                }

                float evComp = imaging.exposureCompensationEv;
                if (ImGui::DragFloat("Exposure Compensation (EV)", &evComp, 0.1f, -6.f, 6.f, "%+.1f"))
                {
                    imaging.exposureCompensationEv = evComp;
                    imagingChanged = true;
                }

                if (imagingChanged)
                {
                    cam->SyncExposureFromImaging();
                    SceneSession::Get().MarkDirty();
                }

                ImGui::Text("Exposure (linear): %.4f", cam->GetComputedExposureLinear());
                ImGui::TextDisabled("Ref: f/2.8, 1/60s, ISO100 -> exposure 1.0");
            }

            float gamma = setting.tonemap_setting.y;
            if (ImGui::DragFloat("Gamma", &gamma, 0.1, 0.0, 10.0))
            {
                setting.tonemap_setting.y = gamma;
                SceneSession::Get().MarkDirty();
            }

            if (ImGui::CollapsingHeader("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
            {
                bool enalbe = setting.bloom_setting.w > 0;
                if (ImGui::Checkbox("enable", &enalbe))
                    setting.bloom_setting.w = enalbe ? 1.0 : -1.0;
                float threashold = setting.bloom_setting.x;
                if (ImGui::DragFloat("threadshold", &threashold, 0.1, -1.0, 10.0))
                    setting.bloom_setting.x = threashold;

                float softKeen = setting.bloom_setting.y;
                if (ImGui::DragFloat("softKeen", &softKeen, 0.1, -1.0, 1.0))
                    setting.bloom_setting.y = softKeen;

                float intensity = setting.bloom_setting.z;
                if (ImGui::DragFloat("bloom intensity", &intensity, 0.1, 0.0, 100.0))
                    setting.bloom_setting.z = intensity;
            }
            if (ImGui::CollapsingHeader("SSAO", ImGuiTreeNodeFlags_DefaultOpen))
            {
                DragFloat(setting.sso_setting.x, "ssao radius", 0.1, 0.0, 100.0);
                float ssao_bias = setting.sso_setting.y;
                if (ImGui::DragFloat("ssao bias", &ssao_bias, 0.001, 0.0, 10.0))
                    setting.sso_setting.y = ssao_bias;

                float ssao_pow = setting.sso_setting.z;
                if (ImGui::DragFloat("ssao pow", &ssao_pow, 0.1, 0.0, 100.0))
                    setting.sso_setting.z = ssao_pow;

                float ssao_ins = setting.sso_setting.w;
                if (ImGui::DragFloat("ssao intensity", &ssao_ins, 0.1, 0.0, 100.0))
                    setting.sso_setting.w = ssao_ins;
            }
        }
    }

    if (MeshRender* mr = ent->GetComponent<MeshRender>())
    {
        if (ImGui::CollapsingHeader("MeshRender", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const std::shared_ptr<Model>& model = mr->GetModel();
            const auto& models = AssetManager::Get().m_models;

            AssetPickerItems modelItems;
            {
                std::vector<std::string> modelKeys;
                AssetManager::CollectUiVisibleKeys(models, modelKeys, true);
                modelItems.SetKeys(std::move(modelKeys));
            }

            int current_item = 0;
            if (model)
            {
                for (size_t i = 0; i < modelItems.size(); ++i)
                {
                    if (modelItems.keys[i] == model->m_path)
                    {
                        current_item = static_cast<int>(i);
                        break;
                    }
                }
            }

            if (DrawAssetPathCombo("Model", &current_item, modelItems))
            {
                if (current_item >= 0 && static_cast<size_t>(current_item) < modelItems.size())
                {
                    const std::string& selectedPath = modelItems.keys[static_cast<size_t>(current_item)];
                    if (!model || model->m_path != selectedPath)
                    {
                        if (std::shared_ptr<Model> newModel = AssetManager::Get().GetAsset<Model>(selectedPath))
                        {
                            mr->SetModel(newModel);
                            SceneSession::Get().MarkDirty();
                        }
                    }
                }
            }
            if (model)
            {
                ImGui::TextDisabled("Path: %s", model->m_path.c_str());

                ImGui::Separator();
                if (ImGui::TreeNode("Material Overrides"))
                {
                    DrawMeshRenderMaterialOverrides(mr, model);
                    ImGui::TreePop();
                }

                ImGui::Separator();
                if (ImGui::TreeNode("Rendering"))
                {
                    bool drawCustomDepth = mr->GetDrawCustomDepth();
                    if (ImGui::Checkbox("Draw Custom Depth", &drawCustomDepth))
                    {
                        mr->SetDrawCustomDepth(drawCustomDepth);
                        SceneSession::Get().MarkDirty();
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Write oct-encoded normal + depth to the custom depth buffer (e.g. water refraction).");

                    bool castShadow = mr->GetCastShadow();
                    if (ImGui::Checkbox("Cast Shadow", &castShadow))
                    {
                        mr->SetCastShadow(castShadow);
                        SceneSession::Get().MarkDirty();
                    }

                    ImGui::TreePop();
                }
            }
        }
    }

    if (Light* light = ent->GetComponent<Light>())
    {
        if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            glm::vec3 color = light->GetColor();
            if (ImGui::ColorEdit3("Color", &color.x))
                light->SetColor(color);

            float intensity = light->GetIntensity();
            if (light->type == LightType::Directional)
            {
                if (ImGui::DragFloat("Illuminance (lux)", &intensity, 1000.0f, 0.0f, 200000.0f, "%.0f"))
                    light->SetIntensity(intensity);
                ImGui::TextDisabled("Ref: 100k lux ~ outdoor sun; gray18 HDR ~ %.5f",
                                    LightingConvention::DirectionalCalibrationMeasured::
                                        kReferenceHdrGray18AtReferenceLux);
            }
            else if (light->type == LightType::PointLight || light->type == LightType::SpotLight)
            {
                if (ImGui::DragFloat("Luminous flux (lm)", &intensity, 10.0f, 0.0f, 50000.0f, "%.0f"))
                    light->SetIntensity(intensity);
                ImGui::TextDisabled("E @ 1m (face to light): %.1f lux", intensity * LightingConvention::kInvFourPi);
                ImGui::TextDisabled("Default bulb ref: %.0f lm", LightingConvention::PointLightBaseline::kDefaultPointLumens);
            }
            else if (ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 100.0f))
                light->SetIntensity(intensity);

            if (light->type == LightType::SpotLight)
            {
                float inCutOff = light->m_inCutoff;
                if (ImGui::DragFloat("in Cut Off", &inCutOff))
                    light->m_inCutoff = inCutOff;

                float outCutoff = light->m_outCutoff;
                if (ImGui::DragFloat("out Cut Off", &outCutoff))
                    light->m_outCutoff = outCutoff;
            }

            if (ImGui::CollapsingHeader("Shadow", ImGuiTreeNodeFlags_DefaultOpen))
            {
                float z_near = light->m_nearPlane;
                if (ImGui::DragFloat("near palne", &z_near))
                    light->m_nearPlane = z_near;

                float z_far = light->m_farPlane;
                if (ImGui::DragFloat("far plane", &z_far))
                    light->m_farPlane = z_far;

                float bias = light->m_bias;
                if (ImGui::DragFloat("bias", &bias, 0.0001, 0.0, 1.0, "%.6f"))
                    light->m_bias = bias;

                int pcfSample = light->m_pcfSample;
                if (ImGui::DragInt("Pcf Sample", &pcfSample, 1, 0, 8))
                    light->m_pcfSample = pcfSample;

                float shadow_area = light->m_shadow_area;
                if (ImGui::DragFloat("Shadow Area", &shadow_area, 0.1, 0.0, 100.0))
                    light->m_shadow_area = shadow_area;

                // shadow size
                static const char* shadowResItems[4] = {"512", "1024", "2048", "4096"};
                ShadowRes res = light->m_ShadowRes;
                int current_item = (int)res / 512 - 1;
                if (ImGui::Combo("Model", &current_item, shadowResItems, 4))
                {
                    res = (ShadowRes)((current_item + 1) * 512);
                    light->m_ShadowRes = res;
                }
            }
        }
    }

    if (SkyBox* skybox = ent->GetComponent<SkyBox>())
    {
        if (ImGui::CollapsingHeader("SkyBox", ImGuiTreeNodeFlags_DefaultOpen))
        {
            AssetPickerItems cubemapItems;
            CollectLoadedCubemapPickerItems(cubemapItems);

            if (cubemapItems.size() == 0)
            {
                ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "No cubemaps loaded.");
            }
            else
            {
                int cubemapIdx = FindCubemapIndex(cubemapItems.keys, skybox->GetIBL().get());
                if (DrawAssetPathCombo("IBL", &cubemapIdx, cubemapItems))
                {
                    std::shared_ptr<IBLImage> ibl =
                        AssetManager::Get().GetAsset<IBLImage>(cubemapItems.keys[static_cast<size_t>(cubemapIdx)]);
                    if (ibl)
                        skybox->SetIBL(ibl);
                }

                if (std::shared_ptr<IBLImage>& current = skybox->GetIBL(); current)
                    ImGui::TextDisabled("%s", current->m_path.c_str());
            }

            ImGui::Separator();
            ImGui::TextUnformatted("IBL Lighting (PBR)");

            bool iblLighting = skybox->IsIBLLightingEnabled();
            if (ImGui::Checkbox("Enable IBL Lighting", &iblLighting))
                skybox->SetIBLLightingEnabled(iblLighting);

            float iblIntensity = skybox->GetIBLLightingIntensity();
            if (ImGui::DragFloat("IBL Lighting Intensity", &iblIntensity, 0.01f, 0.0f, 10.0f))
                skybox->SetIBLLightingIntensity(iblIntensity);

            ImGui::Separator();
            ImGui::TextUnformatted("Sky Background");

            bool drawBg = skybox->IsDrawBackgroundEnabled();
            if (ImGui::Checkbox("Draw Sky Background", &drawBg))
                skybox->SetDrawBackgroundEnabled(drawBg);

            float brightness = skybox->GetBrightness();
            if (ImGui::DragFloat("Sky Display Brightness", &brightness, 0.1f, 0.0f, 10.0f))
                skybox->SetBrightness(brightness);

            int previewIdx = static_cast<int>(skybox->GetCubemapPreview());
            const char* previewLabels[] = {"Prefiltered (Mip0 / Environment)", "Irradiance"};
            if (ImGui::Combo("Cubemap Preview", &previewIdx, previewLabels, IM_ARRAYSIZE(previewLabels)))
                skybox->SetCubemapPreview(static_cast<SkyBox::CubemapPreview>(previewIdx));

            ImGui::Separator();
            ImGui::TextUnformatted("Fur Ambient (_AmbientColor)");
            glm::vec3 color = skybox->GetColor();
            if (ImGui::ColorEdit3("Color", &color.x))
                skybox->SetColor(color);

            float intensity = skybox->GetIntensity();
            if (ImGui::DragFloat("Fur Ambient Intensity", &intensity, 0.1f, 0.0f, 10.0f))
                skybox->SetIntensity(intensity);
        }
    }

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.2f, 0.2f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.1f, 0.1f, 1.f));
    if (ImGui::Button("Delete Entity"))
    {
        const int deletedIdx = item_selected_idx;
        EntityManager::Get().DestroyEntity(ent);
        SceneSession::Get().MarkDirty();
        const int newCount = static_cast<int>(EntityManager::Get().GetEntityCount());
        if (newCount <= 0)
            item_selected_idx = 0;
        else if (deletedIdx >= newCount)
            item_selected_idx = newCount - 1;
        else
            item_selected_idx = deletedIdx;
        m_detailLastSelectedIdx = -1;
        ImGui::PopStyleColor(3);
        ImGui::End();
        return;
    }
    ImGui::PopStyleColor(3);

    ImGui::End();
}

void Gui::RefreshAssetBrowserListing()
{
    CollectAssetBrowserEntries(m_assetBrowserPath, m_assetBrowserNames, m_assetBrowserPaths, m_assetBrowserKinds);
    PruneAssetBrowserSelection(m_assetBrowserSelection, m_assetBrowserPaths.size());
}

void Gui::SyncAssetDetailFromBrowserSelection()
{
    if (m_assetBrowserSelectedIndices == m_assetBrowserLastSyncedDetailIndices)
        return;

    m_assetBrowserLastSyncedDetailIndices = m_assetBrowserSelectedIndices;

    if (m_assetBrowserSelection.Size == 0)
    {
        if (m_detailViewMode == DetailViewMode::Asset)
            ClearAssetDetail();
        return;
    }

    if (m_assetBrowserSelection.Size != 1)
        return;

    void* it = nullptr;
    ImGuiID id = 0;
    if (!m_assetBrowserSelection.GetNextSelectedItem(&it, &id))
        return;

    const int idx = static_cast<int>(id);
    if (idx < 0 || static_cast<size_t>(idx) >= m_assetBrowserPaths.size())
        return;

    const AssetEntryKind kind = static_cast<AssetEntryKind>(m_assetBrowserKinds[static_cast<size_t>(idx)]);
    if (kind == AssetEntryKind::Folder)
        ClearAssetDetail();
    else
        OpenAssetDetail(m_assetBrowserPaths[static_cast<size_t>(idx)]);
}

void Gui::ClearAssetBrowserSelection()
{
    m_assetBrowserSelection.Clear();
    m_assetBrowserSelectedIndices.clear();
    m_assetBrowserLastSyncedDetailIndices.clear();
}

void Gui::ShowAssetBrowser(RenderContext& context)
{
    if (!ImGui::Begin("Asset Browser"))
    {
        ImGui::End();
        return;
    }

    if (m_assetBrowserNames.empty())
        RefreshAssetBrowserListing();

    std::error_code ec;
    const bool dirValid =
        std::filesystem::exists(m_assetBrowserPath, ec) && std::filesystem::is_directory(m_assetBrowserPath, ec);

    if (ImGui::Button("Up"))
    {
        std::filesystem::path current(m_assetBrowserPath);
        if (current.has_parent_path() && current.parent_path() != current)
        {
            const std::string parent = NormalizeAssetPath(current.parent_path().generic_string());
            std::snprintf(m_assetBrowserPath, sizeof(m_assetBrowserPath), "%s", parent.c_str());
            ClearAssetBrowserSelection();
            ClearAssetDetail();
            RefreshAssetBrowserListing();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        RefreshAssetBrowserListing();
    ImGui::SameLine();
    if (ImGui::Button("Content/Project"))
    {
        std::snprintf(m_assetBrowserPath, sizeof(m_assetBrowserPath), "%s", "Content/Project");
        ClearAssetBrowserSelection();
        ClearAssetDetail();
        RefreshAssetBrowserListing();
    }

    if (ImGui::InputText("Path", m_assetBrowserPath, IM_ARRAYSIZE(m_assetBrowserPath)))
    {
        ClearAssetBrowserSelection();
        ClearAssetDetail();
        RefreshAssetBrowserListing();
    }

    ImGui::TextDisabled("Ctrl/Shift+Click multi-select | Double-click load");

    if (!dirValid)
        ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Directory does not exist.");

    ImGui::Separator();

    constexpr float kCreatePanelWidth = 118.f;
    constexpr float kBottomBarHeight = 52.f;
    const ImVec2 contentAvail = ImGui::GetContentRegionAvail();

    ImGui::BeginChild("asset_browser_main", ImVec2(contentAvail.x, contentAvail.y - kBottomBarHeight),
                      ImGuiChildFlags_None);
    ImGui::BeginChild("asset_browser_list", ImVec2(contentAvail.x - kCreatePanelWidth - 8.f, 0.f),
                      ImGuiChildFlags_Borders);

    const ImGuiTableFlags tableFlags =
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("asset_browser_table", 2, tableFlags, ImVec2(0.f, 0.f)))
    {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Type");
        ImGui::TableHeadersRow();

        const int itemCount = static_cast<int>(m_assetBrowserNames.size());
        ImGuiMultiSelectFlags msFlags =
            ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_BoxSelect1d | ImGuiMultiSelectFlags_ScopeRect;
        ImGuiMultiSelectIO* msIo = ImGui::BeginMultiSelect(msFlags, m_assetBrowserSelection.Size, itemCount);
        m_assetBrowserSelection.ApplyRequests(msIo);

        std::string pendingFolderPath;
        std::string pendingLoadPath;
        AssetEntryKind pendingLoadKind = AssetEntryKind::Unknown;

        for (int i = 0; i < itemCount; ++i)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();

            const AssetEntryKind kind = static_cast<AssetEntryKind>(m_assetBrowserKinds[static_cast<size_t>(i)]);
            const bool itemSelected = m_assetBrowserSelection.Contains(static_cast<ImGuiID>(i));
            ImGui::SetNextItemSelectionUserData(i);
            if (ImGui::Selectable(m_assetBrowserNames[static_cast<size_t>(i)].c_str(), itemSelected))
            {
                if (kind == AssetEntryKind::Folder && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift)
                    ClearAssetDetail();
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                if (kind == AssetEntryKind::Folder)
                    pendingFolderPath = m_assetBrowserPaths[static_cast<size_t>(i)];
                else if (CanLoadAssetEntry(kind))
                {
                    pendingLoadPath = m_assetBrowserPaths[static_cast<size_t>(i)];
                    pendingLoadKind = kind;
                }
            }

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(AssetEntryKindLabel(kind));
        }

        msIo = ImGui::EndMultiSelect();
        m_assetBrowserSelection.ApplyRequests(msIo);
        CollectAssetBrowserSelectionIndices(m_assetBrowserSelection, m_assetBrowserSelectedIndices);

        if (!pendingFolderPath.empty())
        {
            std::snprintf(m_assetBrowserPath, sizeof(m_assetBrowserPath), "%s", pendingFolderPath.c_str());
            ClearAssetBrowserSelection();
            ClearAssetDetail();
            RefreshAssetBrowserListing();
        }
        else
        {
            SyncAssetDetailFromBrowserSelection();
            if (!pendingLoadPath.empty())
            {
                if (pendingLoadKind == AssetEntryKind::Scene)
                {
                    PendingSceneAction action;
                    action.kind = PendingSceneActionKind::LoadScenePath;
                    action.path = pendingLoadPath;
                    RequestSceneAction(action, context);
                }
                else
                {
                    const int batchLoadable = CountLoadableAssetBrowserIndices(
                        m_assetBrowserSelectedIndices, m_assetBrowserKinds, m_assetBrowserPaths.size());
                    if (m_assetBrowserSelectedIndices.size() > 1 && batchLoadable > 1)
                    {
                        LoadAssetBrowserEntriesByIndices(m_assetBrowserSelectedIndices, m_assetBrowserPaths,
                                                         m_assetBrowserKinds, m_assetBrowserStatus);
                    }
                    else
                    {
                        std::string msg;
                        LoadAssetEntry(pendingLoadKind, pendingLoadPath, msg, &context);
                        m_assetBrowserStatus = std::move(msg);
                    }
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("asset_browser_create", ImVec2(kCreatePanelWidth, 0.f), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("Create");
    ImGui::Separator();
    ImGui::BeginDisabled(!dirValid);
    if (ImGui::Button("Material", ImVec2(-FLT_MIN, 0.f)))
    {
        std::snprintf(m_materialCreateName, sizeof(m_materialCreateName), "%s", "NewMaterial");
        m_openCreateMaterialModal = true;
    }
    if (ImGui::Button("Folder", ImVec2(-FLT_MIN, 0.f)))
    {
        std::snprintf(m_folderCreateName, sizeof(m_folderCreateName), "%s", "NewFolder");
        m_openCreateFolderModal = true;
    }
    if (ImGui::Button("Scene", ImVec2(-FLT_MIN, 0.f)))
    {
        std::snprintf(m_sceneCreateName, sizeof(m_sceneCreateName), "%s", "NewScene");
        m_openCreateSceneModal = true;
    }
    ImGui::EndDisabled();
    ImGui::EndChild();
    ImGui::EndChild();

    ImGui::Separator();

    const int selectedCount = static_cast<int>(m_assetBrowserSelectedIndices.size());
    const int loadableCount =
        CountLoadableAssetBrowserIndices(m_assetBrowserSelectedIndices, m_assetBrowserKinds, m_assetBrowserPaths.size());

    if (selectedCount > 0)
    {
        ImGui::Text("Selected: %d item(s)", selectedCount);
        if (selectedCount == 1)
        {
            void* it = nullptr;
            ImGuiID id = 0;
            if (m_assetBrowserSelection.GetNextSelectedItem(&it, &id))
            {
                const int idx = static_cast<int>(id);
                if (idx >= 0 && static_cast<size_t>(idx) < m_assetBrowserPaths.size())
                {
                    const AssetEntryKind kind =
                        static_cast<AssetEntryKind>(m_assetBrowserKinds[static_cast<size_t>(idx)]);
                    ImGui::Text("Type: %s", AssetEntryKindLabel(kind));
                    ImGui::TextWrapped("%s", m_assetBrowserPaths[static_cast<size_t>(idx)].c_str());
                }
            }
        }

        ImGui::BeginDisabled(loadableCount == 0);
        if (ImGui::Button(loadableCount <= 1 ? "Load" : "Load Selected"))
        {
            if (selectedCount == 1)
            {
                void* it = nullptr;
                ImGuiID id = 0;
                if (m_assetBrowserSelection.GetNextSelectedItem(&it, &id))
                {
                    const int idx = static_cast<int>(id);
                    if (idx >= 0 && static_cast<size_t>(idx) < m_assetBrowserPaths.size())
                    {
                        const AssetEntryKind kind =
                            static_cast<AssetEntryKind>(m_assetBrowserKinds[static_cast<size_t>(idx)]);
                        if (kind == AssetEntryKind::Scene)
                        {
                            PendingSceneAction action;
                            action.kind = PendingSceneActionKind::LoadScenePath;
                            action.path = m_assetBrowserPaths[static_cast<size_t>(idx)];
                            RequestSceneAction(action, context);
                        }
                        else
                        {
                            LoadAssetBrowserEntriesByIndices(m_assetBrowserSelectedIndices, m_assetBrowserPaths,
                                                             m_assetBrowserKinds, m_assetBrowserStatus);
                        }
                    }
                }
            }
            else
            {
                LoadAssetBrowserEntriesByIndices(m_assetBrowserSelectedIndices, m_assetBrowserPaths,
                                                 m_assetBrowserKinds, m_assetBrowserStatus);
            }
        }
        ImGui::EndDisabled();

        if (selectedCount == 1)
        {
            void* it = nullptr;
            ImGuiID id = 0;
            if (m_assetBrowserSelection.GetNextSelectedItem(&it, &id))
            {
                const int idx = static_cast<int>(id);
                if (idx >= 0 && static_cast<size_t>(idx) < m_assetBrowserPaths.size())
                {
                    const AssetEntryKind kind =
                        static_cast<AssetEntryKind>(m_assetBrowserKinds[static_cast<size_t>(idx)]);
                    const bool canDelete = kind != AssetEntryKind::Folder && kind != AssetEntryKind::Unknown;

                    ImGui::SameLine();
                    ImGui::BeginDisabled(!canDelete);
                    if (ImGui::Button("Delete"))
                    {
                        const std::string& selectedPath = m_assetBrowserPaths[static_cast<size_t>(idx)];
                        std::string msg;
                        if (DeleteAssetFile(selectedPath, msg))
                        {
                            if (m_assetDetailPath == selectedPath)
                                ClearAssetDetail();
                            ClearAssetBrowserSelection();
                            RefreshAssetBrowserListing();
                        }
                        m_assetBrowserStatus = std::move(msg);
                    }
                    ImGui::EndDisabled();

                    if (kind == AssetEntryKind::Folder)
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("Open"))
                        {
                            std::snprintf(m_assetBrowserPath, sizeof(m_assetBrowserPath), "%s",
                                          m_assetBrowserPaths[static_cast<size_t>(idx)].c_str());
                            ClearAssetBrowserSelection();
                            ClearAssetDetail();
                            RefreshAssetBrowserListing();
                        }
                    }
                }
            }
        }
    }
    else
    {
        ImGui::TextUnformatted("Select one or more files to load.");
    }

    if (!m_assetBrowserStatus.empty())
        ImGui::TextWrapped("%s", m_assetBrowserStatus.c_str());

    if (m_openCreateMaterialModal)
        ImGui::OpenPopup("CreateMaterialModal");
    if (m_openCreateFolderModal)
        ImGui::OpenPopup("CreateFolderModal");

    ImGui::SetNextWindowSize(ImVec2(360.f, 0.f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("CreateMaterialModal", &m_openCreateMaterialModal, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Create material in:");
        ImGui::TextWrapped("%s", m_assetBrowserPath);
        ImGui::InputText("Material Name", m_materialCreateName, IM_ARRAYSIZE(m_materialCreateName));

        if (ImGui::Button("Create", ImVec2(120.f, 0.f)))
        {
            std::string createdPath;
            std::string msg;
            if (CreateMaterialFileInDirectory(m_assetBrowserPath, m_materialCreateName, createdPath, msg))
            {
                RefreshAssetBrowserListing();
                ClearAssetBrowserSelection();
                for (int i = 0; i < static_cast<int>(m_assetBrowserPaths.size()); ++i)
                {
                    if (m_assetBrowserPaths[static_cast<size_t>(i)] == createdPath)
                    {
                        m_assetBrowserSelection.SetItemSelected(static_cast<ImGuiID>(i), true);
                        OpenAssetDetail(createdPath);
                        break;
                    }
                }
                m_assetBrowserStatus = std::move(msg);
                m_openCreateMaterialModal = false;
            }
            else
            {
                m_assetBrowserStatus = std::move(msg);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
            m_openCreateMaterialModal = false;
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowSize(ImVec2(360.f, 0.f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("CreateFolderModal", &m_openCreateFolderModal, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Create folder in:");
        ImGui::TextWrapped("%s", m_assetBrowserPath);
        ImGui::InputText("Folder Name", m_folderCreateName, IM_ARRAYSIZE(m_folderCreateName));

        if (ImGui::Button("Create", ImVec2(120.f, 0.f)))
        {
            std::string createdPath;
            std::string msg;
            if (CreateFolderInDirectory(m_assetBrowserPath, m_folderCreateName, createdPath, msg))
            {
                RefreshAssetBrowserListing();
                ClearAssetBrowserSelection();
                for (int i = 0; i < static_cast<int>(m_assetBrowserPaths.size()); ++i)
                {
                    if (m_assetBrowserPaths[static_cast<size_t>(i)] == createdPath)
                    {
                        m_assetBrowserSelection.SetItemSelected(static_cast<ImGuiID>(i), true);
                        break;
                    }
                }
                m_assetBrowserStatus = std::move(msg);
                m_openCreateFolderModal = false;
            }
            else
            {
                m_assetBrowserStatus = std::move(msg);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
            m_openCreateFolderModal = false;
        ImGui::EndPopup();
    }

    ImGui::End();
}

void Gui::SelectEntity(const std::shared_ptr<Entity>& entity)
{
    if (!entity)
        return;

    const std::vector<std::shared_ptr<Entity>>& entities = EntityManager::Get().GetEntities();
    for (size_t i = 0; i < entities.size(); ++i)
    {
        if (entities[i] == entity)
        {
            item_selected_idx = static_cast<int>(i);
            m_detailLastSelectedIdx = -1;
            m_detailViewMode = DetailViewMode::Entity;
            m_assetBrowserLastSyncedDetailIndices = m_assetBrowserSelectedIndices;
            break;
        }
    }
}

void Gui::ShowScene()
{
    if (!ImGui::Begin("Scene"))
    {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Create Entity");
    ImGui::InputText("Name", m_createEntityName, IM_ARRAYSIZE(m_createEntityName));

    static const char* kEntityTypes[] = {
        "Empty", "Camera", "Mesh Render", "Directional Light", "Point Light", "Spot Light", "Skybox",
    };
    ImGui::Combo("Type", &m_createEntityType, kEntityTypes, IM_ARRAYSIZE(kEntityTypes));

    const auto& models = AssetManager::Get().m_models;

    AssetPickerItems modelItems;
    AssetPickerItems cubemapItems;

    bool canCreate = true;

    if (m_createEntityType == 2)
    {
        std::vector<std::string> modelKeys;
        AssetManager::CollectUiVisibleKeys(models, modelKeys, true);
        modelItems.SetKeys(std::move(modelKeys));

        if (modelItems.size() == 0)
        {
            ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "No models loaded. Use Asset Browser first.");
            canCreate = false;
        }
        else
        {
            if (m_createEntityModelIdx < 0 || m_createEntityModelIdx >= static_cast<int>(modelItems.size()))
                m_createEntityModelIdx = 0;
            DrawAssetPathCombo("Model", &m_createEntityModelIdx, modelItems);
        }
    }
    else if (m_createEntityType == 6)
    {
        CollectLoadedCubemapPickerItems(cubemapItems);

        if (EntityManager::Get().skyBox)
        {
            ImGui::TextColored(ImVec4(1.f, 0.8f, 0.2f, 1.f), "Skybox already exists: %s",
                               EntityManager::Get().skyBox->m_name.c_str());
            canCreate = false;
        }
        else if (cubemapItems.size() == 0)
        {
            ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "No cubemaps loaded. Use Asset Browser first.");
            canCreate = false;
        }
        else
        {
            if (m_createEntityCubemapIdx < 0 || m_createEntityCubemapIdx >= static_cast<int>(cubemapItems.size()))
                m_createEntityCubemapIdx = 0;
            DrawAssetPathCombo("CubeMap", &m_createEntityCubemapIdx, cubemapItems);
        }
    }
    else if (m_createEntityType == 0)
    {
        ImGui::Checkbox("Add Transform", &m_createEntityAddTransform);
    }

    ImGui::BeginDisabled(!canCreate);
    if (ImGui::Button("Create", ImVec2(-1.f, 0.f)))
    {
        const std::string name = m_createEntityName;
        EntityManager& em = EntityManager::Get();
        std::shared_ptr<Entity> created;

        switch (m_createEntityType)
        {
        case 0:
            created = em.CreateEntity(name);
            if (m_createEntityAddTransform)
                created->AddComponent<Transform>();
            m_createEntityStatus = "Created empty entity: " + created->m_name;
            break;
        case 1:
            created = em.CreateCameraEntity(name);
            m_createEntityStatus = "Created camera: " + created->m_name;
            break;
        case 2:
            if (std::shared_ptr<Model> model =
                    AssetManager::Get().GetAsset<Model>(modelItems.keys[static_cast<size_t>(m_createEntityModelIdx)]))
            {
                created = em.CreateMeshRenderEntity(name, model);
                m_createEntityStatus = "Created mesh render: " + created->m_name;
            }
            else
                m_createEntityStatus = "Failed: model not found.";
            break;
        case 3:
            created = em.CreateLight(name, LightType::Directional);
            m_createEntityStatus = "Created directional light: " + created->m_name;
            break;
        case 4:
            created = em.CreateLight(name, LightType::PointLight);
            m_createEntityStatus = "Created point light: " + created->m_name;
            break;
        case 5:
            created = em.CreateLight(name, LightType::SpotLight);
            m_createEntityStatus = "Created spot light: " + created->m_name;
            break;
        case 6:
            if (std::shared_ptr<IBLImage> ibl =
                    AssetManager::Get().GetAsset<IBLImage>(cubemapItems.keys[static_cast<size_t>(m_createEntityCubemapIdx)]))
            {
                created = em.CreateSkyBoxEntity(name, ibl);
                m_createEntityStatus = "Created skybox: " + created->m_name;
            }
            else
                m_createEntityStatus = "Failed: IBL not found.";
            break;
        default:
            m_createEntityStatus = "Unknown entity type.";
            break;
        }

        if (created)
        {
            created->Init();
            SelectEntity(created);
            SceneSession::Get().MarkDirty();
        }
    }
    ImGui::EndDisabled();

    if (!m_createEntityStatus.empty())
        ImGui::TextWrapped("%s", m_createEntityStatus.c_str());

    ImGui::Separator();
    ImGui::Text("Entities in scene: %zu", EntityManager::Get().GetEntityCount());

    ImGui::End();
}

#if GLE_ENABLE_RENDERDOC
void Gui::ShowRenderDoc()
{
    if (!ImGui::Begin("RenderDoc"))
    {
        ImGui::End();
        return;
    }

    if (RenderDocSupport::IsAvailable())
    {
        ImGui::TextColored(ImVec4(0.4f, 1.f, 0.5f, 1.f), "API connected");
        ImGui::TextUnformatted("F12: capture frame (when launched from RenderDoc)");
    }
    else
    {
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0.3f, 1.f), "API not available");
        ImGui::TextWrapped("Set renderDocPath in Preferences, install RenderDoc, or launch this exe via qrenderdoc.");
    }

    ImGui::Separator();

    ImGui::BeginDisabled(!RenderDocSupport::IsAvailable());
    if (ImGui::Button("Capture Next Frame"))
        RenderDocSupport::TriggerCapture();

    if (RenderDocSupport::IsFrameCapturing())
    {
        ImGui::SameLine();
        if (ImGui::Button("End Capture"))
            RenderDocSupport::EndFrameCapture();
    }
    else if (ImGui::Button("Start Capture"))
    {
        RenderDocSupport::StartFrameCapture();
    }
    ImGui::EndDisabled();

    ImGui::End();
}
#endif

void Gui::ShowOutline()
{
    if (!ImGui::Begin("outline"))
    {
        ImGui::End();
        return;
    }

    const int entityCount = static_cast<int>(EntityManager::Get().GetEntityCount());
    if (entityCount <= 0)
        item_selected_idx = 0;
    else if (item_selected_idx < 0 || item_selected_idx >= entityCount)
        item_selected_idx = entityCount - 1;

    if (ImGui::BeginListBox(" ", ImVec2(-FLT_MIN, -FLT_MIN)))
    {
        for (int n = 0; n < entityCount; n++)
        {
            const bool is_selected = (item_selected_idx == n);
            if (ImGui::Selectable(EntityManager::Get().GetEntities()[n]->m_name.c_str(), is_selected))
            {
                item_selected_idx = n;
                m_detailLastSelectedIdx = -1;
                m_detailViewMode = DetailViewMode::Entity;
                m_assetBrowserLastSyncedDetailIndices = m_assetBrowserSelectedIndices;
            }

            // if (item_highlight && ImGui::IsItemHovered())
            //     item_highlighted_idx = n;

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
    }

    ImGui::End();
}

void Gui::DragFloat(float& value, const char* label, const float speed, const float min, const float max)
{
    float v = value;
    if (ImGui::DragFloat(label, &v, speed, min, max))
        value = v;
}
#undef MODULE

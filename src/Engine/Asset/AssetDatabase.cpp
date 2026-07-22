#include "AssetDatabase.h"

#include "AssetPaths.h"
#include "../Core/Log.h"
#include "../Core/util.h"

#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <unordered_map>
#include <unordered_set>

#define MODULE "AssetDatabase"

namespace
{
struct MetaRegistry
{
    std::unordered_map<std::string, std::string> guidToSourcePath;
    std::unordered_map<std::string, AssetMeta> metaBySourcePath;
};

bool ParseMetaJson(const nlohmann::json& j, const std::string& fallbackSourcePath, AssetMeta& outMeta);

MetaRegistry& GetRegistry()
{
    static MetaRegistry registry;
    return registry;
}

void RegisterMeta(const AssetMeta& meta)
{
    AssetMeta normalized = meta;
    normalized.sourcePath = AssetDataBase::CanonicalizeMetaSourcePath(meta.sourcePath);
    if (normalized.sourcePath.empty())
        return;

    GetRegistry().metaBySourcePath[normalized.sourcePath] = normalized;
    if (!normalized.guid.empty())
    {
        const auto existing = GetRegistry().guidToSourcePath.find(normalized.guid);
        if (existing != GetRegistry().guidToSourcePath.end() && existing->second != normalized.sourcePath)
        {
            LogA(LogLevel::WARNING, "Duplicate guid '{}' for '{}' and '{}', keeping first entry", normalized.guid,
                 existing->second, normalized.sourcePath);
            return;
        }
        GetRegistry().guidToSourcePath[normalized.guid] = normalized.sourcePath;
    }
}

std::string NormalizeAssetPath(const std::filesystem::path& path)
{
    std::string normalized = path.lexically_normal().string();
    for (char& c : normalized)
    {
        if (c == '\\')
            c = '/';
    }
    return normalized;
}

std::string SourcePathFromMetaFile(const std::filesystem::path& metaFilePath)
{
    return NormalizeAssetPath(metaFilePath.parent_path() / metaFilePath.stem());
}

std::string ReadTextFileStripUtf8Bom(const std::string& ioPath)
{
    std::ifstream ifs(ioPath, std::ios::binary);
    if (!ifs)
        return {};

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (content.size() >= 3 && static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB && static_cast<unsigned char>(content[2]) == 0xBF)
    {
        content.erase(0, 3);
    }
    return content;
}

bool LoadMetaFromFile(const std::string& metaPath, const std::string& fallbackSourcePath, AssetMeta& outMeta)
{
    std::string ioPath = metaPath;
    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();
    if (!metaPath.empty() && !std::filesystem::path(metaPath).is_absolute())
        ioPath = AssetPaths::Get().ToAbsolutePath(metaPath);

    const std::string content = ReadTextFileStripUtf8Bom(ioPath);
    if (content.empty())
        return false;

    try
    {
        nlohmann::json j = nlohmann::json::parse(content);
        return ParseMetaJson(j, fallbackSourcePath, outMeta);
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }
}

std::string MetaFilePath(const std::string& sourcePath)
{
    return sourcePath + ".meta";
}

bool AssetSourceFileExists(const std::string& sourcePath)
{
    if (sourcePath.empty())
        return false;
    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();
    std::error_code ec;
    return std::filesystem::is_regular_file(AssetPaths::Get().ToAbsolutePath(sourcePath), ec);
}

std::string ToProjectRelativeSourcePath(const std::filesystem::path& absolutePath)
{
    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();
    return AssetPaths::Get().ToRelativeSourcePath(absolutePath);
}

std::string CanonicalizeMetaSourcePathImpl(const std::string& sourcePath)
{
    if (sourcePath.empty())
        return {};

    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();

    AssetPaths& paths = AssetPaths::Get();
    std::string resolved = NormalizeAssetPath(paths.ResolveToSourcePath(sourcePath));

    if (const std::filesystem::path input(resolved); input.is_absolute())
        resolved = ToProjectRelativeSourcePath(input);

    const bool bareFileName = resolved.find('/') == std::string::npos && resolved.find('\\') == std::string::npos;
    if (bareFileName)
    {
        const std::string fromTextures = Util::ResolveShaderDefaultTexturePath(resolved);
        if (!fromTextures.empty())
            resolved = NormalizeAssetPath(paths.ResolveToSourcePath(fromTextures));
    }

    if (const std::filesystem::path input(resolved); input.is_absolute())
        resolved = ToProjectRelativeSourcePath(input);

    return resolved;
}

std::string ToLowerExtension(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    for (char& c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

AssetType InferAssetType(const std::string& sourcePath)
{
    const std::string ext = ToLowerExtension(std::filesystem::path(sourcePath));
    if (ext == ".glsl" || ext == ".frag" || ext == ".vert" || ext == ".comp")
        return AssetType::Shader;
    if (ext == ".mat" || ext == ".material")
        return AssetType::Material;
    if (ext == ".scene")
        return AssetType::Scene;
    if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".dae" || ext == ".3ds")
        return AssetType::Model;
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr")
        return AssetType::Texture2D;
    LogA(LogLevel::WARNING, "Unknown asset extension '{}', defaulting type to Texture2D", ext);
    return AssetType::Texture2D;
}

const char* AssetTypeToString(AssetType type)
{
    switch (type)
    {
    case AssetType::Model:
        return "model";
    case AssetType::Texture2D:
        return "texture";
    case AssetType::TextureCube:
        return "cubemap";
    case AssetType::Material:
        return "material";
    case AssetType::Shader:
        return "shader";
    case AssetType::Scene:
        return "scene";
    }
    return "texture";
}

bool ParseAssetType(const std::string& token, AssetType& outType)
{
    std::string lower = token;
    for (char& c : lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower == "model")
    {
        outType = AssetType::Model;
        return true;
    }
    if (lower == "texture" || lower == "texture2d")
    {
        outType = AssetType::Texture2D;
        return true;
    }
    if (lower == "cubemap" || lower == "texturecube")
    {
        outType = AssetType::TextureCube;
        return true;
    }
    if (lower == "material")
    {
        outType = AssetType::Material;
        return true;
    }
    if (lower == "shader")
    {
        outType = AssetType::Shader;
        return true;
    }
    if (lower == "scene")
    {
        outType = AssetType::Scene;
        return true;
    }
    return false;
}

std::string GenerateGuid()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist;
    const uint32_t a = dist(gen);
    const uint32_t b = dist(gen);
    const uint32_t c = dist(gen);
    const uint32_t d = dist(gen);
    const uint32_t bVersion = (b & 0xFFFF0FFFu) | 0x00004000u;
    const uint32_t cVariant = (c & 0x3FFFFFFFu) | 0x80000000u;
    char buf[37];
    std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%04x%08x", a, (bVersion >> 16) & 0xFFFFu,
                  bVersion & 0xFFFFu, (cVariant >> 16) & 0xFFFFu, cVariant & 0xFFFFu, d);
    return buf;
}

nlohmann::json MetaToJson(const AssetMeta& meta)
{
    nlohmann::json j;
    j["guid"] = meta.guid;
    j["type"] = AssetTypeToString(meta.type);
    j["source"] = meta.sourcePath;
    j["importSettings"] = meta.importSettings.is_null() ? nlohmann::json::object() : meta.importSettings;
    j["dependencies"] = meta.dependencies;
    return j;
}

bool ParseMetaJson(const nlohmann::json& j, const std::string& fallbackSourcePath, AssetMeta& outMeta)
{
    if (!j.is_object())
        return false;

    outMeta = AssetMeta{};
    outMeta.sourcePath = NormalizeAssetPath(fallbackSourcePath);

    if (j.contains("guid"))
        outMeta.guid = j.at("guid").get<std::string>();
    if (j.contains("source"))
        outMeta.sourcePath = NormalizeAssetPath(j.at("source").get<std::string>());
    else if (j.contains("sourcePath"))
        outMeta.sourcePath = NormalizeAssetPath(j.at("sourcePath").get<std::string>());
    else
        outMeta.sourcePath = NormalizeAssetPath(fallbackSourcePath);

    if (j.contains("type"))
    {
        AssetType parsed = AssetType::Texture2D;
        if (!ParseAssetType(j.at("type").get<std::string>(), parsed))
        {
            LogA(LogLevel::WARNING, "Meta '{}': invalid type, infer from source path", fallbackSourcePath);
            outMeta.type = InferAssetType(outMeta.sourcePath);
        }
        else
        {
            outMeta.type = parsed;
        }
    }
    else
    {
        outMeta.type = InferAssetType(outMeta.sourcePath);
    }

    if (j.contains("importSettings"))
        outMeta.importSettings = j.at("importSettings");
    else
        outMeta.importSettings = nlohmann::json::object();

    outMeta.dependencies.clear();
    if (j.contains("dependencies") && j.at("dependencies").is_array())
    {
        for (const auto& dep : j.at("dependencies"))
            outMeta.dependencies.push_back(dep.get<std::string>());
    }

    return !outMeta.guid.empty() && !outMeta.sourcePath.empty();
}

bool HasImportKey(const AssetMeta& meta, const char* key)
{
    return key != nullptr && meta.importSettings.is_object() && meta.importSettings.contains(key);
}

bool IsFloatTextureExtensionImpl(const std::string& sourcePath)
{
    const std::string ext = ToLowerExtension(std::filesystem::path(sourcePath));
    return ext == ".hdr" || ext == ".exr";
}

bool DefaultCubemapSrgb(const std::string& sourcePath)
{
    if (IsFloatTextureExtensionImpl(sourcePath))
        return false;
    return Util::TextureLoadUsesSrgb(sourcePath);
}

bool DefaultTextureGenerateMipsImpl(AssetType type, const std::string& sourcePath)
{
    (void)sourcePath;
    if (type == AssetType::TextureCube)
        return true;
    return type == AssetType::Texture2D;
}
} // namespace

bool AssetDataBase::DefaultTextureGenerateMips(AssetType type, const std::string& sourcePath)
{
    return DefaultTextureGenerateMipsImpl(type, sourcePath);
}

bool AssetDataBase::EnsureTextureImportSettings(AssetMeta& meta, const std::string& sourcePath)
{
    bool changed = false;
    if (!HasImportKey(meta, "showInUi"))
    {
        SetImportBool(meta, "showInUi", true);
        changed = true;
    }
    if (!HasImportKey(meta, "srgb"))
    {
        SetImportBool(meta, "srgb", DefaultCubemapSrgb(sourcePath));
        changed = true;
    }
    if (!HasImportKey(meta, "generateMips"))
    {
        SetImportBool(meta, "generateMips", DefaultTextureGenerateMipsImpl(meta.type, sourcePath));
        changed = true;
    }
    if (!HasImportKey(meta, "wrapMode"))
    {
        SetImportString(meta, "wrapMode", "Repeat");
        changed = true;
    }
    if (!HasImportKey(meta, "filterMode"))
    {
        SetImportString(meta, "filterMode", "Linear");
        changed = true;
    }
    return changed;
}

namespace
{

AssetMeta CreateDefaultMeta(const std::string& sourcePath)
{
    AssetMeta meta;
    meta.guid = GenerateGuid();
    meta.type = InferAssetType(sourcePath);
    meta.sourcePath = sourcePath;
    meta.importSettings = nlohmann::json::object();
    meta.importSettings["showInUi"] = true;
    if (meta.type == AssetType::Texture2D || meta.type == AssetType::TextureCube)
    {
        meta.importSettings["srgb"] = DefaultCubemapSrgb(sourcePath);
        meta.importSettings["generateMips"] = DefaultTextureGenerateMipsImpl(meta.type, sourcePath);
        meta.importSettings["wrapMode"] = "Repeat";
        meta.importSettings["filterMode"] = "Linear";
    }
    if (meta.type == AssetType::TextureCube)
    {
        meta.importSettings["kPhiSamples"] = 64;
        meta.importSettings["kThetaSamples"] = 32;
        meta.importSettings["kIrradianceFaceSize"] = 32;
        meta.importSettings["kPrefilterFaceSize"] = 0;
        meta.importSettings["kMinPrefilterFaceSize"] = 512;
    }
    meta.dependencies.clear();
    return meta;
}
bool LooksLikeGuid(const std::string& text)
{
    if (text.size() != 36)
        return false;
    return text[8] == '-' && text[13] == '-' && text[18] == '-' && text[23] == '-';
}
} // namespace

std::string AssetDataBase::NormalizeSourcePath(const std::string& path)
{
    if (path.empty())
        return {};
    return NormalizeAssetPath(std::filesystem::path(path));
}

std::string AssetDataBase::CanonicalizeMetaSourcePath(const std::string& sourcePath)
{
    return CanonicalizeMetaSourcePathImpl(sourcePath);
}

bool AssetDataBase::GetImportBool(const AssetMeta& meta, const char* key, bool defaultValue)
{
    if (key == nullptr || !meta.importSettings.is_object() || !meta.importSettings.contains(key))
        return defaultValue;
    const nlohmann::json& value = meta.importSettings.at(key);
    if (value.is_boolean())
        return value.get<bool>();
    if (value.is_number_integer())
        return value.get<int>() != 0;
    if (value.is_number_unsigned())
        return value.get<unsigned>() != 0;
    if (value.is_string())
    {
        std::string lower = value.get<std::string>();
        for (char& c : lower)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (lower == "true" || lower == "1" || lower == "yes" || lower == "on")
            return true;
        if (lower == "false" || lower == "0" || lower == "no" || lower == "off")
            return false;
    }
    return defaultValue;
}

void AssetDataBase::SetImportBool(AssetMeta& meta, const char* key, bool value)
{
    if (key == nullptr)
        return;
    if (!meta.importSettings.is_object())
        meta.importSettings = nlohmann::json::object();
    meta.importSettings[key] = value;
}

int AssetDataBase::GetImportInt(const AssetMeta& meta, const char* key, int defaultValue)
{
    if (key == nullptr || !meta.importSettings.is_object() || !meta.importSettings.contains(key))
        return defaultValue;
    const nlohmann::json& value = meta.importSettings.at(key);
    if (value.is_number_integer())
        return value.get<int>();
    if (value.is_number_unsigned())
        return static_cast<int>(value.get<unsigned>());
    if (value.is_number_float())
        return static_cast<int>(value.get<double>());
    if (value.is_boolean())
        return value.get<bool>() ? 1 : 0;
    return defaultValue;
}

void AssetDataBase::SetImportInt(AssetMeta& meta, const char* key, int value)
{
    if (key == nullptr)
        return;
    if (!meta.importSettings.is_object())
        meta.importSettings = nlohmann::json::object();
    meta.importSettings[key] = value;
}

std::string AssetDataBase::GetImportString(const AssetMeta& meta, const char* key)
{
    if (key == nullptr || !meta.importSettings.is_object() || !meta.importSettings.contains(key))
        return {};
    const nlohmann::json& value = meta.importSettings.at(key);
    if (value.is_string())
        return value.get<std::string>();
    return {};
}

void AssetDataBase::SetImportString(AssetMeta& meta, const char* key, const std::string& value)
{
    if (key == nullptr)
        return;
    if (!meta.importSettings.is_object())
        meta.importSettings = nlohmann::json::object();
    if (value.empty())
        meta.importSettings.erase(key);
    else
        meta.importSettings[key] = value;
}

void AssetDataBase::UpdateImportBool(const std::string& sourcePath, const char* key, bool value)
{
    AssetMeta meta = LoadOrCreateMeta(sourcePath);
    SetImportBool(meta, key, value);
    SaveMeta(meta);
}

void AssetDataBase::UpdateDependencies(const std::string& sourcePath, const std::vector<std::string>& dependencies)
{
    AssetMeta meta = LoadOrCreateMeta(sourcePath);
    std::unordered_set<std::string> merged(meta.dependencies.begin(), meta.dependencies.end());
    for (const std::string& dep : dependencies)
    {
        if (dep.empty())
            continue;
        const std::string canonical = CanonicalizeMetaSourcePathImpl(dep);
        if (!canonical.empty())
            merged.insert(canonical);
    }
    meta.dependencies.assign(merged.begin(), merged.end());
    SaveMeta(meta);
}

bool AssetDataBase::IsFloatTextureExtension(const std::string& sourcePath)
{
    return IsFloatTextureExtensionImpl(sourcePath);
}

namespace
{
constexpr int kDefaultIrradiancePhiSamples = 64;
constexpr int kDefaultIrradianceThetaSamples = 32;
constexpr int kDefaultIrradianceFaceSize = 32;
constexpr int kDefaultPrefilterFaceSize = 0;
constexpr int kDefaultMinPrefilterFaceSize = 512;

bool EnsureIBLBakeImportSettings(AssetMeta& meta)
{
    bool changed = false;
    if (!HasImportKey(meta, "kPhiSamples"))
    {
        AssetDataBase::SetImportInt(meta, "kPhiSamples", kDefaultIrradiancePhiSamples);
        changed = true;
    }
    if (!HasImportKey(meta, "kThetaSamples"))
    {
        AssetDataBase::SetImportInt(meta, "kThetaSamples", kDefaultIrradianceThetaSamples);
        changed = true;
    }
    if (!HasImportKey(meta, "kIrradianceFaceSize"))
    {
        AssetDataBase::SetImportInt(meta, "kIrradianceFaceSize", kDefaultIrradianceFaceSize);
        changed = true;
    }
    if (!HasImportKey(meta, "kPrefilterFaceSize"))
    {
        AssetDataBase::SetImportInt(meta, "kPrefilterFaceSize", kDefaultPrefilterFaceSize);
        changed = true;
    }
    if (!HasImportKey(meta, "kMinPrefilterFaceSize"))
    {
        AssetDataBase::SetImportInt(meta, "kMinPrefilterFaceSize", kDefaultMinPrefilterFaceSize);
        changed = true;
    }
    return changed;
}
} // namespace

bool AssetDataBase::EnsureCubemapImportSettings(AssetMeta& meta, const std::string& sourcePath)
{
    bool changed = false;
    if (meta.type != AssetType::TextureCube)
    {
        meta.type = AssetType::TextureCube;
        changed = true;
    }
    if (EnsureTextureImportSettings(meta, sourcePath))
        changed = true;
    if (EnsureIBLBakeImportSettings(meta))
        changed = true;
    if (meta.importSettings.contains("hdr"))
    {
        meta.importSettings.erase("hdr");
        changed = true;
    }
    if (meta.importSettings.contains("layout"))
    {
        meta.importSettings.erase("layout");
        changed = true;
    }
    return changed;
}

void AssetDataBase::EnsureCubemapMeta(const std::string& sourcePath, bool srgb)
{
    AssetMeta meta = LoadOrCreateMeta(sourcePath);
    EnsureCubemapImportSettings(meta, sourcePath);
    SetImportBool(meta, "srgb", srgb);
    SaveMeta(meta);
}

void AssetDataBase::RemoveMeta(const std::string& sourcePath)
{
    const std::string path = NormalizeSourcePath(sourcePath);
    if (path.empty())
        return;

    const auto it = GetRegistry().metaBySourcePath.find(path);
    if (it == GetRegistry().metaBySourcePath.end())
        return;

    if (!it->second.guid.empty())
        GetRegistry().guidToSourcePath.erase(it->second.guid);
    GetRegistry().metaBySourcePath.erase(it);
}

AssetDataBase::AssetDataBase()
{
    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();

    for (const std::string& root : AssetPaths::Get().GetContentScanRoots())
        ScanAllMeta(root);
}
AssetDataBase::~AssetDataBase() = default;

AssetDataBase& AssetDataBase::Get()
{
    static AssetDataBase instance;
    return instance;
}

AssetMeta AssetDataBase::LoadOrCreateMeta(const std::string& sourcePath)
{
    const std::string path = CanonicalizeMetaSourcePathImpl(sourcePath);
    if (path.empty())
    {
        LogA(LogLevel::ERROR, "LoadOrCreateMeta: empty source path");
        return {};
    }

    if (!AssetSourceFileExists(path))
    {
        LogA(LogLevel::WARNING, "LoadOrCreateMeta: asset file missing, skip meta for '{}' (input='{}')", path,
             sourcePath);
        return {};
    }

    const auto cached = GetRegistry().metaBySourcePath.find(path);
    if (cached != GetRegistry().metaBySourcePath.end())
        return cached->second;

    const std::string metaPath = MetaFilePath(path);
    AssetMeta meta;
    if (LoadMetaFromFile(metaPath, path, meta))
    {
        meta.sourcePath = CanonicalizeMetaSourcePathImpl(meta.sourcePath.empty() ? path : meta.sourcePath);
        RegisterMeta(meta);
        LogA(LogLevel::INFO, "Loaded meta: '{}' guid={}", path, meta.guid);
        return meta;
    }

    std::error_code existsEc;
    const std::string metaIoPath = AssetPaths::Get().ToAbsolutePath(metaPath);
    if (std::filesystem::exists(metaIoPath, existsEc))
    {
        LogA(LogLevel::WARNING, "Meta file '{}' invalid or unreadable, using defaults in memory (file not overwritten)",
             metaPath);
        meta = CreateDefaultMeta(path);
        RegisterMeta(meta);
        return meta;
    }

    meta = CreateDefaultMeta(path);
    SaveMeta(meta);
    LogA(LogLevel::INFO, "Created meta: '{}' guid={}", path, meta.guid);
    return meta;
}

bool AssetDataBase::RefreshMetaFromDisk(const std::string& sourcePath)
{
    const std::string path = CanonicalizeMetaSourcePathImpl(sourcePath);
    if (path.empty() || !AssetSourceFileExists(path))
        return false;

    const std::string metaPath = MetaFilePath(path);
    AssetMeta meta;
    if (!LoadMetaFromFile(metaPath, path, meta))
        return false;

    meta.sourcePath = CanonicalizeMetaSourcePathImpl(meta.sourcePath.empty() ? path : meta.sourcePath);
    RegisterMeta(meta);
    return true;
}

bool AssetDataBase::TryGetMeta(const std::string& sourcePath, AssetMeta& outMeta) const
{
    const std::string path = CanonicalizeMetaSourcePathImpl(sourcePath);
    if (path.empty())
        return false;
    const auto it = GetRegistry().metaBySourcePath.find(path);
    if (it == GetRegistry().metaBySourcePath.end())
        return false;
    outMeta = it->second;
    return true;
}

std::string AssetDataBase::ResolveAssetRef(const std::string& guidOrPath)
{
    if (guidOrPath.empty())
        return {};
    if (LooksLikeGuid(guidOrPath))
    {
        const std::string path = ResolveGuid(guidOrPath);
        return path.empty() ? std::string{} : CanonicalizeMetaSourcePathImpl(path);
    }

    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();
    return CanonicalizeMetaSourcePathImpl(guidOrPath);
}

std::string AssetDataBase::ResolveGuid(const std::string& guid)
{
    if (guid.empty())
        return {};

    const auto it = GetRegistry().guidToSourcePath.find(guid);
    if (it != GetRegistry().guidToSourcePath.end())
        return it->second;

    LogA(LogLevel::WARNING, "ResolveGuid: unknown guid '{}'", guid);
    return {};
}

void AssetDataBase::SaveMeta(const AssetMeta& meta)
{
    AssetMeta normalized = meta;
    normalized.sourcePath = CanonicalizeMetaSourcePathImpl(meta.sourcePath);
    if (normalized.sourcePath.empty())
    {
        LogA(LogLevel::ERROR, "SaveMeta: empty source path");
        return;
    }
    if (!AssetSourceFileExists(normalized.sourcePath))
    {
        LogA(LogLevel::WARNING, "SaveMeta: asset file missing, skip meta for '{}'", normalized.sourcePath);
        return;
    }
    if (normalized.guid.empty())
    {
        LogA(LogLevel::ERROR, "SaveMeta: empty guid for '{}'", normalized.sourcePath);
        return;
    }

    const std::string metaPath = MetaFilePath(normalized.sourcePath);
    std::ofstream ofs(AssetPaths::Get().ToAbsolutePath(metaPath));
    if (!ofs)
    {
        LogA(LogLevel::ERROR, "SaveMeta: cannot write '{}'", metaPath);
        return;
    }

    try
    {
        ofs << MetaToJson(normalized).dump(4);
    }
    catch (const nlohmann::json::exception& e)
    {
        LogA(LogLevel::ERROR, "SaveMeta: JSON serialize failed for '{}': {}", normalized.sourcePath, e.what());
        return;
    }

    RegisterMeta(normalized);
    LogA(LogLevel::INFO, "Saved meta: '{}'", metaPath);
}

size_t AssetDataBase::ScanAllMeta(const std::string& rootPath)
{
    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();

    std::error_code ec;
    const std::filesystem::path root(AssetPaths::Get().ToAbsolutePath(rootPath));
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec))
    {
        LogA(LogLevel::WARNING, "ScanAllMeta: root '{}' missing or not a directory", rootPath);
        return 0;
    }

    size_t loaded = 0;
    size_t skipped = 0;
    size_t removed = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, ec))
    {
        if (ec)
        {
            LogA(LogLevel::WARNING, "ScanAllMeta: directory iteration error under '{}'", rootPath);
            break;
        }
        if (!entry.is_regular_file(ec))
            continue;
        if (ToLowerExtension(entry.path()) != ".meta")
            continue;

        const std::filesystem::path metaFilePath = entry.path();
        const std::string sourcePath = SourcePathFromMetaFile(metaFilePath);
        const std::string metaPath = NormalizeAssetPath(metaFilePath);

        AssetMeta meta;
        if (!LoadMetaFromFile(metaPath, sourcePath, meta))
        {
            ++skipped;
            LogA(LogLevel::WARNING, "ScanAllMeta: skip invalid meta '{}'", metaPath);
            continue;
        }

        meta.sourcePath = CanonicalizeMetaSourcePathImpl(meta.sourcePath.empty() ? sourcePath : meta.sourcePath);
        if (!std::filesystem::exists(AssetPaths::Get().ToAbsolutePath(meta.sourcePath), ec))
        {
            RemoveMeta(meta.sourcePath);
            if (std::filesystem::remove(metaFilePath, ec) && !ec)
            {
                ++removed;
                LogA(LogLevel::INFO, "ScanAllMeta: removed orphan meta '{}' (asset '{}' missing)", metaPath,
                     meta.sourcePath);
            }
            else
            {
                LogA(LogLevel::WARNING, "ScanAllMeta: failed to remove orphan meta '{}' (asset '{}' missing)",
                     metaPath, meta.sourcePath);
            }
            continue;
        }

        RegisterMeta(meta);
        ++loaded;
    }

    LogA(LogLevel::INFO, "ScanAllMeta: indexed {} meta file(s) under '{}' ({} skipped, {} orphan removed)", loaded,
         rootPath, skipped, removed);
    return loaded;
}

#undef MODULE

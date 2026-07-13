#include "LoaderManager.h"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <sys/stat.h>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "../Core/Log.h"
#include "Types/ComputeShader.h"
#include "Types/Material.h"
#include "Types/Mesh.h"
#include "Types/Model.h"
#include "Types/RenderQueue.h"
#include "Types/Shader.h"
#include "Types/ShaderPass.h"
#include "Types/ShaderTags.h"
#include "Types/Texture/IBLImage.h"
#include "Types/Texture/Texture.h"
#include "Types/Texture/Texture2D.h"
#include "Types/Texture/TextureCube.h"

#include "../../../external/stb/stb_image.h"
#include "../Core/Config.h"
#include "../Core/util.h"
#include "AssetDatabase.h"
#include "AssetManager.h"
#include "AssetPaths.h"
#include "glm/common.hpp"

#include "../Renderer/CubemapBaker.h"
#include "../Renderer/FramebufferDesc.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/glm.hpp"
#include "glm/trigonometric.hpp"

#include "glm/matrix.hpp"
#include <glm/gtc/matrix_transform.hpp>

#define MODULE "LoaderManager"

int64_t get_mtime(const std::string& filePath)
{
    struct stat result;
    if (stat(filePath.c_str(), &result) == 0)
        return result.st_mtime;

    if (AssetPaths::IsInitialized())
    {
        const std::string absolutePath = AssetPaths::Get().ToAbsolutePath(filePath);
        if (!absolutePath.empty() && absolutePath != filePath && stat(absolutePath.c_str(), &result) == 0)
            return result.st_mtime;
    }
    return -1;
}

void stringToLower(std::string& str)
{
    std::transform(str.begin(), str.end(), str.begin(), [](char& c) { return static_cast<char>(std::tolower(c)); });
}

std::string getBlockContent(const std::string& content, const int startPoint)
{
    std::string blockContent;
    bool blockStartFlag = false;
    int bracketCount = 0;
    for (char c : content.substr(startPoint))
    {
        if (c == '{' and not blockStartFlag)
        {
            blockStartFlag = true;
            bracketCount += 1;
            continue;
        }
        else if (c == '{')
            bracketCount += 1;
        else if (c == '}')
            bracketCount -= 1;
        if (bracketCount == 0 and blockStartFlag)
            break;
        if (blockStartFlag)
            blockContent += c;
    }
    return blockContent;
}

template <typename T> FileState<T>::FileState() = default;

template <typename T> FileState<T>::FileState(const std::string& path)
{
    this->path = path;
    UpdateModifyTime();
}

template <typename T> void FileState<T>::AddRelativeAsset(std::shared_ptr<T> asset)
{
    if (!asset)
        return;
    for (const auto& existing : relativeAsset)
    {
        if (existing == asset)
            return;
    }
    relativeAsset.push_back(std::move(asset));
}

template <typename T> void FileState<T>::UpdateModifyTime()
{
    mTime = get_mtime(path);
    mMetaTime = get_mtime(path + ".meta");
}

template <typename T> bool FileState<T>::IsNeedReload()
{
    if (get_mtime(path) != mTime)
        return true;
    if (get_mtime(path + ".meta") != mMetaTime)
        return true;
    return false;
}

LoaderManager& LoaderManager::Get()
{
    static LoaderManager instance;
    return instance;
}

LoaderManager::~LoaderManager() = default;

Func DeterminFuncByString(const std::string& state)
{
    if (state == "always")
        return Func::ALWAYS;
    else if (state == "never")
        return Func::NEVER;
    else if (state == "less")
        return Func::LESS;
    else if (state == "equal")
        return Func::EQUAL;
    else if (state == "lequal")
        return Func::LEQUAL;
    else if (state == "greater")
        return Func::GREATER;
    else if (state == "notequal")
        return Func::NOTEQUAL;
    else if (state == "gequal")
        return Func::GEQUAL;

    return Func::ALWAYS;
}

static inline std::string Trim(std::string s)
{
    if (!s.empty() && s.back() == '\r')
        s.pop_back();
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    return s;
}

static inline std::vector<std::string> TokenizeTrimmed(const std::string& s)
{
    std::vector<std::string> out;
    std::istringstream iss(Trim(s));
    std::string tok;
    while (iss >> tok)
        out.push_back(std::move(tok));
    return out;
}

static int FindPassBlockStart(const std::string& textLower)
{
    for (size_t pos = 0; pos + 4 <= textLower.size();)
    {
        const size_t found = textLower.find("pass", pos);
        if (found == std::string::npos)
            return -1;
        const size_t after = found + 4;
        if (after < textLower.size() &&
            (std::isalnum(static_cast<unsigned char>(textLower[after])) || textLower[after] == '_'))
        {
            pos = found + 1;
            continue;
        }
        size_t i = after;
        while (i < textLower.size() && std::isspace(static_cast<unsigned char>(textLower[i])))
            ++i;
        if (i >= textLower.size() || textLower[i] == '{')
            return static_cast<int>(found);
        pos = found + 1;
    }
    return -1;
}

static bool ParseCommaFloats(const std::string& rhs, std::vector<float>& out)
{
    std::string s = rhs;
    if (!s.empty() && s.front() == '(' && s.back() == ')')
        s = s.substr(1, s.size() - 2);
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, ','))
    {
        part = Trim(part);
        if (part.empty())
            continue;
        try
        {
            out.push_back(std::stof(part));
        }
        catch (...)
        {
            return false;
        }
    }
    return !out.empty();
}

static std::string StripLineComment(const std::string& line)
{
    const size_t pos = line.find("//");
    if (pos == std::string::npos)
        return Trim(line);
    return Trim(line.substr(0, pos));
}

static bool TryParseVecConstructor(const std::string& expr, int components, std::vector<float>& out)
{
    std::string lower = expr;
    stringToLower(lower);
    const std::string tag = "vec" + std::to_string(components) + "(";
    const size_t pos = lower.find(tag);
    if (pos == std::string::npos)
        return false;

    const size_t start = pos + tag.size();
    const size_t end = expr.find(')', start);
    if (end == std::string::npos)
        return false;

    std::vector<float> comps;
    if (!ParseCommaFloats(expr.substr(start, end - start), comps))
        return false;

    if (comps.size() == 1)
        out.assign(static_cast<size_t>(components), comps[0]);
    else if (static_cast<int>(comps.size()) == components)
        out = std::move(comps);
    else
        return false;

    return true;
}

static bool TryParseScalarTimesVec(const std::string& expr, int components, std::vector<float>& out)
{
    const size_t mul = expr.find('*');
    if (mul == std::string::npos)
        return false;

    std::string scalarPart = Trim(expr.substr(0, mul));
    std::string vecPart = Trim(expr.substr(mul + 1));
    if (scalarPart.empty() || vecPart.empty())
        return false;

    std::vector<float> comps;
    if (!TryParseVecConstructor(vecPart, components, comps))
        return false;

    float scalar = 0.0f;
    try
    {
        scalar = std::stof(scalarPart);
    }
    catch (...)
    {
        return false;
    }

    for (float& c : comps)
        c *= scalar;
    out = std::move(comps);
    return true;
}

static bool ParseVecComponents(const std::string& expr, int components, std::vector<float>& out)
{
    if (expr.empty())
        return false;
    if (TryParseScalarTimesVec(expr, components, out))
        return true;
    if (TryParseVecConstructor(expr, components, out))
        return true;
    return ParseCommaFloats(expr, out) && static_cast<int>(out.size()) >= components;
}

static bool AssignMaterialPropertyFromInitializer(const std::string& typeTok, const std::string& initExpr,
                                                  MaterialProperty& outProp)
{
    if (typeTok == "float")
    {
        outProp.type = MaterialPropertyType::Float;
        float v = 0.0f;
        if (!initExpr.empty())
        {
            try
            {
                v = std::stof(initExpr);
            }
            catch (...)
            {
                return false;
            }
        }
        outProp.value = v;
        return true;
    }
    if (typeTok == "int")
    {
        outProp.type = MaterialPropertyType::Int;
        int v = 0;
        if (!initExpr.empty())
        {
            try
            {
                v = std::stoi(initExpr);
            }
            catch (...)
            {
                return false;
            }
        }
        outProp.value = v;
        return true;
    }
    if (typeTok == "bool")
    {
        outProp.type = MaterialPropertyType::Bool;
        bool v = false;
        if (!initExpr.empty())
        {
            std::string b = initExpr;
            stringToLower(b);
            v = (b == "true" || b == "1" || b == "on");
        }
        outProp.value = v;
        return true;
    }
    if (typeTok == "vec2")
    {
        outProp.type = MaterialPropertyType::Vec2;
        glm::vec2 v(0.0f);
        std::vector<float> comps;
        if (!initExpr.empty() && ParseVecComponents(initExpr, 2, comps))
            v = glm::vec2(comps[0], comps[1]);
        outProp.value = v;
        return true;
    }
    if (typeTok == "vec3")
    {
        outProp.type = MaterialPropertyType::Vec3;
        glm::vec3 v(0.0f);
        std::vector<float> comps;
        if (!initExpr.empty() && ParseVecComponents(initExpr, 3, comps))
            v = glm::vec3(comps[0], comps[1], comps[2]);
        outProp.value = v;
        return true;
    }
    if (typeTok == "vec4")
    {
        outProp.type = MaterialPropertyType::Vec4;
        glm::vec4 v(0.0f);
        std::vector<float> comps;
        if (!initExpr.empty() && ParseVecComponents(initExpr, 4, comps))
            v = glm::vec4(comps[0], comps[1], comps[2], comps[3]);
        else if (initExpr.empty())
            v = glm::vec4(1.0f);
        outProp.value = v;
        return true;
    }
    if (typeTok == "sampler2d")
    {
        outProp.type = MaterialPropertyType::Texture2D;
        outProp.value = std::shared_ptr<Texture>{};
        return true;
    }
    if (typeTok == "samplercube")
    {
        outProp.type = MaterialPropertyType::TextureCube;
        outProp.value = std::shared_ptr<Texture>{};
        return true;
    }
    return false;
}

static bool ParseMaterialPropertyLine(const std::string& rawLine, MaterialProperty& outProp)
{
    const std::string line = Trim(rawLine);
    if (line.empty() || line[0] == '{' || line[0] == '}')
        return false;

    const auto tokens = TokenizeTrimmed(line);
    if (tokens.size() < 2)
        return false;

    std::string typeTok = tokens[0];
    stringToLower(typeTok);

    outProp.name = tokens[1];
    outProp.isDirty = true;
    outProp.hasExplicitDefault = false;

    std::string defaultRhs;
    const size_t eq = line.find('=');
    if (eq != std::string::npos)
        defaultRhs = Trim(line.substr(eq + 1));

    if (!AssignMaterialPropertyFromInitializer(typeTok, defaultRhs, outProp))
        return false;

    if (outProp.type == MaterialPropertyType::Texture2D || outProp.type == MaterialPropertyType::TextureCube)
    {
        if (eq != std::string::npos)
        {
            outProp.defaultTexturePath = Util::ResolveShaderDefaultTexturePath(defaultRhs);
            outProp.hasExplicitDefault = !outProp.defaultTexturePath.empty();
        }
        return true;
    }

    if (eq != std::string::npos)
        outProp.hasExplicitDefault = true;
    return true;
}

static bool ParseGlslUniformLine(const std::string& rawLine, MaterialProperty& outProp)
{
    std::string line = StripLineComment(rawLine);
    if (line.find("uniform") == std::string::npos)
        return false;

    const size_t uniformPos = line.find("uniform");
    line = Trim(line.substr(uniformPos + 7));
    while (!line.empty() && line.back() == ';')
        line.pop_back();

    const auto tokens = TokenizeTrimmed(line);
    if (tokens.size() < 2)
        return false;

    std::string typeTok = tokens[0];
    stringToLower(typeTok);
    if (typeTok == "sampler2d" || typeTok == "samplercube")
        return false;

    outProp.name = tokens[1];
    const size_t bracket = outProp.name.find('[');
    if (bracket != std::string::npos)
        outProp.name = outProp.name.substr(0, bracket);

    const size_t eq = line.find('=');
    if (eq == std::string::npos)
        return false;

    std::string initExpr = Trim(line.substr(eq + 1));
    while (!initExpr.empty() && initExpr.back() == ';')
        initExpr.pop_back();

    outProp.isDirty = true;
    outProp.hasExplicitDefault = false;
    return AssignMaterialPropertyFromInitializer(typeTok, initExpr, outProp);
}

static void ApplyGlslUniformDefaults(const std::string& source,
                                     std::unordered_map<std::string, MaterialProperty>& propertiesMap)
{
    std::istringstream iss(source);
    std::string line;
    while (std::getline(iss, line))
    {
        MaterialProperty parsed;
        if (!ParseGlslUniformLine(line, parsed))
            continue;

        auto it = propertiesMap.find(parsed.name);
        if (it == propertiesMap.end())
            continue;
        if (it->second.type != parsed.type)
            continue;
        if (it->second.hasExplicitDefault)
            continue;

        it->second.value = parsed.value;
    }
}

static void ApplyGlslUniformDefaultsFromPasses(const std::vector<PassCode>& codes,
                                               std::unordered_map<std::string, MaterialProperty>& propertiesMap)
{
    for (const PassCode& code : codes)
    {
        ApplyGlslUniformDefaults(code.VertexShader, propertiesMap);
        ApplyGlslUniformDefaults(code.FragmentShader, propertiesMap);
        if (!code.GeometryShader.empty())
            ApplyGlslUniformDefaults(code.GeometryShader, propertiesMap);
    }
}

StencilOp DeterminOpByString(const std::string& state)
{
    if (state == "keep")
        return StencilOp::KEEP;
    else if (state == "zero")
        return StencilOp::ZERO;
    else if (state == "replace")
        return StencilOp::REPLACE;
    else if (state == "incr")
        return StencilOp::INCR;
    else if (state == "incrwrap")
        return StencilOp::INCR_WRAP;
    else if (state == "decr")
        return StencilOp::DECR;
    else if (state == "decrwrap")
        return StencilOp::DECR_WRAP;
    else if (state == "invert")
        return StencilOp::INVERT;
    return StencilOp::KEEP;
}

BlendFunc DeterminBlendFunc(const std::string& state)
{
    if (state == "zero")
        return BlendFunc::ZERO;
    else if (state == "one")
        return BlendFunc::ONE;
    else if (state == "srccolor")
        return BlendFunc::SRC_COLOR;
    else if (state == "oneminussrccolor" || state == "oneminuscrccolor")
        return BlendFunc::ONE_MINUS_SRC_COLOR;
    else if (state == "dstcolor")
        return BlendFunc::DST_COLOR;
    else if (state == "oneminusdstcolor")
        return BlendFunc::ONE_MINUS_DST_COLOR;
    else if (state == "srcalpha")
        return BlendFunc::SRC_ALPHA;
    else if (state == "oneminussrcalpha")
        return BlendFunc::ONE_MINUS_SRC_ALPHA;
    else if (state == "dstalpha")
        return BlendFunc::DST_ALPHA;
    else if (state == "oneminusdstalpha")
        return BlendFunc::ONE_MINUS_DST_ALPHA;
    return BlendFunc::ZERO;
}

BlendEq DeterminBlendEquation(const std::string& state)
{
    if (state == "add")
        return BlendEq::ADD;
    else if (state == "subtract")
        return BlendEq::SUBTRACT;
    else if (state == "reversesubtract")
        return BlendEq::REVERSE_SUBTRACT;
    else if (state == "min")
        return BlendEq::MIN;
    else if (state == "max")
        return BlendEq::MAX;
    return BlendEq::ADD;
}

static inline std::vector<std::string> TokenizeLowerTrimmed(const std::string& s)
{
    std::string t = Trim(s);
    std::vector<std::string> out;
    out.reserve(8);
    std::istringstream iss(t);
    std::string tok;
    while (iss >> tok)
    {
        stringToLower(tok);
        out.push_back(std::move(tok));
    }
    return out;
}

static std::string ExtractTagValueFromLine(const std::string& line)
{
    size_t i = 0;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
        ++i;
    while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])))
        ++i;
    while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])))
        ++i;

    std::string value = line.substr(i);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.pop_back();
    return value;
}

/// 仅解析 SubShader 之前的 Shader 级 Queue（避免 Pass 内 queue 被误读或覆盖 Pass 设置）。
static int ParseShaderLevelRenderQueue(const std::string& shaderContent)
{
    std::string lower = shaderContent;
    stringToLower(lower);
    const size_t subShaderPos = lower.find("subshader");
    const std::string head = subShaderPos == std::string::npos ? shaderContent : shaderContent.substr(0, subShaderPos);

    std::istringstream iss(head);
    std::string line;
    while (std::getline(iss, line))
    {
        const auto tokens = TokenizeLowerTrimmed(line);
        if (tokens.size() < 2 || tokens[0] != "queue")
            continue;

        int queueValue = RenderQueue::Geometry;
        if (RenderQueue::TryParseToken(tokens[1], queueValue))
            return queueValue;
        LogA(LogLevel::WARNING, "Invalid Queue value '{}' in shader file (shader level)", tokens[1]);
    }
    return -1;
}

/// Pass 级 queue 优先；多 Pass 时取最大值（数值越大越晚绘制）。
static int ResolveEffectiveRenderQueue(int shaderLevelQueue, const std::vector<PassOption>& passOptions)
{
    int passQueue = -1;
    for (const PassOption& opt : passOptions)
    {
        if (opt.renderQueue >= 0)
            passQueue = std::max(passQueue, opt.renderQueue);
    }
    if (passQueue >= 0)
        return passQueue;
    return shaderLevelQueue;
}

LoaderManager::LoaderManager()
{
    stbi_set_flip_vertically_on_load(true);
}

void LoaderManager::UpdateAssetFromDisk()
{
    std::vector<std::string> dirtyShaderPaths;
    dirtyShaderPaths.reserve(m_shaderFiles.size());
    for (auto& [path, file] : m_shaderFiles)
    {
        if (file.IsNeedReload())
            dirtyShaderPaths.push_back(path);
    }

    std::vector<std::string> dirtyComputePaths;
    dirtyComputePaths.reserve(m_computeShaderFiles.size());
    for (auto& [path, file] : m_computeShaderFiles)
    {
        if (file.IsNeedReload())
            dirtyComputePaths.push_back(path);
    }

    if (!dirtyShaderPaths.empty())
    {
        LogA(LogLevel::INFO, "Shader hot-reload: {} file(s) changed", dirtyShaderPaths.size());

        std::unordered_set<Shader*> seen;
        std::vector<std::shared_ptr<Shader>> uniqueShaders;
        for (const std::string& path : dirtyShaderPaths)
        {
            auto it = m_shaderFiles.find(path);
            if (it == m_shaderFiles.end())
                continue;
            for (auto& shader : it->second.relativeAsset)
            {
                if (!shader)
                    continue;
                if (seen.insert(shader.get()).second)
                    uniqueShaders.push_back(shader);
            }
        }

        std::unordered_set<Shader*> failed;
        for (auto& shader : uniqueShaders)
        {
            if (!LoadShader(shader->m_path, shader, true))
            {
                failed.insert(shader.get());
                LogA(LogLevel::ERROR, "Shader reload failed: {} ({})", shader->m_name, shader->m_path);
            }
            else
            {
                LogA(LogLevel::INFO, "Shader reload OK: {} ({})", shader->m_name, shader->m_path);
            }
        }

        for (const std::string& path : dirtyShaderPaths)
        {
            auto it = m_shaderFiles.find(path);
            if (it == m_shaderFiles.end())
                continue;
            FileState<Shader>& file = it->second;
            bool anyFailed = false;
            for (auto& shader : file.relativeAsset)
            {
                if (shader && failed.count(shader.get()))
                {
                    anyFailed = true;
                    break;
                }
            }
            if (!anyFailed)
                file.UpdateModifyTime();
        }
    }

    if (dirtyComputePaths.empty())
        return;

    LogA(LogLevel::INFO, "Compute shader hot-reload: {} file(s) changed", dirtyComputePaths.size());

    std::unordered_set<ComputeShader*> seenCompute;
    std::vector<std::shared_ptr<ComputeShader>> uniqueComputeShaders;
    for (const std::string& path : dirtyComputePaths)
    {
        auto it = m_computeShaderFiles.find(path);
        if (it == m_computeShaderFiles.end())
            continue;
        for (auto& shader : it->second.relativeAsset)
        {
            if (!shader)
                continue;
            if (seenCompute.insert(shader.get()).second)
                uniqueComputeShaders.push_back(shader);
        }
    }

    std::unordered_set<ComputeShader*> failedCompute;
    for (auto& shader : uniqueComputeShaders)
    {
        if (!LoadComputeShader(shader->m_path, shader, true))
        {
            failedCompute.insert(shader.get());
            LogA(LogLevel::ERROR, "Compute shader reload failed: {} ({})", shader->m_name, shader->m_path);
        }
        else
        {
            LogA(LogLevel::INFO, "Compute shader reload OK: {} ({})", shader->m_name, shader->m_path);
        }
    }

    for (const std::string& path : dirtyComputePaths)
    {
        auto it = m_computeShaderFiles.find(path);
        if (it == m_computeShaderFiles.end())
            continue;
        FileState<ComputeShader>& file = it->second;
        bool anyFailed = false;
        for (auto& shader : file.relativeAsset)
        {
            if (shader && failedCompute.count(shader.get()))
            {
                anyFailed = true;
                break;
            }
        }
        if (!anyFailed)
            file.UpdateModifyTime();
    }
}

bool LoaderManager::LoadTextureFromFile(const std::string& path, std::shared_ptr<Texture2D>& tex, bool forceReload)
{
    const std::string resolvedPath =
        AssetDataBase::CanonicalizeMetaSourcePath(AssetDataBase::Get().ResolveAssetRef(path));
    if (resolvedPath.empty())
    {
        LogA(LogLevel::ERROR, "LoadTextureFromFile: invalid texture path '{}'", path);
        return false;
    }

    AssetDataBase::Get().RefreshMetaFromDisk(resolvedPath);
    AssetMeta meta = AssetDataBase::Get().LoadOrCreateMeta(resolvedPath);
    if (AssetDataBase::EnsureTextureImportSettings(meta, resolvedPath))
        AssetDataBase::Get().SaveMeta(meta);

    const bool useSrgb = AssetDataBase::GetImportBool(meta, "srgb", Util::TextureLoadUsesSrgb(resolvedPath));
    const bool useGenerateMips = AssetDataBase::GetImportBool(
        meta, "generateMips", AssetDataBase::DefaultTextureGenerateMips(meta.type, resolvedPath));
    const std::string ioPath = AssetPaths::Get().ToAbsolutePath(resolvedPath);

    const auto cached = m_TextureFiles.find(resolvedPath);
    if (cached != m_TextureFiles.end())
    {
        const std::shared_ptr<Texture2D>& existing = cached->second.relativeAsset[0];
        const bool importMatches =
            existing && existing->m_srgb == useSrgb && existing->m_generateMips == useGenerateMips;
        if (!forceReload && !cached->second.IsNeedReload() && importMatches)
        {
            tex = existing;
            return true;
        }
        LogA(LogLevel::INFO, "Reloading texture: {} (srgb={}, generateMips={})", resolvedPath, useSrgb,
             useGenerateMips);
        tex = existing;
    }

    int width, height, channels;
    unsigned char* data = stbi_load(ioPath.c_str(), &width, &height, &channels, 0);
    if (!data)
    {
        LogA(LogLevel::ERROR, "failed to load texture:{}", resolvedPath);
        return false;
    }
    TextureDesc desc = TextureDesc::MakeAuto(width, height, channels, useSrgb);
    desc.generateMips = useGenerateMips;
    if (useGenerateMips)
        desc.sampler.minFilter = GL_LINEAR;
    tex->Create(desc, data);
    tex->m_path = resolvedPath;
    tex->m_name = Util::GetNameFromPath(resolvedPath);
    tex->m_generateMips = useGenerateMips;

    stbi_image_free(data); // 释放内存

    if (cached == m_TextureFiles.end())
    {
        FileState<Texture2D> file(resolvedPath);
        file.AddRelativeAsset(tex);
        m_TextureFiles[resolvedPath] = file;
        LogA(LogLevel::INFO, "Texture loaded: {} ({}x{}, {} ch, srgb={}, generateMips={})", resolvedPath, width, height,
             channels, useSrgb, useGenerateMips);
    }
    else
    {
        cached->second.UpdateModifyTime();
        LogA(LogLevel::INFO, "Texture reloaded: {} ({}x{}, {} ch, srgb={}, generateMips={})", resolvedPath, width,
             height, channels, useSrgb, useGenerateMips);
    }
    return true;
}
bool LoaderManager::LoadIBLMapFromFile(const std::string& path, std::shared_ptr<IBLImage>& ibl, bool forceReload)
{
    if (!ibl)
    {
        LogA(LogLevel::ERROR, "LoadIBLMapFromFile: null IBLImage pointer");
        return false;
    }

    const std::string resolvedPath =
        AssetDataBase::CanonicalizeMetaSourcePath(AssetDataBase::Get().ResolveAssetRef(path));
    if (resolvedPath.empty())
    {
        LogA(LogLevel::ERROR, "LoadIBLMapFromFile: invalid IBL path '{}'", path);
        return false;
    }

    const std::string ioPath = AssetPaths::Get().ToAbsolutePath(resolvedPath);
    AssetDataBase::Get().RefreshMetaFromDisk(resolvedPath);
    AssetMeta meta = AssetDataBase::Get().LoadOrCreateMeta(resolvedPath);
    if (AssetDataBase::EnsureCubemapImportSettings(meta, resolvedPath))
        AssetDataBase::Get().SaveMeta(meta);

    const bool useSrgb = AssetDataBase::GetImportBool(meta, "srgb", Util::TextureLoadUsesSrgb(resolvedPath));
    const bool useGenerateMips = AssetDataBase::GetImportBool(
        meta, "generateMips", AssetDataBase::DefaultTextureGenerateMips(meta.type, resolvedPath));
    const int phiSamples = AssetDataBase::GetImportInt(meta, "kPhiSamples", 64);
    const int thetaSamples = AssetDataBase::GetImportInt(meta, "kThetaSamples", 32);
    const int irradianceFaceSize = AssetDataBase::GetImportInt(meta, "kIrradianceFaceSize", 32);
    const int prefilterFaceSize = AssetDataBase::GetImportInt(meta, "kPrefilterFaceSize", 0);
    const int minPrefilterFaceSize = AssetDataBase::GetImportInt(meta, "kMinPrefilterFaceSize", 512);
    const bool isHdr = AssetDataBase::IsFloatTextureExtension(resolvedPath);

    const auto cached = m_iblMaps.find(resolvedPath);
    if (cached != m_iblMaps.end())
    {
        const std::shared_ptr<IBLImage>& existing = cached->second.relativeAsset[0];
        const bool importMatches =
            existing && existing->m_srgb == useSrgb && existing->m_generateMips == useGenerateMips &&
            existing->m_irradiancePhiSamples == phiSamples && existing->m_irradianceThetaSamples == thetaSamples &&
            existing->m_irradianceFaceSize == irradianceFaceSize &&
            existing->m_prefilterFaceSize == prefilterFaceSize &&
            existing->m_minPrefilterFaceSize == minPrefilterFaceSize && existing->prefiltered && existing->irradiance;
        if (!forceReload && !cached->second.IsNeedReload() && importMatches)
        {
            ibl = existing;
            return true;
        }
        LogA(LogLevel::INFO,
             "Reloading IBL: {} (srgb={}, generateMips={}, phi={}, theta={}, irradianceFace={}, prefilterFace={}, "
             "minPrefilterFace={})",
             resolvedPath, useSrgb, useGenerateMips, phiSamples, thetaSamples, irradianceFaceSize, prefilterFaceSize,
             minPrefilterFaceSize);
        ibl = existing;
    }

    int width = 0, height = 0, nChannels = 0;
    std::unique_ptr<Texture2D> equirectTex = std::make_unique<Texture2D>();
    if (isHdr)
    {
        float* data = stbi_loadf(ioPath.c_str(), &width, &height, &nChannels, 0);
        if (!data)
        {
            LogA(LogLevel::ERROR, "Failed to load equirect HDR IBL: {}", resolvedPath);
            return false;
        }

        TextureDesc desc = TextureDesc::MakeExplicit(width, height, GL_RGB16F, GL_RGB, GL_FLOAT);
        desc.sampler.wrapS = GL_REPEAT;
        desc.sampler.wrapT = GL_CLAMP_TO_EDGE;
        desc.sampler.wrapR = GL_CLAMP_TO_EDGE;
        desc.sampler.minFilter = GL_LINEAR;
        desc.sampler.magFilter = GL_LINEAR;
        equirectTex->Create(desc, data);
        stbi_image_free(data);
    }
    else
    {
        unsigned char* data = stbi_load(ioPath.c_str(), &width, &height, &nChannels, 0);
        if (!data)
        {
            LogA(LogLevel::ERROR, "Failed to load equirect IBL: {}", resolvedPath);
            return false;
        }

        TextureDesc desc = TextureDesc::MakeAuto(width, height, nChannels, useSrgb);
        desc.sampler.wrapS = GL_REPEAT;
        desc.sampler.wrapT = GL_CLAMP_TO_EDGE;
        desc.sampler.wrapR = GL_CLAMP_TO_EDGE;
        desc.sampler.minFilter = GL_LINEAR;
        desc.sampler.magFilter = GL_LINEAR;
        equirectTex->Create(desc, data);
        stbi_image_free(data);
    }

    equirectTex->m_path = resolvedPath;
    equirectTex->m_name = Util::GetNameFromPath(resolvedPath);

    if (!ibl->prefiltered)
        ibl->prefiltered = std::make_shared<TextureCube>();
    if (!ibl->irradiance)
        ibl->irradiance = std::make_shared<TextureCube>();

    ibl->m_path = resolvedPath;
    ibl->m_name = Util::GetNameFromPath(resolvedPath);
    ibl->m_srgb = useSrgb;
    ibl->m_generateMips = useGenerateMips;
    ibl->m_irradiancePhiSamples = phiSamples;
    ibl->m_irradianceThetaSamples = thetaSamples;
    ibl->m_irradianceFaceSize = irradianceFaceSize;
    ibl->m_prefilterFaceSize = prefilterFaceSize;
    ibl->m_minPrefilterFaceSize = minPrefilterFaceSize;

    ibl->prefiltered->m_path = resolvedPath;
    ibl->prefiltered->m_name = ibl->m_name + "_prefiltered";
    ibl->prefiltered->m_srgb = useSrgb;
    ibl->prefiltered->m_generateMips = useGenerateMips;

    ibl->irradiance->m_path = resolvedPath;
    ibl->irradiance->m_name = ibl->m_name + "_irradiance";
    ibl->irradiance->m_srgb = false;
    ibl->irradiance->m_generateMips = false;

    EquirectToCubemapParams bakeParams;
    bakeParams.srgb = useSrgb;
    bakeParams.generateMips = useGenerateMips;
    bakeParams.faceSize = prefilterFaceSize;
    bakeParams.irradianceFaceSize = irradianceFaceSize;
    bakeParams.minPrefilterFaceSize = minPrefilterFaceSize;
    bakeParams.irradiancePhiSamples = phiSamples;
    bakeParams.irradianceThetaSamples = thetaSamples;

    if (!CubemapBaker::BakeIBLFromEquirect(*equirectTex, *ibl->prefiltered, *ibl->irradiance, bakeParams))
    {
        LogA(LogLevel::ERROR, "Failed to bake equirect IBL: {}", resolvedPath);
        return false;
    }

    const int64_t fileMtime = get_mtime(ioPath);
    if (cached == m_iblMaps.end())
    {
        FileState<IBLImage> file(resolvedPath);
        file.mTime = fileMtime;
        file.mMetaTime = get_mtime(resolvedPath + ".meta");
        file.AddRelativeAsset(ibl);
        m_iblMaps[resolvedPath] = file;
        LogA(LogLevel::INFO, "IBL loaded: '{}' prefiltered mip0 {}x{}, irradiance {}x{}", ibl->m_path,
             ibl->prefiltered->GetWidth(), ibl->prefiltered->GetHeight(), ibl->irradiance->GetWidth(),
             ibl->irradiance->GetHeight());
    }
    else
    {
        cached->second.UpdateModifyTime();
        LogA(LogLevel::INFO, "IBL reloaded: '{}' prefiltered mip0 {}x{}, irradiance {}x{}", ibl->m_path,
             ibl->prefiltered->GetWidth(), ibl->prefiltered->GetHeight(), ibl->irradiance->GetWidth(),
             ibl->irradiance->GetHeight());
    }
    return true;
}

bool LoaderManager::LoadShader(const std::string& path, std::shared_ptr<Shader>& shader, bool reload)
{
    const std::string resolvedPath = AssetDataBase::Get().ResolveAssetRef(path);
    AssetDataBase::Get().LoadOrCreateMeta(resolvedPath);

    const auto cached = m_shaderFiles.find(resolvedPath);
    if (cached != m_shaderFiles.end())
    {
        if (!reload && cached->second.IsNeedReload())
            reload = true;
        if (!reload)
        {
            shader = cached->second.relativeAsset[0];
            return true;
        }
        shader = cached->second.relativeAsset[0];
    }
    LogA(LogLevel::INFO, "{}{}", reload ? "Reloading shader: " : "Loading shader: ", resolvedPath);
    shader->m_name = Util::GetNameFromPath(resolvedPath);
    shader->m_path = resolvedPath;
    currentShader = shader;
    std::vector<PassCode> codes;
    std::vector<PassOption> options;
    std::unordered_map<std::string, MaterialProperty> propertiesMap;
    std::vector<std::string> propertyOrder;
    const std::string shaderFileContent = ReadAndPerprocessShaderFile(resolvedPath);
    const int shaderLevelQueue = ParseShaderLevelRenderQueue(shaderFileContent);
    if (codes.empty() && !LoadShaderCodeFromFile(resolvedPath, codes, options, propertiesMap, propertyOrder))
    {
        LogA(LogLevel::ERROR, "Shader '{}' has no Pass blocks", resolvedPath);
        return false;
    }
    const int explicitRenderQueue = ResolveEffectiveRenderQueue(shaderLevelQueue, options);
    shader->CompileShaderFromCode(codes, options, propertiesMap, propertyOrder, explicitRenderQueue);

    if (reload)
    {
        for (Material* mat : shader->m_rerelativeMaterials)
        {
            if (mat)
                mat->Update();
        }
    }
    if (cached != m_shaderFiles.end())
        cached->second.UpdateModifyTime();
    else if (auto it = m_shaderFiles.find(resolvedPath); it != m_shaderFiles.end())
        it->second.UpdateModifyTime();

    currentShader = nullptr;
    return true;
}

bool LoaderManager::LoadComputeShader(const std::string& path, std::shared_ptr<ComputeShader>& shader, bool reload)
{
    const std::string resolvedPath = AssetDataBase::Get().ResolveAssetRef(path);
    if (resolvedPath.empty())
    {
        LogA(LogLevel::ERROR, "LoadComputeShader: invalid asset ref '{}'", path);
        return false;
    }
    AssetDataBase::Get().LoadOrCreateMeta(resolvedPath);

    const auto cached = m_computeShaderFiles.find(resolvedPath);
    if (cached != m_computeShaderFiles.end())
    {
        if (!reload && cached->second.IsNeedReload())
            reload = true;
        if (!reload)
        {
            if (cached->second.relativeAsset.empty() || !cached->second.relativeAsset[0])
            {
                LogA(LogLevel::WARNING, "Compute shader cache entry has no asset: '{}'", resolvedPath);
            }
            else
            {
                shader = cached->second.relativeAsset[0];
                return true;
            }
        }
        if (!cached->second.relativeAsset.empty() && cached->second.relativeAsset[0])
            shader = cached->second.relativeAsset[0];
    }
    else if (!shader)
    {
        shader = std::make_shared<ComputeShader>();
    }

    LogA(LogLevel::INFO, "{}{}", reload ? "Reloading compute shader: " : "Loading compute shader: ", resolvedPath);
    shader->m_name = Util::GetNameFromPath(resolvedPath);
    shader->m_path = resolvedPath;

    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();
    const std::string sourcePath = AssetDataBase::NormalizeSourcePath(resolvedPath);
    const std::string ioPath = AssetPaths::Get().ToAbsolutePath(sourcePath);

    std::ifstream ifs(ioPath, std::ios::binary);
    if (!ifs)
    {
        LogA(LogLevel::ERROR, "Failed to open compute shader file: '{}'", resolvedPath);
        return false;
    }

    std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (code.size() >= 3 && static_cast<unsigned char>(code[0]) == 0xEF &&
        static_cast<unsigned char>(code[1]) == 0xBB && static_cast<unsigned char>(code[2]) == 0xBF)
    {
        code.erase(0, 3);
    }
    if (code.empty())
    {
        LogA(LogLevel::ERROR, "Compute shader file is empty: '{}'", resolvedPath);
        return false;
    }

    if (!shader->LoadFromCode(code.c_str()))
    {
        LogA(LogLevel::ERROR, "Failed to load compute shader: '{}'", resolvedPath);
        return false;
    }

    if (cached == m_computeShaderFiles.end())
    {
        FileState<ComputeShader> file(resolvedPath);
        file.AddRelativeAsset(shader);
        m_computeShaderFiles[resolvedPath] = std::move(file);
    }

    if (cached != m_computeShaderFiles.end())
        cached->second.UpdateModifyTime();
    else if (auto it = m_computeShaderFiles.find(resolvedPath); it != m_computeShaderFiles.end())
        it->second.UpdateModifyTime();

    return true;
}

bool LoaderManager::LoadShaderCodeFromFile(const std::string& path, std::vector<PassCode>& codes,
                                           std::vector<PassOption>& options,
                                           std::unordered_map<std::string, MaterialProperty>& propertiesMap,
                                           std::vector<std::string>& propertyOrder)
{
    std::string ShaderFileContent = ReadAndPerprocessShaderFile(path);
    std::string shaderCodeLower = ShaderFileContent;
    stringToLower(shaderCodeLower);

    int propertiesStartPoint = static_cast<int>(shaderCodeLower.find("properties"));

    if (propertiesStartPoint != std::string::npos)
    {
        std::string properties = getBlockContent(ShaderFileContent, propertiesStartPoint);
        std::istringstream iss(properties);
        std::string line;
        propertyOrder.clear();
        while (std::getline(iss, line))
        {
            MaterialProperty prop;
            if (!ParseMaterialPropertyLine(line, prop))
                continue;
            if (propertiesMap.find(prop.name) == propertiesMap.end())
                propertyOrder.push_back(prop.name);
            propertiesMap[prop.name] = std::move(prop);
        }
        if (!propertiesMap.empty())
            LogA(LogLevel::INFO, "Shader '{}' Properties: {} entries", path, propertiesMap.size());
    }

    int SubShaderStartPoint = static_cast<int>(shaderCodeLower.find("subshader"));
    if (SubShaderStartPoint == std::string::npos)
    {
        LogA(LogLevel::ERROR, "Shader '{}' has no SubShader block", path);
        return false;
    }

    std::string subShader = getBlockContent(ShaderFileContent, SubShaderStartPoint);
    int PassStartPoint = 0;
    int currentPassIndex = 0;

    while (true)
    {
        std::string passCode = subShader.substr(PassStartPoint);
        std::string passCodeLower = passCode;
        stringToLower(passCodeLower);
        subShader = passCode;
        PassStartPoint = FindPassBlockStart(passCodeLower);
        if (PassStartPoint == -1)
            break;

        std::string pass = getBlockContent(subShader, PassStartPoint);
        PassCode code;
        PassOption option;
        std::string realPassCode;
        // 获取通道代码，然后替换
        std::istringstream iss(pass);
        std::string line;
        bool realCodeFlag = false;
        bool isInStencilBlock = false;
        bool isInTagsBlock = false;
        while (std::getline(iss, line))
        {
            if (line.find("GLSLPROGRAM") != std::string::npos)
            {
                realCodeFlag = true;
                continue;
            }
            if (line.find("ENDGLSL") != std::string::npos)
            {
                realCodeFlag = false;
                continue;
            }
            if (realCodeFlag)
            {
                realPassCode = realPassCode + "\n" + line;
                continue;
            }

            std::string lineLower = line;
            stringToLower(lineLower);
            auto tokens = TokenizeLowerTrimmed(lineLower);
            if (tokens.empty())
            {
                if (iss.eof())
                    break;
                continue;
            }

            if (tokens[0] == "name")
            {
                int nameStart = static_cast<int>(lineLower.find("\"")) + 1;
                int nameEnd = static_cast<int>(lineLower.rfind("\""));
                code.passName = line.substr(nameStart, nameEnd - nameStart);
            }
            else if (tokens[0] == "zwrite")
            {
                const std::string state = (tokens.size() >= 2) ? tokens[1] : "on";
                if (state == "off" || state == "false" || state == "0")
                    option.ZWrite = false;
                else
                    option.ZWrite = true;
            }
            else if (tokens[0] == "ztest")
            {
                // 支持：
                // - ztest off
                // - ztest on
                // - ztest less/lequal/... （同时设置 enableDepthTest=true + 深度比较函数）
                const std::string state = (tokens.size() >= 2) ? tokens[1] : "on";
                if (state == "off")
                    option.enableDepthTest = false;
                else
                {
                    option.enableDepthTest = true;
                    if (state != "on")
                        option.zTest = DeterminFuncByString(state);
                }
            }
            else if (tokens[0] == "cwrite")
            {
                // 读取颜色通道写入选项

                GLboolean colorWriteState[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
                for (size_t i = 0; i < 4 && (i + 1) < tokens.size(); i++)
                {
                    const std::string& state = tokens[i + 1];
                    colorWriteState[i] = (state == "off" || state == "false" || state == "0") ? GL_FALSE : GL_TRUE;
                }
                std::memcpy(&option.writeColor, colorWriteState, sizeof(option.writeColor));
            }
            else if (tokens[0] == "cull")
            {
                const std::string state = (tokens.size() >= 2) ? tokens[1] : "off";
                if (state == "front")
                    option.cullMode = CullMode::FRONT;
                else if (state == "back")
                    option.cullMode = CullMode::BACK;
                else if (state == "off")
                    option.cullMode = CullMode::OFF;
            }
            else if (tokens[0] == "blendeq")
            {
                const std::string state = (tokens.size() >= 2) ? tokens[1] : "add";
                option.blendEq = DeterminBlendEquation(state);
            }
            else if (tokens[0] == "blend")
            {
                // 支持：
                // - blend off
                // - blend <src> <dst>
                if (tokens.size() >= 2 && tokens[1] == "off")
                {
                    option.enableBlend = false;
                }
                else if (tokens.size() >= 3)
                {
                    option.enableBlend = true;
                    option.sFactor = DeterminBlendFunc(tokens[1]);
                    option.dFactor = DeterminBlendFunc(tokens[2]);
                }
                else
                {
                    // 容错：只写 blend 则默认开启但保持默认因子
                    option.enableBlend = true;
                }
            }
            else if (tokens[0] == "queue")
            {
                if (tokens.size() >= 2)
                {
                    int queueValue = RenderQueue::Geometry;
                    if (RenderQueue::TryParseToken(tokens[1], queueValue))
                        option.renderQueue = queueValue;
                    else
                        LogA(LogLevel::WARNING, "Shader '{}' pass: invalid queue '{}'", path, tokens[1]);
                }
            }
            else if (tokens[0] == "stencil")
            {
                // 支持：
                // - stencil off
                // - Stencil { ... }
                if (tokens.size() >= 2 && tokens[1] == "off")
                {
                    option.enableStencilTest = false;
                    isInStencilBlock = false;
                }
                else
                {
                    option.enableStencilTest = true;
                    isInStencilBlock = true;
                }
                continue;
            }
            else if (tokens[0] == "tags")
            {
                isInTagsBlock = true;
                continue;
            }
            else if (tokens[0] == "drawtime")
            {
                if (tokens.size() >= 2)
                {
                    try
                    {
                        option.DrawTimes = std::stoi(tokens[1]);
                    }
                    catch (const std::invalid_argument& e)
                    {
                        LogA(LogLevel::WARNING, "Failed to convert string:{} to int", tokens[1]);
                    }
                }
            }

            if (isInStencilBlock)
            {
                if (tokens[0] == "{")
                {
                    continue;
                }
                else if (tokens[0] == "andmask")
                {
                    if (tokens.size() >= 2)
                        option.AndMask = std::stoi(tokens[1], nullptr, 0);
                    continue;
                }
                else if (tokens[0] == "bitmask")
                {
                    if (tokens.size() >= 2)
                        option.BitMask = std::stoi(tokens[1], nullptr, 0);
                    continue;
                }
                else if (tokens[0] == "func")
                {
                    if (tokens.size() >= 2)
                        option.StencilFunc = DeterminFuncByString(tokens[1]);
                    continue;
                }
                else if (tokens[0] == "ref")
                {
                    if (tokens.size() >= 2)
                        option.Stencilref = std::stoi(tokens[1], nullptr, 0);
                    continue;
                }
                else if (tokens[0] == "fail")
                {
                    if (tokens.size() >= 2)
                        option.stencilFail = DeterminOpByString(tokens[1]);
                    continue;
                }
                else if (tokens[0] == "dpfail")
                {
                    if (tokens.size() >= 2)
                        option.stencilDpFail = DeterminOpByString(tokens[1]);
                }
                else if (tokens[0] == "dppass")
                {
                    if (tokens.size() >= 2)
                        option.stencilDpPass = DeterminOpByString(tokens[1]);
                }
                else if (tokens[0] == "}")
                {
                    isInStencilBlock = false;
                }
            }

            if (isInTagsBlock)
            {
                if (tokens[0] == "{")
                    continue;
                if (tokens[0] == "}")
                {
                    isInTagsBlock = false;
                    continue;
                }
                if (tokens.size() >= 2)
                {
                    const std::string key = CanonicalizeShaderTagKey(tokens[0]);
                    const std::string value = ExtractTagValueFromLine(line);
                    if (!key.empty() && !value.empty())
                        option.tags[key] = value;
                }
                continue;
            }

            if (iss.eof())
                break;
        }
        const Config& config = Config::Get();
        std::string shaderHeader = config.GetGlslVersionString() + "\n";
        shaderHeader += config.GetShaderVersionDefines();
        shaderHeader += "#define MAXLOCALLIGHT " + std::to_string(Config::MaxLocalLight) + "\n";
        shaderHeader += "#define MAXLOCALLIGHTVERTEX " + std::to_string(Config::MaxLocalLight * 18) + "\n";
        code.VertexShader = shaderHeader + "#define VERTEX\n" + realPassCode;
        code.FragmentShader = shaderHeader + "#define FRAGMENT\n" + realPassCode;
        if (passCode.find("#ifdef GEOMETRY") != static_cast<size_t>(-1))
        {
            code.GeometryShader = shaderHeader + "#define GEOMETRY\n" + realPassCode;
        }
        codes.push_back(code);
        options.push_back(option);
        PassStartPoint += static_cast<int>(pass.size() + 4);
        currentPassIndex += 1;
    }
    (void)currentPassIndex;

    if (!propertiesMap.empty())
        ApplyGlslUniformDefaultsFromPasses(codes, propertiesMap);

    return true;
}

namespace
{
std::string ResolveModelRelativeTexturePath(const std::string& modelPath, const std::string& texturePath)
{
    if (texturePath.empty() || texturePath[0] == '*')
        return {};

    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();
    AssetPaths& paths = AssetPaths::Get();

    namespace fs = std::filesystem;
    const std::string resolvedModel = paths.ResolveToSourcePath(modelPath);
    const fs::path modelDir = fs::path(paths.ToAbsolutePath(resolvedModel)).parent_path();
    const fs::path raw(texturePath);

    std::error_code ec;
    const auto tryCandidate = [&](const fs::path& candidate) -> std::string
    {
        if (!fs::is_regular_file(candidate, ec))
            return {};
        std::error_code relEc;
        const std::string relative = paths.ToRelativeSourcePath(fs::absolute(candidate, ec));
        if (!relative.empty() && relative.find(':') == std::string::npos)
            return AssetDataBase::NormalizeSourcePath(relative);
        return AssetDataBase::NormalizeSourcePath(fs::absolute(candidate, ec).generic_string());
    };

    if (std::string resolved = tryCandidate(modelDir / raw); !resolved.empty())
        return resolved;
    if (std::string resolved = tryCandidate(modelDir / raw.filename()); !resolved.empty())
        return resolved;

    const std::string fromAssetRef = AssetDataBase::NormalizeSourcePath(paths.ResolveToSourcePath(texturePath));
    if (!fromAssetRef.empty() && fs::is_regular_file(paths.ToAbsolutePath(fromAssetRef), ec))
        return fromAssetRef;

    if (raw.filename() == raw)
    {
        const std::string fromTextures = AssetDataBase::NormalizeSourcePath(
            paths.ResolveToSourcePath(Util::ResolveShaderDefaultTexturePath(texturePath)));
        if (!fromTextures.empty() && fs::is_regular_file(paths.ToAbsolutePath(fromTextures), ec))
            return fromTextures;
    }

    return {};
}

void ExtractSourceMaterials(const aiScene* scene, const std::string& modelPath, Model& model)
{
    model.m_sourceMaterials.clear();
    model.m_sourceMaterials.resize(scene->mNumMaterials);

    for (unsigned i = 0; i < scene->mNumMaterials; ++i)
    {
        aiMaterial* material = scene->mMaterials[i];
        SourceMaterialInfo& info = model.m_sourceMaterials[i];

        aiString name;
        material->Get(AI_MATKEY_NAME, name);
        info.name = name.C_Str();
        if (info.name.empty())
            info.name = "Material" + std::to_string(i);

        aiString texPath;
        if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) != AI_SUCCESS)
            material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
        if (texPath.length > 0)
            info.diffusePath = ResolveModelRelativeTexturePath(modelPath, texPath.C_Str());
    }
}
} // namespace

bool LoaderManager::LoadModel(const std::string& path, std::shared_ptr<Model>& model)
{
    const std::string resolvedPath = AssetDataBase::Get().ResolveAssetRef(path);
    AssetDataBase::Get().LoadOrCreateMeta(resolvedPath);

    const auto cached = m_models.find(resolvedPath);
    if (cached != m_models.end())
    {
        if (!cached->second.IsNeedReload())
        {
            model = cached->second.relativeAsset[0];
            return true;
        }
        LogA(LogLevel::INFO, "Reloading model: {}", resolvedPath);
        model = cached->second.relativeAsset[0];
        model->m_meshSections.clear();
        model->m_materials.clear();
        model->m_sourceMaterials.clear();
        model->m_bounds = Bounds();
    }

    currentModel = model;
    currentModel->m_name = Util::GetNameFromPath(resolvedPath);
    currentModel->m_path = resolvedPath;

    const std::string ioPath = AssetPaths::Get().ToAbsolutePath(resolvedPath);
    Assimp::Importer importer;
    // 如果没有顶点的法线模型处理中，我们会为模型生成法线和UV坐标
    const aiScene* scene =
        importer.ReadFile(ioPath, aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_CalcTangentSpace);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        LogA(LogLevel::ERROR, "Assimp failed '{}': {}", resolvedPath, importer.GetErrorString());
        currentModel = nullptr;
        return false;
    }
    ExtractSourceMaterials(scene, resolvedPath, *currentModel);
    ProcessNode(scene->mRootNode, scene);
    if (cached == m_models.end())
    {
        LogA(LogLevel::INFO, "Model '{}' loaded ({} mesh sections)", model->m_name, model->m_meshSections.size());
        FileState<Model> file(resolvedPath);
        file.AddRelativeAsset(currentModel);
        m_models[resolvedPath] = file;
    }
    else
    {
        cached->second.UpdateModifyTime();
        LogA(LogLevel::INFO, "Model '{}' reloaded ({} mesh sections)", model->m_name, model->m_meshSections.size());
    }

    currentModel = nullptr;
    return true;
}

void LoaderManager::ProcessNode(aiNode* node, const aiScene* scene)
{
    // 处理所有的网格
    for (unsigned int i = 0; i < node->mNumMeshes; i++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        ProcessMesh(mesh, scene);
    }
    // 递归处理节点
    for (unsigned int i = 0; i < node->mNumChildren; i++)
        ProcessNode(node->mChildren[i], scene);
}

void LoaderManager::ProcessMesh(aiMesh* mesh, const aiScene* /*scene*/)
{
    std::shared_ptr<Mesh> out_mesh = std::make_unique<Mesh>();
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    glm::vec3 minPoint = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 maxPoint = glm::vec3(std::numeric_limits<float>::min());
    // 处理顶点信息
    for (unsigned int i = 0; i < mesh->mNumVertices; i++)
    {
        Vertex vertex;

        // 位置
        glm::vec3 vector;
        vector.x = mesh->mVertices[i].x;
        vector.y = mesh->mVertices[i].y;
        vector.z = mesh->mVertices[i].z;
        vertex.Position = vector;

        // 存储最大点和最小点
        minPoint = glm::min(minPoint, vector);
        maxPoint = glm::max(maxPoint, vector);

        // 法线
        vector.x = mesh->mNormals[i].x;
        vector.y = mesh->mNormals[i].y;
        vector.z = mesh->mNormals[i].z;
        vertex.Normal = vector;

        // 切线 + bitangent 符号（tangent.w，用于镜像 UV 岛的正确 TBN）
        if (mesh->mTangents)
        {
            const glm::vec3 tangent(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
            float handedness = 1.0f;
            if (mesh->mBitangents)
            {
                const glm::vec3 bitangent(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
                handedness = (glm::dot(glm::cross(vertex.Normal, tangent), bitangent) < 0.0f) ? -1.0f : 1.0f;
            }
            vertex.Tangent = glm::vec4(tangent, handedness);
        }
        else
        {
            vertex.Tangent = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        }

        // 顶点颜色
        if (mesh->mColors[0])
        {
            vector.x = mesh->mColors[0][i].r;
            vector.y = mesh->mColors[0][i].g;
            vector.z = mesh->mColors[0][i].b;
            vertex.Color = vector;
        }
        else
        {
            vertex.Color = glm::vec3(0, 0, 0);
        }

        // 纹理坐标
        if (mesh->mTextureCoords[0])
        {
            glm::vec2 vec;
            vec.x = mesh->mTextureCoords[0][i].x;
            vec.y = mesh->mTextureCoords[0][i].y;
            vertex.Texcoord0 = vec;
        }
        else
        {
            vertex.Texcoord0 = glm::vec2(0.0f, 0.0f);
        }

        if (mesh->mTextureCoords[1])
        {
            glm::vec2 vec;
            vec.x = mesh->mTextureCoords[1][i].x;
            vec.y = mesh->mTextureCoords[1][i].y;
            vertex.Texcoord1 = vec;
        }
        else
        {
            vertex.Texcoord1 = glm::vec2(0.0f, 0.0f);
        }

        vertices.push_back(vertex);
    }

    // 处理索引信息
    for (unsigned int i = 0; i < mesh->mNumFaces; i++)
    {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
        {
            indices.push_back(face.mIndices[j]);
        }
    }
    Bounds bounds(maxPoint, minPoint);
    out_mesh->Create(vertices, indices, bounds);
    out_mesh->m_name = mesh->mName.C_Str();

    int materialIndex = mesh->mMaterialIndex;
    if (materialIndex < 0 || static_cast<unsigned>(materialIndex) >= currentModel->m_sourceMaterials.size())
        materialIndex = 0;

    currentModel->AddSection(out_mesh, materialIndex);
}

namespace
{
bool ShaderIncludeFileExists(const std::string& sourcePath)
{
    if (sourcePath.empty())
        return false;
    std::error_code ec;
    return std::filesystem::exists(AssetPaths::Get().ToAbsolutePath(sourcePath), ec);
}

std::string JoinSourcePath(const std::string& base, const std::string& relative)
{
    if (base.empty())
        return AssetDataBase::NormalizeSourcePath(relative);
    if (relative.empty())
        return AssetDataBase::NormalizeSourcePath(base);
    return AssetDataBase::NormalizeSourcePath(base + "/" + relative);
}

std::string TrimIncludeToken(std::string token)
{
    auto isSpace = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!token.empty() && isSpace(static_cast<unsigned char>(token.front())))
        token.erase(token.begin());
    while (!token.empty() && isSpace(static_cast<unsigned char>(token.back())))
        token.pop_back();
    return token;
}

std::string ExtractIncludeTokenFromLine(const std::string& line)
{
    if (line.find("#include") == std::string::npos)
        return {};

    const size_t pathStart = line.find_first_of("\"<", line.find("#include"));
    if (pathStart == std::string::npos)
        return {};

    const char open = line[pathStart];
    const char close = (open == '"') ? '"' : '>';
    const size_t pathEnd = line.find(close, pathStart + 1);
    if (pathEnd == std::string::npos || pathEnd <= pathStart + 1)
        return {};

    return TrimIncludeToken(line.substr(pathStart + 1, pathEnd - pathStart - 1));
}

std::string StripEngineIncludePrefix(std::string token)
{
    static constexpr const char* kPrefixes[] = {"/include/", "include/"};
    for (const char* prefix : kPrefixes)
    {
        const size_t len = std::char_traits<char>::length(prefix);
        if (token.size() >= len && token.compare(0, len, prefix) == 0)
            return token.substr(len);
    }
    return token;
}

std::string ResolveShaderIncludePath(const std::string& includingSourcePath, const std::string& rawIncludeToken)
{
    const std::string includeToken = TrimIncludeToken(rawIncludeToken);
    if (includeToken.empty())
        return {};

    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();

    AssetPaths& paths = AssetPaths::Get();
    const std::string engineIncludeRoot = paths.GetEngineShaderIncludeRoot();

    const auto tryPath = [](const std::string& candidate) -> std::string
    {
        const std::string normalized = AssetDataBase::NormalizeSourcePath(candidate);
        return ShaderIncludeFileExists(normalized) ? normalized : std::string{};
    };

    const auto tryEngineInclude = [&](const std::string& subPath) -> std::string
    {
        if (subPath.empty())
            return {};
        return tryPath(JoinSourcePath(engineIncludeRoot, subPath));
    };

    const std::string engineRelative = StripEngineIncludePrefix(includeToken);

    // 项目 shader 只能引用引擎头：统一在 engineIncludeRoot 下解析
    if (std::string resolved = tryEngineInclude(engineRelative); !resolved.empty())
        return resolved;

    const size_t slash = engineRelative.find_last_of('/');
    if (slash != std::string::npos && slash + 1 < engineRelative.size())
    {
        if (std::string resolved = tryEngineInclude(engineRelative.substr(slash + 1)); !resolved.empty())
            return resolved;
    }

    // 引擎 include 目录内部的相对引用（如 Core.glsl → GLInput.glsl）
    if (!includingSourcePath.empty())
    {
        const size_t folderSlash = includingSourcePath.find_last_of('/');
        const std::string rootFolder =
            folderSlash != std::string::npos ? includingSourcePath.substr(0, folderSlash) : std::string{};
        const std::string engineRootPrefix = engineIncludeRoot + "/";
        if (!rootFolder.empty() && (rootFolder == engineIncludeRoot || rootFolder.rfind(engineRootPrefix, 0) == 0))
        {
            if (std::string resolved = tryPath(JoinSourcePath(rootFolder, includeToken)); !resolved.empty())
                return resolved;
        }
    }

    LogA(LogLevel::ERROR,
         "Shader include not found: '{}' (included from '{}', engineIncludeRoot='{}', tried='{}'); only engine "
         "include headers are supported",
         includeToken, includingSourcePath, engineIncludeRoot, JoinSourcePath(engineIncludeRoot, engineRelative));
    return {};
}

std::unordered_set<std::string>& ActiveShaderIncludeStack()
{
    thread_local std::unordered_set<std::string> stack;
    return stack;
}
} // namespace

std::string LoaderManager::ReadAndPerprocessShaderFile(const std::string& path)
{
    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();

    const std::string sourcePath = AssetDataBase::NormalizeSourcePath(path);
    const std::string ioPath = AssetPaths::Get().ToAbsolutePath(sourcePath);

    std::unordered_set<std::string>& includeStack = ActiveShaderIncludeStack();
    if (!includeStack.insert(sourcePath).second)
    {
        LogA(LogLevel::WARNING, "Circular shader include skipped: '{}' (included from chain)", sourcePath);
        return {};
    }
    struct IncludeStackGuard
    {
        std::unordered_set<std::string>& stack;
        std::string key;
        ~IncludeStackGuard()
        {
            stack.erase(key);
        }
    } includeGuard{includeStack, sourcePath};

    if (m_shaderFiles.find(sourcePath) == m_shaderFiles.end())
    {
        m_shaderFiles[sourcePath] = FileState<Shader>(sourcePath);
    }
    m_shaderFiles[sourcePath].AddRelativeAsset(currentShader);

    std::ifstream shaderFile;
    std::string shaderCode;
    shaderFile.exceptions(std::ifstream::badbit);
    try
    {
        shaderFile.open(ioPath);
        if (!shaderFile.is_open())
        {
            LogA(LogLevel::ERROR, "Cannot open shader file: {} (absolute: {})", sourcePath, ioPath);
            return shaderCode;
        }
        std::string line;
        bool firstLine = true;
        while (std::getline(shaderFile, line))
        {
            if (firstLine && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF)
            {
                line.erase(0, 3);
            }
            firstLine = false;

            const std::string includeToken = ExtractIncludeTokenFromLine(line);
            if (!includeToken.empty())
            {
                const std::string includePath = ResolveShaderIncludePath(sourcePath, includeToken);
                if (!includePath.empty())
                    shaderCode = shaderCode + "\n" + ReadAndPerprocessShaderFile(includePath);
                else
                    LogA(LogLevel::ERROR, "Failed to resolve #include \"{}\" in '{}'", includeToken, sourcePath);
            }
            else if (line.find("#include") != std::string::npos)
            {
                LogA(LogLevel::WARNING, "Malformed #include line in '{}': {}", sourcePath, line);
            }
            else
            {
                shaderCode = shaderCode + "\n" + line;
            }
            if (shaderFile.eof())
                break;
        }
    }
    catch (const std::ifstream::failure& e)
    {
        LogA(LogLevel::ERROR, "ERROR::SHADER::FILE_NOT_SUCCESSFUL::READ::\n{}", e.what());
    }
    return shaderCode;
}

namespace
{
std::string NormalizeMaterialTypeToken(std::string type)
{
    stringToLower(type);
    if (type == "sampler2d" || type == "texture2d")
        return "texture2d";
    if (type == "samplercube" || type == "texturecube")
        return "texturecube";
    return type;
}

bool LooksLikeTexturePath(const std::string& path)
{
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= path.size())
        return path.find('/') != std::string::npos || path.find('\\') != std::string::npos;
    std::string ext = path.substr(dot);
    stringToLower(ext);
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr";
}

bool ParseMaterialQueueField(const nlohmann::json& j, MaterialFileDesc& outDesc, const std::string& path)
{
    if (j.is_number_integer())
    {
        outDesc.queueOverride = j.get<int>();
        outDesc.hasQueueOverride = true;
        return true;
    }
    if (j.is_string())
    {
        const std::string token = j.get<std::string>();
        if (RenderQueue::TryParseToken(token, outDesc.queueOverride))
        {
            outDesc.hasQueueOverride = true;
            return true;
        }
        LogA(LogLevel::WARNING, "Material '{}': invalid queue '{}'", path, token);
        return false;
    }
    LogA(LogLevel::WARNING, "Material '{}': queue must be int or string", path);
    return false;
}

bool ParseMaterialPropertyValue(const std::string& propName, const std::string& typeToken, const nlohmann::json& j,
                                MaterialProperty& outProp)
{
    const std::string type = NormalizeMaterialTypeToken(typeToken);
    outProp.name = propName;
    outProp.hasExplicitDefault = true;

    if (type == "float")
    {
        outProp.type = MaterialPropertyType::Float;
        outProp.value = j.at("value").get<float>();
        return true;
    }
    if (type == "int")
    {
        outProp.type = MaterialPropertyType::Int;
        outProp.value = j.at("value").get<int>();
        return true;
    }
    if (type == "bool")
    {
        outProp.type = MaterialPropertyType::Bool;
        outProp.value = j.at("value").get<bool>();
        return true;
    }
    if (type == "vec2")
    {
        outProp.type = MaterialPropertyType::Vec2;
        const auto& arr = j.at("value");
        outProp.value = glm::vec2(arr.at(0).get<float>(), arr.at(1).get<float>());
        return true;
    }
    if (type == "vec3")
    {
        outProp.type = MaterialPropertyType::Vec3;
        const auto& arr = j.at("value");
        outProp.value = glm::vec3(arr.at(0).get<float>(), arr.at(1).get<float>(), arr.at(2).get<float>());
        return true;
    }
    if (type == "vec4")
    {
        outProp.type = MaterialPropertyType::Vec4;
        const auto& arr = j.at("value");
        outProp.value =
            glm::vec4(arr.at(0).get<float>(), arr.at(1).get<float>(), arr.at(2).get<float>(), arr.at(3).get<float>());
        return true;
    }
    if (type == "mat3")
    {
        outProp.type = MaterialPropertyType::Mat3;
        const auto& arr = j.at("value");
        if (!arr.is_array() || arr.size() != 9)
            return false;
        glm::mat3 m(1.0f);
        for (int i = 0; i < 9; ++i)
            m[i / 3][i % 3] = arr.at(i).get<float>();
        outProp.value = m;
        return true;
    }
    if (type == "mat4")
    {
        outProp.type = MaterialPropertyType::Mat4;
        const auto& arr = j.at("value");
        if (!arr.is_array() || arr.size() != 16)
            return false;
        glm::mat4 m(1.0f);
        for (int i = 0; i < 16; ++i)
            m[i / 4][i % 4] = arr.at(i).get<float>();
        outProp.value = m;
        return true;
    }
    if (type == "texture2d")
    {
        outProp.type = MaterialPropertyType::Texture2D;
        outProp.value = std::shared_ptr<Texture>{};
        const std::string path =
            j.contains("path") ? j.at("path").get<std::string>() : j.at("value").get<std::string>();
        outProp.defaultTexturePath = Util::ResolveShaderDefaultTexturePath(path);
        if (j.contains("srgb"))
            outProp.textureSrgb = j.at("srgb").get<bool>();
        else
            outProp.textureSrgb = Util::TextureLoadUsesSrgb(outProp.defaultTexturePath, propName);
        return true;
    }
    if (type == "texturecube")
    {
        outProp.type = MaterialPropertyType::TextureCube;
        outProp.value = std::shared_ptr<Texture>{};
        const std::string path =
            j.contains("path") ? j.at("path").get<std::string>() : j.at("value").get<std::string>();
        outProp.defaultTexturePath = Util::ResolveShaderDefaultTexturePath(path);
        if (j.contains("srgb"))
            outProp.textureSrgb = j.at("srgb").get<bool>();
        else
            outProp.textureSrgb = true;
        return true;
    }
    return false;
}

bool ParseMaterialPropertyShorthand(const std::string& propName, const nlohmann::json& j, MaterialProperty& outProp)
{
    outProp.name = propName;
    outProp.hasExplicitDefault = true;

    if (j.is_boolean())
    {
        outProp.type = MaterialPropertyType::Bool;
        outProp.value = j.get<bool>();
        return true;
    }
    if (j.is_number_integer())
    {
        outProp.type = MaterialPropertyType::Int;
        outProp.value = j.get<int>();
        return true;
    }
    if (j.is_number_float())
    {
        outProp.type = MaterialPropertyType::Float;
        outProp.value = j.get<float>();
        return true;
    }
    if (j.is_string())
    {
        const std::string path = Util::ResolveShaderDefaultTexturePath(j.get<std::string>());
        if (!LooksLikeTexturePath(path))
            return false;
        outProp.type = MaterialPropertyType::Texture2D;
        outProp.value = std::shared_ptr<Texture>{};
        outProp.defaultTexturePath = path;
        outProp.textureSrgb = Util::TextureLoadUsesSrgb(path, propName);
        return true;
    }
    if (j.is_array())
    {
        const size_t n = j.size();
        if (n == 2)
        {
            outProp.type = MaterialPropertyType::Vec2;
            outProp.value = glm::vec2(j.at(0).get<float>(), j.at(1).get<float>());
            return true;
        }
        if (n == 3)
        {
            outProp.type = MaterialPropertyType::Vec3;
            outProp.value = glm::vec3(j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>());
            return true;
        }
        if (n == 4)
        {
            outProp.type = MaterialPropertyType::Vec4;
            outProp.value =
                glm::vec4(j.at(0).get<float>(), j.at(1).get<float>(), j.at(2).get<float>(), j.at(3).get<float>());
            return true;
        }
        if (n == 9)
        {
            outProp.type = MaterialPropertyType::Mat3;
            glm::mat3 m(1.0f);
            for (int i = 0; i < 9; ++i)
                m[i / 3][i % 3] = j.at(i).get<float>();
            outProp.value = m;
            return true;
        }
        if (n == 16)
        {
            outProp.type = MaterialPropertyType::Mat4;
            glm::mat4 m(1.0f);
            for (int i = 0; i < 16; ++i)
                m[i / 4][i % 4] = j.at(i).get<float>();
            outProp.value = m;
            return true;
        }
    }
    return false;
}

bool ParseMaterialPropertiesObject(const nlohmann::json& props, MaterialFileDesc& outDesc, const std::string& path)
{
    if (!props.is_object())
    {
        LogA(LogLevel::ERROR, "Material '{}': properties must be a JSON object", path);
        return false;
    }

    for (const auto& [propName, propJson] : props.items())
    {
        MaterialProperty prop;
        bool ok = false;
        if (propJson.is_object() && propJson.contains("type"))
            ok = ParseMaterialPropertyValue(propName, propJson.at("type").get<std::string>(), propJson, prop);
        else
            ok = ParseMaterialPropertyShorthand(propName, propJson, prop);

        if (!ok)
        {
            LogA(LogLevel::WARNING, "Material '{}': skip invalid property '{}'", path, propName);
            continue;
        }
        prop.name = propName;
        outDesc.properties[propName] = std::move(prop);
    }
    return true;
}
} // namespace

bool LoaderManager::ParseMaterialFile(const std::string& path, MaterialFileDesc& outDesc)
{
    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();
    const std::string ioPath = AssetPaths::Get().ToAbsolutePath(path);
    std::ifstream file(ioPath);
    if (!file.is_open())
    {
        LogA(LogLevel::ERROR, "Cannot open material file: {}", path);
        return false;
    }

    nlohmann::json j;
    try
    {
        file >> j;
    }
    catch (const nlohmann::json::exception& e)
    {
        LogA(LogLevel::ERROR, "Material file '{}' JSON parse error: {}", path, e.what());
        return false;
    }

    if (!j.is_object())
    {
        LogA(LogLevel::ERROR, "Material file '{}' root must be a JSON object", path);
        return false;
    }

    outDesc = MaterialFileDesc{};

    if (j.contains("name"))
        outDesc.materialName = j.at("name").get<std::string>();
    else if (j.contains("materialName"))
        outDesc.materialName = j.at("materialName").get<std::string>();

    if (j.contains("shader"))
        outDesc.shaderPath = j.at("shader").get<std::string>();
    else if (j.contains("shaderPath"))
        outDesc.shaderPath = j.at("shaderPath").get<std::string>();

    if (j.contains("showInUi"))
        outDesc.showInUi = j.at("showInUi").get<bool>();

    if (j.contains("queue"))
        ParseMaterialQueueField(j.at("queue"), outDesc, path);
    else if (j.contains("renderQueue"))
        ParseMaterialQueueField(j.at("renderQueue"), outDesc, path);

    if (j.contains("properties"))
    {
        if (!ParseMaterialPropertiesObject(j.at("properties"), outDesc, path))
            return false;
    }

    if (outDesc.shaderPath.empty())
    {
        LogA(LogLevel::ERROR, "Material file '{}' missing shader path", path);
        return false;
    }
    if (outDesc.materialName.empty())
        outDesc.materialName = Util::GetNameFromPath(path);

    LogA(LogLevel::INFO, "Material file parsed: '{}' shader='{}' queue={} props={}", outDesc.materialName,
         outDesc.shaderPath, outDesc.hasQueueOverride ? std::to_string(outDesc.queueOverride) : std::string("inherit"),
         outDesc.properties.size());
    return true;
}

namespace
{
std::string GetMaterialPropertyTexturePath(const MaterialProperty& prop)
{
    if (std::holds_alternative<std::shared_ptr<Texture>>(prop.value))
    {
        const std::shared_ptr<Texture>& tex = std::get<std::shared_ptr<Texture>>(prop.value);
        if (tex && tex->GetId() && !tex->m_path.empty())
            return AssetDataBase::NormalizeSourcePath(tex->m_path);
    }
    return prop.defaultTexturePath;
}

nlohmann::json MaterialPropertyToJson(const MaterialProperty& prop)
{
    switch (prop.type)
    {
    case MaterialPropertyType::Float:
        return std::get<float>(prop.value);
    case MaterialPropertyType::Int:
        return std::get<int>(prop.value);
    case MaterialPropertyType::Bool:
        return std::get<bool>(prop.value);
    case MaterialPropertyType::Vec2:
    {
        const glm::vec2 v = std::get<glm::vec2>(prop.value);
        return nlohmann::json::array({v.x, v.y});
    }
    case MaterialPropertyType::Vec3:
    {
        const glm::vec3 v = std::get<glm::vec3>(prop.value);
        return nlohmann::json::array({v.x, v.y, v.z});
    }
    case MaterialPropertyType::Vec4:
    {
        const glm::vec4 v = std::get<glm::vec4>(prop.value);
        return nlohmann::json::array({v.x, v.y, v.z, v.w});
    }
    case MaterialPropertyType::Mat3:
    {
        const glm::mat3 m = std::get<glm::mat3>(prop.value);
        nlohmann::json arr = nlohmann::json::array();
        for (int i = 0; i < 9; ++i)
            arr.push_back(m[i / 3][i % 3]);
        return arr;
    }
    case MaterialPropertyType::Mat4:
    {
        const glm::mat4 m = std::get<glm::mat4>(prop.value);
        nlohmann::json arr = nlohmann::json::array();
        for (int i = 0; i < 16; ++i)
            arr.push_back(m[i / 4][i % 4]);
        return arr;
    }
    case MaterialPropertyType::Texture2D:
    {
        nlohmann::json j;
        j["type"] = "texture2d";
        j["path"] = GetMaterialPropertyTexturePath(prop);
        j["srgb"] = prop.textureSrgb;
        return j;
    }
    case MaterialPropertyType::TextureCube:
    {
        nlohmann::json j;
        j["type"] = "texturecube";
        j["path"] = GetMaterialPropertyTexturePath(prop);
        j["srgb"] = prop.textureSrgb;
        return j;
    }
    default:
        return nlohmann::json::object();
    }
}
} // namespace

bool LoaderManager::BuildMaterialFileDescFromMaterial(const Material& mat, MaterialFileDesc& outDesc)
{
    const std::shared_ptr<Shader> shader = mat.GetShader();
    if (!shader || shader->m_path.empty())
        return false;

    outDesc = MaterialFileDesc{};
    outDesc.materialName = mat.m_name;
    outDesc.shaderPath = AssetDataBase::NormalizeSourcePath(shader->m_path);
    outDesc.showInUi = mat.m_showInUi;
    outDesc.hasQueueOverride = mat.HasRenderQueueOverride();
    if (outDesc.hasQueueOverride)
        outDesc.queueOverride = mat.GetRenderQueue();

    outDesc.properties.clear();
    const auto& properties = mat.GetProperties();
    for (const std::string& propName : mat.GetPropertyOrder())
    {
        const auto it = properties.find(propName);
        if (it == properties.end())
            continue;

        MaterialProperty saved = it->second;
        saved.name = propName;
        saved.hasExplicitDefault = true;
        if (saved.type == MaterialPropertyType::Texture2D || saved.type == MaterialPropertyType::TextureCube)
            saved.defaultTexturePath = GetMaterialPropertyTexturePath(saved);
        outDesc.properties[propName] = std::move(saved);
    }
    return true;
}

bool LoaderManager::WriteMaterialFile(const std::string& path, const MaterialFileDesc& desc)
{
    if (desc.shaderPath.empty())
    {
        LogA(LogLevel::ERROR, "WriteMaterialFile: empty shader path");
        return false;
    }

    nlohmann::json j;
    j["name"] = desc.materialName.empty() ? Util::GetNameFromPath(path) : desc.materialName;
    j["shader"] = desc.shaderPath;
    j["showInUi"] = desc.showInUi;
    if (desc.hasQueueOverride)
        j["renderQueue"] = desc.queueOverride;

    nlohmann::json props = nlohmann::json::object();
    for (const auto& [propName, prop] : desc.properties)
        props[propName] = MaterialPropertyToJson(prop);
    j["properties"] = std::move(props);

    std::ofstream file(AssetPaths::Get().ToAbsolutePath(path));
    if (!file.is_open())
    {
        LogA(LogLevel::ERROR, "WriteMaterialFile: cannot open '{}' for writing", path);
        return false;
    }

    try
    {
        file << j.dump(4);
    }
    catch (const nlohmann::json::exception& e)
    {
        LogA(LogLevel::ERROR, "WriteMaterialFile: JSON serialize failed for '{}': {}", path, e.what());
        return false;
    }

    LogA(LogLevel::INFO, "Material file written: '{}'", path);
    return true;
}

#undef MODULE
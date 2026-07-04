#include "util.h"

#include "../Asset/AssetPaths.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace
{
std::string DefaultTextureBase()
{
    if (AssetPaths::IsInitialized())
        return AssetPaths::Get().GetEngineContentRoot() + "/textures/";
    return "Content/Engine/textures/";
}

static std::string TrimTextureToken(std::string s)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
    if (s.size() >= 2)
    {
        const char q = s.front();
        if ((q == '"' || q == '\'') && s.back() == q)
            return s.substr(1, s.size() - 2);
    }
    return s;
}

static bool HasTextureFileExtension(const std::string& name)
{
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= name.size())
        return false;
    std::string ext = name.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp";
}

static bool TextureSourceExists(const std::string& sourcePath)
{
    if (sourcePath.empty())
        return false;
    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();
    std::error_code ec;
    return std::filesystem::exists(AssetPaths::Get().ToAbsolutePath(sourcePath), ec);
}

static std::string ResolveTextureSourcePath(const std::string& token)
{
    if (!AssetPaths::IsInitialized())
        AssetPaths::Init();
    return AssetPaths::Get().ResolveToSourcePath(token);
}
} // namespace

std::string Util::ResolveShaderDefaultTexturePath(const std::string& token)
{
    std::string name = TrimTextureToken(token);
    if (name.empty())
        return {};

    if (name.find("://") != std::string::npos)
        return ResolveTextureSourcePath(name);

    const bool hasSlash = name.find('/') != std::string::npos || name.find('\\') != std::string::npos;
    if (hasSlash)
    {
        const std::string resolved = ResolveTextureSourcePath(name);
        return resolved.empty() ? name : resolved;
    }

    const std::string base = DefaultTextureBase();
    if (HasTextureFileExtension(name))
    {
        const std::string path = base + name;
        if (TextureSourceExists(path))
            return path;
        if (TextureSourceExists(name))
            return ResolveTextureSourcePath(name);
        return path;
    }

    static const char* kExts[] = {".png", ".jpg", ".jpeg"};
    for (const char* ext : kExts)
    {
        const std::string path = base + name + ext;
        if (TextureSourceExists(path))
            return path;
    }
    return base + name + ".png";
}

bool Util::TextureLoadUsesSrgb(const std::string& path, const std::string& propertyName)
{
    auto containsNormal = [](const std::string& s)
    {
        std::string lower = s;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return lower.find("normal") != std::string::npos;
    };
    if (containsNormal(path))
        return false;
    if (!propertyName.empty() && containsNormal(propertyName))
        return false;
    return true;
}

std::string Util::GetNameFromPath(const std::string& path)
{
    return path.substr(path.find_last_of("/") + 1, path.find_last_of(".") - path.find_last_of("/") - 1);
}

std::string Util::GetUniqueName(const std::string& name, const std::vector<std::string>& existNames)
{
    int index = 0;
    std::string uniqueName;
    while (true)
    {
        if (index == 0)
            uniqueName = name;
        else
            uniqueName = name + "_" + std::to_string(index);
        auto it = std::find(existNames.begin(), existNames.end(), uniqueName);
        if (it == existNames.end())
            return uniqueName;
        index++;
    }
}

float Util::lerp(float a, float b, float f)
{
    return a + f * (b - a);
}

void Util::ClearScreen(GLenum buffer, float r, float g, float b, float a)
{
    glStencilMask(0xFF);
    glClearStencil(0);
    glDepthMask(GL_TRUE);
    glClearColor(r, g, b, a);
    glClear(buffer);
}

#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>

class Util
{
  public:
    static std::string GetNameFromPath(const std::string& path);
    static std::string GetUniqueName(const std::string& name, const std::vector<std::string>& existNames);
    static void ClearScreen(GLenum buffer = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                            float r = 0, float g = 0, float b = 0, float a = 0);
    static float lerp(float a, float b, float f);
    static std::string ResolveShaderDefaultTexturePath(const std::string& token);
    static bool TextureLoadUsesSrgb(const std::string& path, const std::string& propertyName = {});
};

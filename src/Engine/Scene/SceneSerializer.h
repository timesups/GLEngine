#pragma once
#include <string>

class RenderContext;

class SceneSerializer
{
  public:
    static bool SaveScene(const std::string& path, const RenderContext& context, std::string& outMessage);
    static bool LoadScene(const std::string& path, RenderContext& context, std::string& outMessage);
    static bool CreateEmptySceneFile(const std::string& dirPath, const char* nameInput, std::string& outFilePath,
                                     std::string& outMessage);
};

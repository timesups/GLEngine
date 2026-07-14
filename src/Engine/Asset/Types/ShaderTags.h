#pragma once

#include <string>
#include <unordered_map>
#include <vector>

using ShaderTagMap = std::unordered_map<std::string, std::string>;

class Shader;
class ShaderPass;

/// 组装 "Key:Value" 文本 Tag（保留原样大小写；匹配时忽略大小写）。
std::string MakeShaderTag(const std::string& key, const std::string& value);
bool ParseShaderTag(const std::string& tag, std::string& outKey, std::string& outValue);

bool ShaderTagEqualsIgnoreCase(const std::string& a, const std::string& b);
/// 将完整 Tag 规范为小写，用于索引查找。
std::string NormalizeShaderTag(const std::string& tag);
const std::string* FindShaderTag(const ShaderTagMap& tags, const std::string& key);

/// 将 Config::renderPipeline 映射为默认 LightMode（Forward -> Forward，其余 -> Deferred）。
std::string DefaultLightModeForActivePipeline();

/// 根据 DrawSetting 中的 tag 列表补全默认 LightMode。
std::vector<std::string> BuildEffectiveShaderTags(const std::vector<std::string>& drawTags);

bool PassMatchesShaderTags(const ShaderPass& pass, const std::vector<std::string>& drawTags);
ShaderPass* FindShaderPassForTags(Shader& shader, const std::vector<std::string>& drawTags);
bool ShaderHasShaderTag(const Shader& shader, const std::string& tag);

#include "RenderQueue.h"

#include <cctype>

namespace
{
bool TryParseInt(const std::string& token, int& outValue)
{
    if (token.empty())
        return false;
    size_t idx = 0;
    if (token[0] == '+' || token[0] == '-')
        idx = 1;
    if (idx >= token.size())
        return false;
    for (size_t i = idx; i < token.size(); ++i)
    {
        if (!std::isdigit(static_cast<unsigned char>(token[i])))
            return false;
    }
    try
    {
        outValue = std::stoi(token);
        return true;
    }
    catch (...)
    {
        return false;
    }
}
} // namespace

bool RenderQueue::TryParseToken(const std::string& token, int& outQueue)
{
    std::string lower = token;
    for (char& c : lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lower == "background" || lower == "bg")
    {
        outQueue = Background;
        return true;
    }
    if (lower == "opaque" || lower == "geometry")
    {
        outQueue = Geometry;
        return true;
    }
    if (lower == "alphatest" || lower == "alpha_test" || lower == "cutout")
    {
        outQueue = AlphaTest;
        return true;
    }
    if (lower == "skybox")
    {
        outQueue = Skybox;
        return true;
    }
    if (lower == "transparent" || lower == "translucent")
    {
        outQueue = Transparent;
        return true;
    }
    if (lower == "overlay")
    {
        outQueue = Overlay;
        return true;
    }
    return TryParseInt(token, outQueue);
}

int RenderQueue::DefaultFromBlendEnabled(bool blendEnabled)
{
    return blendEnabled ? Transparent : Geometry;
}

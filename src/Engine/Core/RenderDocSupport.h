#pragma once

struct GLFWwindow;

namespace RenderDocSupport
{
#if GLE_ENABLE_RENDERDOC
void Init();
bool IsAvailable();

void TriggerCapture();
void StartFrameCapture();
void EndFrameCapture();
bool IsFrameCapturing();

void SetActiveWindow(GLFWwindow* window);
void ApplyOverlaySettings();
void Reload();
#else
inline void Init() {}
inline bool IsAvailable() { return false; }
inline void TriggerCapture() {}
inline void StartFrameCapture() {}
inline void EndFrameCapture() {}
inline bool IsFrameCapturing() { return false; }
inline void SetActiveWindow(GLFWwindow*) {}
inline void ApplyOverlaySettings() {}
inline void Reload() {}
#endif
} // namespace RenderDocSupport

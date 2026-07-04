#include "RenderDocSupport.h"

#if GLE_ENABLE_RENDERDOC

#include "Config.h"
#include "Log.h"

#include <filesystem>
#include <string>

#include <renderdoc_app.h>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <Windows.h>
#endif

#include <GLFW/glfw3.h>
#if defined(_WIN32)
#include <GLFW/glfw3native.h>
#endif

#define MODULE "RenderDoc"

namespace
{
	RENDERDOC_API_1_6_0* s_api = nullptr;
	HMODULE s_loadedModule = nullptr;

#if defined(_WIN32)
	std::string ResolveRenderDocDllPath(const std::string& configured)
	{
		if (configured.empty())
			return {};

		std::filesystem::path path(configured);
		std::error_code ec;
		if (std::filesystem::is_directory(path, ec))
			path /= "renderdoc.dll";
		else if (path.extension().empty())
			path /= "renderdoc.dll";
		return path.string();
	}

	bool TryBindApi(void* mod, const char* sourceLabel)
	{
		if (!mod)
			return false;

		auto getApi =
			reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(static_cast<HMODULE>(mod), "RENDERDOC_GetAPI"));
		if (!getApi)
		{
			Log(MODULE, LogLevel::WARNING, "RenderDoc module at {} missing RENDERDOC_GetAPI", sourceLabel);
			return false;
		}

		RENDERDOC_API_1_6_0* api = nullptr;
		if (!getApi(eRENDERDOC_API_Version_1_6_0, reinterpret_cast<void**>(&api)))
		{
			Log(MODULE, LogLevel::WARNING, "RenderDoc module at {} does not support API 1.6.0", sourceLabel);
			return false;
		}

		s_api = api;
		return true;
	}

	void* LoadRenderDocModule()
	{
		if (HMODULE injected = GetModuleHandleA("renderdoc.dll"))
		{
			if (TryBindApi(injected, "injected renderdoc.dll"))
				return injected;
			s_api = nullptr;
			return nullptr;
		}

		const std::string configuredDll = ResolveRenderDocDllPath(Config::Get().renderDocPath);
		if (!configuredDll.empty())
		{
			if (HMODULE mod = LoadLibraryA(configuredDll.c_str()))
			{
				if (TryBindApi(mod, configuredDll.c_str()))
				{
					s_loadedModule = mod;
					return mod;
				}
				FreeLibrary(mod);
				s_api = nullptr;
			}
			else
			{
				Log(MODULE, LogLevel::WARNING, "Failed to load RenderDoc from configured path: {}", configuredDll);
			}
		}

		const char* installPaths[] = {
			"C:\\Program Files\\RenderDoc\\renderdoc.dll",
			"C:\\Program Files (x86)\\RenderDoc\\renderdoc.dll",
		};
		for (const char* path : installPaths)
		{
			if (HMODULE mod = LoadLibraryA(path))
			{
				if (TryBindApi(mod, path))
				{
					s_loadedModule = mod;
					return mod;
				}
				FreeLibrary(mod);
				s_api = nullptr;
			}
		}
		return nullptr;
	}

	void ShutdownRenderDocModule()
	{
		s_api = nullptr;
		if (s_loadedModule)
		{
			FreeLibrary(s_loadedModule);
			s_loadedModule = nullptr;
		}
	}
#endif
}

void RenderDocSupport::Init()
{
	if (s_api)
		return;

#if defined(_WIN32)
	if (!LoadRenderDocModule())
	{
		Log(MODULE, LogLevel::INFO,
		    "RenderDoc not loaded (set renderDocPath in config, install RenderDoc, or launch via qrenderdoc)");
		return;
	}

	s_api->UnloadCrashHandler();

	int major = 0;
	int minor = 0;
	int patch = 0;
	if (s_api->GetAPIVersion)
		s_api->GetAPIVersion(&major, &minor, &patch);

	Log(MODULE, LogLevel::INFO, "RenderDoc API ready (installed {}.{}.{})", major, minor, patch);
	ApplyOverlaySettings();
#else
	Log(MODULE, LogLevel::INFO, "RenderDoc integration is only enabled on Windows in this build");
#endif
}

void RenderDocSupport::Reload()
{
#if defined(_WIN32)
	ShutdownRenderDocModule();
	if (Config::Get().enableRenderDoc)
		Init();
#endif
}

bool RenderDocSupport::IsAvailable()
{
	return s_api != nullptr;
}

void RenderDocSupport::TriggerCapture()
{
	if (!s_api || !s_api->TriggerCapture)
		return;
	s_api->TriggerCapture();
	Log(MODULE, LogLevel::INFO, "RenderDoc: capture triggered for next frame");
}

void RenderDocSupport::StartFrameCapture()
{
	if (!s_api || !s_api->StartFrameCapture)
		return;
	s_api->StartFrameCapture(nullptr, nullptr);
	Log(MODULE, LogLevel::INFO, "RenderDoc: frame capture started");
}

void RenderDocSupport::EndFrameCapture()
{
	if (!s_api || !s_api->EndFrameCapture)
		return;
	s_api->EndFrameCapture(nullptr, nullptr);
	Log(MODULE, LogLevel::INFO, "RenderDoc: frame capture ended");
}

bool RenderDocSupport::IsFrameCapturing()
{
	if (!s_api || !s_api->IsFrameCapturing)
		return false;
	return s_api->IsFrameCapturing() != 0;
}

void RenderDocSupport::SetActiveWindow(GLFWwindow* window)
{
	if (!s_api || !s_api->SetActiveWindow || !window)
		return;

#if defined(_WIN32)
	s_api->SetActiveWindow(nullptr, glfwGetWin32Window(window));
#else
	(void)window;
#endif

	ApplyOverlaySettings();
}

void RenderDocSupport::ApplyOverlaySettings()
{
	if (!s_api || !s_api->MaskOverlayBits)
		return;

	const uint32_t overlayMask = Config::Get().showRenderDocOverlay ? eRENDERDOC_Overlay_Default
	                                                                  : eRENDERDOC_Overlay_None;
	s_api->MaskOverlayBits(0, overlayMask);
}

#undef MODULE

#endif

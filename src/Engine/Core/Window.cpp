#include "Window.h"

#include "../Core/Log.h"
#include "Config.h"
#include "GLFW/glfw3.h"
#include "RenderDocSupport.h"

#include "../../../external/imgui/imgui.h"
#include "../../../external/imgui/imgui_internal.h"
#include "../Entity/Components/Camera.h"
#include "../Entity/Entity.h"

#include <cmath>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif

#include "Resource.h"

namespace
{
void ApplyEmbeddedWindowIcon(GLFWwindow* window)
{
    HICON hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_ICON1));
    if (!hIcon)
        return;
    HWND hwnd = glfwGetWin32Window(window);
    SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
}
} // namespace
#endif

#define MODULE "Window"

/// 将 Dock 中央节点映射到帧缓冲像素；失败则返回 false（调用方用整窗）
static bool SceneViewportFromDockSpace(ImGuiID dockspaceRootId, int fbW, int fbH, int* outX, int* outY, int* outW,
                                       int* outH)
{
    const ImGuiIO& io = ImGui::GetIO();
    if ((io.ConfigFlags & ImGuiConfigFlags_DockingEnable) == 0 || dockspaceRootId == 0 || fbW <= 0 || fbH <= 0)
        return false;

    ImGuiDockNode* central = ImGui::DockBuilderGetCentralNode(dockspaceRootId);
    if (!central || !central->IsVisible || central->Size.x < 1.f || central->Size.y < 1.f)
        return false;

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float p0x = central->Pos.x - vp->Pos.x;
    const float p0y = central->Pos.y - vp->Pos.y;
    const float p1x = p0x + central->Size.x;
    const float p1y = p0y + central->Size.y;
    const float sx = (io.DisplaySize.x > 0.f) ? (static_cast<float>(fbW) / io.DisplaySize.x) : 1.f;
    const float sy = (io.DisplaySize.y > 0.f) ? (static_cast<float>(fbH) / io.DisplaySize.y) : 1.f;

    int x = static_cast<int>(std::floor(p0x * sx));
    int yTop = static_cast<int>(std::floor(p0y * sy));
    const int x1 = static_cast<int>(std::ceil(p1x * sx));
    const int y1 = static_cast<int>(std::ceil(p1y * sy));
    int rw = x1 - x;
    int rh = y1 - yTop;
    if (rw < 1 || rh < 1)
        return false;

    if (x < 0)
        x = 0;
    if (x > fbW - 1)
        x = fbW - 1;
    if (yTop < 0)
        yTop = 0;
    if (yTop > fbH - 1)
        yTop = fbH - 1;
    if (rw > fbW - x)
        rw = fbW - x;
    if (rh > fbH - yTop)
        rh = fbH - yTop;
    if (rw < 1 || rh < 1)
        return false;

    const int glY = fbH - yTop - rh;
    *outX = x;
    *outY = glY;
    *outW = rw;
    *outH = rh;
    return true;
}

static void ApplySceneViewport(RenderContext& context, int fbW, int fbH, bool showUI, Gui& ui)
{
    int svx = 0;
    int svy = 0;
    int svw = fbW;
    int svh = fbH;

    if (showUI)
    {
        const ImGuiID dockId = ui.BuildWidgets(context);
        SceneViewportFromDockSpace(dockId, fbW, fbH, &svx, &svy, &svw, &svh);
    }

    context.sceneViewportX = svx;
    context.sceneViewportY = svy;
    context.sceneViewportWidth = svw;
    context.sceneViewportHeight = svh;
    context.width = svw;
    context.height = svh;
}

static void processInput(GLFWwindow* window, bool rmbLook, RenderContext& context)
{
    // if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    //     glfwSetWindowShouldClose(window, true);
    Window* w = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!w)
        return;

    w->m_axisForward = 0.f;
    w->m_axisRight = 0.f;
    w->m_axisWorldUp = 0.f;

    // 按住右键：独占式游戏输入；否则仅在 ImGui 不抢键盘时处理行走键
    const bool gameOnly = rmbLook;
    const bool allowMoveKeys = gameOnly || !ImGui::GetIO().WantCaptureKeyboard;
    if (allowMoveKeys)
    {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            w->m_axisForward += 1.f;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            w->m_axisForward -= 1.f;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            w->m_axisRight += 1.f;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            w->m_axisRight -= 1.f;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            w->m_axisWorldUp += 1.f;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            w->m_axisWorldUp -= 1.f;
    }

    if (allowMoveKeys)
        w->m_sprintHeld = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
    else
        w->m_sprintHeld = false;

    // G 键边沿触发切换 UI；ImGui 正在使用键盘（如输入框）时不响应
    const ImGuiIO& io = ImGui::GetIO();
    const bool uiToggleDown = glfwGetKey(window, GLFW_KEY_G) == GLFW_PRESS;
    if (uiToggleDown && !w->m_uiToggleKeyHeld && !io.WantCaptureKeyboard && !io.WantTextInput)
        context.ShowUI = !context.ShowUI;
    w->m_uiToggleKeyHeld = uiToggleDown;
}

static bool RmbCameraLookActive(GLFWwindow* window)
{
    return glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS && !ImGui::GetIO().WantCaptureMouse;
}

static void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    Window* w = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!w)
        return;
    if (!RmbCameraLookActive(window))
        return;
    if (w->m_firstMouse)
    {
        w->m_lastMouseX = xpos;
        w->m_lastMouseY = ypos;
        w->m_firstMouse = false;
        return;
    }
    w->m_mouseAccX += xpos - w->m_lastMouseX;
    w->m_mouseAccY += ypos - w->m_lastMouseY;
    w->m_lastMouseX = xpos;
    w->m_lastMouseY = ypos;
}

static void scroll_callback(GLFWwindow* window, double /*xoffset*/, double yoffset)
{
    Window* w = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (w && RmbCameraLookActive(window))
        w->AccumulateScrollDeltaY(static_cast<float>(yoffset));
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    Window* myWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (myWindow)
    {
        myWindow->vWidth = width;
        myWindow->vHeight = height;
    }
    if (width <= 0 || height <= 0)
        LogA(LogLevel::WARNING, "Framebuffer size invalid: {}x{}", width, height);
    glViewport(0, 0, width, height);
}

Window::Window()
{
    if (!glfwInit())
        LogA(LogLevel::ERROR, "glfwInit failed");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, Config::Get().openGLMajor);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, Config::Get().openGLMinor);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

Window::~Window()
{
    LogA(LogLevel::INFO, "Window destroying");
    ui.Clean();
    // 清理资源
    if (win)
        glfwDestroyWindow(win);
    glfwTerminate();
}

bool Window::Create(const int w, const int h, const char* title)
{

    vWidth = w;
    vHeight = h;
    win = glfwCreateWindow(vWidth, vHeight, title, NULL, NULL);
    if (!win)
    {
        LogA(LogLevel::ERROR, "Failed to create window.");
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(win);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        LogA(LogLevel::ERROR, "Failed to load glad");
        return false;
    }

#ifndef GL_TEXTURE_CUBE_MAP_SEAMLESS
#define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F
#endif
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    glfwSwapInterval(Config::Get().vsync ? 1 : 0);

    GLint glMajor = 0, glMinor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &glMajor);
    glGetIntegerv(GL_MINOR_VERSION, &glMinor);

    glViewport(0, 0, vWidth, vHeight);

    glfwSetWindowUserPointer(win, this);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);
    glfwSetCursorPosCallback(win, mouse_callback);
    glfwSetScrollCallback(win, scroll_callback);

    m_firstMouse = true;
    glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

#if defined(_WIN32)
    ApplyEmbeddedWindowIcon(win);
#endif

    LogA(LogLevel::INFO, "Window created {}x{}, OpenGL {}.{}", vWidth, vHeight, glMajor, glMinor);
#if GLE_ENABLE_RENDERDOC
    if (Config::Get().enableRenderDoc)
        RenderDocSupport::SetActiveWindow(win);
#endif
    // 初始化imgui
    ui.Init(this);
    return true;
}

void Window::Run(RenderContext& context)
{
    float lastTime = static_cast<float>(glfwGetTime());
    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        const bool rmbLook = RmbCameraLookActive(win);
        if (rmbLook != m_rmbLookPrev)
        {
            m_firstMouse = true;
            glfwSetInputMode(win, GLFW_CURSOR, rmbLook ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            m_rmbLookPrev = rmbLook;
        }

        const float now = static_cast<float>(glfwGetTime());
        context.engineTime = now;
        deltaTime = now - lastTime;
        lastTime = now;

        context.deltaTime = deltaTime;
        context.framebufferWidth = vWidth;
        context.framebufferHeight = vHeight;

        processInput(win, rmbLook, context);

        const bool showUI = context.ShowUI;
        if (showUI)
            ui.BeginFrame(rmbLook);

        if (context.currentCamera)
        {
            if (Camera* cam = context.currentCamera->GetComponent<Camera>())
            {
                cam->QueueScrollDeltaY(m_scrollDeltaY);
                cam->QueueMouseLookDelta(m_mouseAccX, m_mouseAccY);
                cam->SetMoveAxes(m_axisForward, m_axisRight, m_axisWorldUp);
                cam->SetSprint(m_sprintHeld);
            }
        }
        m_scrollDeltaY = 0.f;
        m_mouseAccX = 0.0;
        m_mouseAccY = 0.0;

        ApplySceneViewport(context, vWidth, vHeight, showUI, ui);

        if (context.currentCamera)
        {
            if (Camera* cam = context.currentCamera->GetComponent<Camera>())
            {
                const int svw = context.sceneViewportWidth;
                const int svh = context.sceneViewportHeight;
                const float aspect = (svh > 0) ? (static_cast<float>(svw) / static_cast<float>(svh)) : (16.f / 9.f);
                cam->SetAspect(aspect);
            }
        }

        Config::Get().initialWidth = context.framebufferWidth;
        Config::Get().initialHeight = context.framebufferHeight;

        glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, "Pass_Render");

        // 保证窗口只有在前台的时候,有效画满才会被渲染.
        bool iconified = glfwGetWindowAttrib(win, GLFW_ICONIFIED) == GLFW_FALSE;
        if (renderCallback && iconified)
            renderCallback(context);
        glPopDebugGroup();

        if (showUI)
            ui.EndFrame();

        glfwSwapBuffers(win);
    }
    LogA(LogLevel::INFO, "Main loop exited");
}

void Window::Close()
{
    if (win)
        glfwSetWindowShouldClose(win, GLFW_TRUE);
}

#undef MODULE
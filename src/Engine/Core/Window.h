#pragma once
#include <functional>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "../Renderer/RenderContext.h"
#include "Gui.h"

using RenderPipelineCallback = std::function<void(RenderContext&)>;

class Window
{
  public:
    Window();
    ~Window();
    bool Create(const int w, const int h, const char* title);
    void Run(RenderContext& context);
    void Close();

    int vWidth = 0;
    int vHeight = 0;
    float deltaTime = 0.f;
    RenderPipelineCallback renderCallback;

    void AccumulateScrollDeltaY(float dy)
    {
        m_scrollDeltaY += dy;
    }
    GLFWwindow* win = nullptr;

  private:
    float m_scrollDeltaY = 0.f;

    double m_mouseAccX = 0.0;
    double m_mouseAccY = 0.0;
    double m_lastMouseX = 0.0;
    double m_lastMouseY = 0.0;
    bool m_firstMouse = true;

    float m_axisForward = 0.f;
    float m_axisRight = 0.f;
    float m_axisWorldUp = 0.f;
    bool m_sprintHeld = false;

    /// 上一帧是否处于右键拖拽视角（用于边沿切换光标模式、重置首帧鼠标）
    bool m_rmbLookPrev = false;
    /// G 键上一帧是否按下（边沿触发 UI 切换，避免长按时每帧翻转）
    bool m_uiToggleKeyHeld = false;
    Gui ui;
    friend void processInput(GLFWwindow* window, bool rmbLook, RenderContext& context);
    friend void mouse_callback(GLFWwindow* window, double xpos, double ypos);
    friend void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    friend void scroll_callback(GLFWwindow* window, double xoffse, double yoffset);
};

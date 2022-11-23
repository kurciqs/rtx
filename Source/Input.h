#ifndef RTX_INPUT_H
#define RTX_INPUT_H

#include "GLFW/glfw3.h"

namespace SauronLT::Input {
    void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);

    void mouseCallback(GLFWwindow *window, double xpos, double ypos);

    void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods);

    void mouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset);

    bool IsKeyDown(int key);

    bool IsMouseButtonDown(int mouseButton);

    void GetCursorPos(float *x, float *y);

    bool IsMouseButtonUp(int mouseButton);

    void SetCursorMode(int mode);
} // Input

#endif //RTX_INPUT_H

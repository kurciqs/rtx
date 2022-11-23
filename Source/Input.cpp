#include "Input.h"

namespace SauronLT::Input {
        static bool keyPressedData[GLFW_KEY_LAST] = {};
        static bool mouseButtonPressedData[GLFW_MOUSE_BUTTON_LAST] = {};
        static bool mouseButtonReleasedData[GLFW_MOUSE_BUTTON_LAST] = {};
        static float mouseX = 0.0f;
        static float mouseY = 0.0f;
        static float mouseScrollX = 0.0f;
        static float mouseScrollY = 0.0f;
        static GLFWwindow* s_Window;

        void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods) {
            if (key >= 0 && key < GLFW_KEY_LAST) {
                keyPressedData[key] = (action == GLFW_PRESS) || (action == GLFW_REPEAT);
            }
        }

        void mouseCallback(GLFWwindow *window, double xpos, double ypos) {
            mouseX = (float) xpos;
            mouseY = (float) ypos;
        }

        void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods) {
            if (button >= 0 && button < GLFW_MOUSE_BUTTON_LAST) {
                mouseButtonPressedData[button] = (action == GLFW_PRESS);
                mouseButtonReleasedData[button] = (action == GLFW_RELEASE);
            }
        }

        void mouseScrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
            mouseScrollX = (float) xoffset;
            mouseScrollY = (float) yoffset;
        }

        bool IsKeyDown(int key) {
            if (key >= 0 && key < GLFW_KEY_LAST) {
                return keyPressedData[key];
            }

            return false;
        }

        bool IsMouseButtonDown(int mouseButton) {
            if (mouseButton >= 0 && mouseButton < GLFW_MOUSE_BUTTON_LAST) {
                return mouseButtonPressedData[mouseButton];
            }

            return false;
        }

        void GetCursorPos(float *x, float *y) {
            *x = mouseX;
            *y = mouseY;
        }

        bool IsMouseButtonUp(int mouseButton) {
            return mouseButtonReleasedData[mouseButton];
        }
}
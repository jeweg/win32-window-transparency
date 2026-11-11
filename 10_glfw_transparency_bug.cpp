#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <cstdio>
#include <cstdlib>

#include "checkerboard.h"
#include "surface_readback.h"

int main() {
    if (!glfwInit()) {
        return EXIT_FAILURE;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(300, 200, "GLFW Window", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return EXIT_FAILURE;
    }
    HWND hwnd = glfwGetWin32Window(window);
    ShowCheckerboardBehind(hwnd);

    glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int, int action, int) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            glfwSetWindowShouldClose(w, GLFW_TRUE);
        }
    });

    // Let GLFW handle the first messages and DWM compose a few frames.
    for (int i = 0; i < 5; ++i) {
        glfwPollEvents();
        DwmFlush();
        Sleep(50);
    }
    printf("Surface in the middle of the client area: %08lX (ARGB)\n", ReadSurfacePixel(hwnd, 150, 100));
    printf("Press Escape in the window to exit.\n");
    fflush(stdout);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}

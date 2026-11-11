/*
OpenGL in a window set up like 05 (black brush + blur-behind), with CS_OWNDC
as OpenGL programs usually have it. Cleared to transparent, with a 50% red
square in the middle (premultiplied).

  --layered    WS_EX_LAYERED, with SetLayeredWindowAttributes(255, LWA_ALPHA)
  --no-own-dc  no CS_OWNDC
  --no-blur    no DwmEnableBlurBehindWindow

Whether this is transparent also depends on how the driver presents OpenGL.
The NVIDIA control panel has a setting for that, "Vulkan/OpenGL present
method". With "Prefer layered on DXGI Swapchain" OpenGL goes through a DXGI
swapchain, and then it only works with --layered (see the README).
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dwmapi.h>
#include <GL/gl.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "opengl32.lib")

#include <cstdio>
#include <cstring>

#include "checkerboard.h"

static bool g_running = true;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            g_running = false;
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) g_running = false;
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main(int argc, char** argv) {
    bool layered = false, ownDC = true, blur = true;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--layered")) layered = true;
        else if (!strcmp(argv[i], "--no-own-dc")) ownDC = false;
        else if (!strcmp(argv[i], "--no-blur")) blur = false;
    }

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    WNDCLASS wc = {};
    wc.style = ownDC ? CS_OWNDC : 0;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"OpenGLTransparencyTest";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    RegisterClass(&wc);

    const DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    const DWORD dwExStyle = layered ? WS_EX_LAYERED : 0;
    constexpr int SIZE = 300;
    RECT rect = {0, 0, SIZE, SIZE};
    AdjustWindowRectEx(&rect, dwStyle, FALSE, dwExStyle);

    HWND hwnd = CreateWindowEx(
        dwExStyle, wc.lpszClassName, L"OpenGL",
        dwStyle,
        100, 100, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd) {
        printf("CreateWindowEx failed: %lu\n", GetLastError());
        return 1;
    }
    if (layered) {
        BOOL ok = SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        printf("SetLayeredWindowAttributes: %s\n", ok ? "ok" : "failed");
    }

    if (blur) {
        HRGN region = CreateRectRgn(0, 0, -1, -1);
        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.fEnable = TRUE;
        bb.hRgnBlur = region;
        DwmEnableBlurBehindWindow(hwnd, &bb);
        DeleteObject(region);
    }

    HDC hdc = GetDC(hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    int format = ChoosePixelFormat(hdc, &pfd);
    DescribePixelFormat(hdc, format, sizeof(pfd), &pfd);
    SetPixelFormat(hdc, format, &pfd);
    printf("Pixel format %d: %d color bits, %d alpha bits\n", format, pfd.cColorBits, pfd.cAlphaBits);

    HGLRC context = wglCreateContext(hdc);
    wglMakeCurrent(hdc, context);
    printf("GL_RENDERER: %s\nGL_VERSION: %s\n", (const char*) glGetString(GL_RENDERER),
           (const char*) glGetString(GL_VERSION));
    fflush(stdout);

    typedef BOOL(WINAPI * SwapIntervalProc)(int);
    auto wglSwapIntervalEXT = (SwapIntervalProc) wglGetProcAddress("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT) wglSwapIntervalEXT(1);

    ShowCheckerboardBehind(hwnd);
    ShowWindow(hwnd, SW_SHOW);

    while (g_running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        glEnable(GL_SCISSOR_TEST);
        glScissor(75, 75, 150, 150);
        glClearColor(0.5f, 0, 0, 0.5f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
        SwapBuffers(hdc);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(context);
    ReleaseDC(hwnd, hdc);
    return 0;
}

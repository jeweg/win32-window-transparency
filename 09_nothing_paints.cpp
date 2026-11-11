/*
What GLFW does on Win32 for GLFW_TRANSPARENT_FRAMEBUFFER, without GLFW: NULL
background brush, WM_ERASEBKGND returns TRUE, and nobody ever paints the
client area. DWM alpha compositing is enabled exactly like in 05.

Nothing ever writes the redirection surface, so you see whatever it started
out with. The program prints the surface bytes in the middle of the client
area, read back through the window DC. Then it swaps the checkerboard's two
colors: where the window shows a copy of what was behind it instead of the
real thing, the squares don't swap.

  --popup        WS_POPUP instead of WS_OVERLAPPEDWINDOW
  --black-brush  a black class brush (what glfw#2681 adds), WM_ERASEBKGND still returns TRUE
  --layered      WS_EX_LAYERED with SetLayeredWindowAttributes(255, LWA_ALPHA) (what glfw#2925 adds)
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <cstdio>
#include <cstring>

#include "checkerboard.h"
#include "surface_readback.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return TRUE;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(hwnd);
            return 0;
    }
    // WM_PAINT ends up here, DefWindowProc just validates.
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main(int argc, char** argv) {
    bool popup = false, blackBrush = false, layered = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--popup")) popup = true;
        else if (!strcmp(argv[i], "--black-brush")) blackBrush = true;
        else if (!strcmp(argv[i], "--layered")) layered = true;
    }

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    const wchar_t CLASS_NAME[] = L"BasicWindowClass";
    WNDCLASS wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = blackBrush ? CreateSolidBrush(RGB(0, 0, 0)) : NULL;
    RegisterClass(&wc);

    const DWORD dwStyle = popup ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    RECT rect = {0, 0, 300, 300};
    AdjustWindowRect(&rect, dwStyle, FALSE);

    HWND hwnd = CreateWindowEx(
        layered ? WS_EX_LAYERED : 0, CLASS_NAME, L"Nothing paints",
        dwStyle,
        100, 100, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd) {
        return 1;
    }
    if (layered) {
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    }

    HRGN region = CreateRectRgn(0, 0, -1, -1);
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable = TRUE;
    bb.hRgnBlur = region;
    DwmEnableBlurBehindWindow(hwnd, &bb);
    DeleteObject(region);

    HWND checkerboard = ShowCheckerboardBehind(hwnd);
    ShowWindow(hwnd, SW_SHOW);

    // Let the window handle its first messages and DWM compose a few frames.
    for (int i = 0; i < 5; ++i) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        DwmFlush();
        Sleep(50);
    }
    printf("%s, %s class brush, WS_EX_LAYERED %s\n",
           popup ? "WS_POPUP" : "WS_OVERLAPPEDWINDOW", blackBrush ? "black" : "NULL", layered ? "on" : "off");
    printf("Surface in the middle of the client area: %08lX (ARGB)\n", ReadSurfacePixel(hwnd, 150, 150));
    FlipCheckerboard(checkerboard);
    printf("Swapped the checkerboard's colors. Press Escape in the window to exit.\n");
    fflush(stdout);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

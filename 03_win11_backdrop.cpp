/*
A Windows 11 system backdrop (Mica) and dark mode. Neither makes the client
area transparent, it stays as opaque as in 01.

Needs Windows SDK 10.0.22621 or newer for DWMWA_SYSTEMBACKDROP_TYPE.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include "checkerboard.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"BasicWindowClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(255, 0, 0));
    RegisterClass(&wc);

    // https://learn.microsoft.com/en-us/windows/win32/winmsg/window-styles
    constexpr DWORD dwStyle = WS_OVERLAPPEDWINDOW;

    // https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles
    constexpr DWORD dwExStyle = 0;

    RECT rect = {0, 0, 300, 300};
    AdjustWindowRect(&rect, dwStyle, FALSE);

    HWND hwnd = CreateWindowEx(
        dwExStyle, CLASS_NAME, L"Basic Window",
        dwStyle,
        100, 100, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd) {
        return 1;
    }

    // Dark mode for the frame. DWMWA_USE_IMMERSIVE_DARK_MODE is 20, Windows 10
    // builds before 20H1 used 19. Not required for the backdrop.
    constexpr BOOL darkmodeSet = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkmodeSet, sizeof(darkmodeSet));

    // Values:
    // DWMSBT_AUTO              (DWM decides, the default)
    // DWMSBT_NONE              (no backdrop)
    // DWMSBT_MAINWINDOW        (Mica)
    // DWMSBT_TRANSIENTWINDOW   (Acrylic)
    // DWMSBT_TABBEDWINDOW      (Mica Alt)
    constexpr DWM_SYSTEMBACKDROP_TYPE backdropType = DWMSBT_MAINWINDOW;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));

    ShowCheckerboardBehind(hwnd);
    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

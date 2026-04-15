/*
Win11-specific API to affect the non-client style.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

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

    // Optional (helps on Win11): immersive dark mode attribute.
    // 20 = DWMWA_USE_IMMERSIVE_DARK_MODE (older), 19 on some builds.
    // Not required for backdrop; safe to ignore if it fails.
    constexpr DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_20 = 20;
    constexpr BOOL darkmodeSet = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_20, &darkmodeSet, sizeof(darkmodeSet));

    // Values:
    // DWMSBT_AUTO              (Auto heuristic)
    // DWMSBT_NONE              (Disable)
    // DWMSBT_MAINWINDOW        (System theme Mica)
    // DWMSBT_TRANSIENTWINDOW   (Acrylic-like)
    // DWMSBT_TABBEDWINDOW      (System theme Mica Alt)
    constexpr DWM_SYSTEMBACKDROP_TYPE backdropType = DWMSBT_AUTO;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdropType, sizeof(backdropType));

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

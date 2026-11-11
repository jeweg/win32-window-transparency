/*
Translucent client area. This one uses a NULL background brush, but then we must
handle WM_ERASEBKGND and WM_PAINT in a very specific way.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include "checkerboard.h"

BOOL SetDwmAlphaCompositing(HWND hWnd, BOOL enable)
{
    if (enable) {
        // GDI swaps the coordinates, so this is one pixel at (-1, -1), outside
        // the client area, and the effect applies nowhere in it. A NULL or an
        // empty region would apply it behind the entire client area, which on
        // Windows 11 is a darkening layer that spoils the compositing (see 06).
        HRGN region = CreateRectRgn(0, 0, -1, -1);
        DWM_BLURBEHIND bb = {0};
        bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.hRgnBlur = region;
        bb.fEnable = TRUE;
        HRESULT hr = DwmEnableBlurBehindWindow(hWnd, &bb);
        DeleteObject(region);
        return SUCCEEDED(hr);
    } else {
        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE;
        bb.fEnable = FALSE;
        return SUCCEEDED(DwmEnableBlurBehindWindow(hWnd, &bb));
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_ERASEBKGND:
            // DO NOT call ValidateRect here!
            return 1;
                
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            // Clear all four channels, including alpha, to 0. A black brush fill
            // writes the same zero bytes (see 07_gdi_alpha).
            PatBlt(hdc,
                   ps.rcPaint.left,
                   ps.rcPaint.top,
                   ps.rcPaint.right  - ps.rcPaint.left,
                   ps.rcPaint.bottom - ps.rcPaint.top,
                   BLACKNESS);

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
    wc.hbrBackground = NULL;
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

    SetDwmAlphaCompositing(hwnd, TRUE);

    ShowCheckerboardBehind(hwnd);
    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

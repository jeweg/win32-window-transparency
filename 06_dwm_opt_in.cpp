/*
Different ways to tell DWM to use the alpha channel of the client area.

The client area is black (zero alpha, see 07) with a 50% red square in the
middle, copied from a premultiplied 32bpp DIB. Pick the opt-in on the command
line:

  --blur          DwmEnableBlurBehindWindow with the region CreateRectRgn(0, 0, -1, -1),
                  which is one pixel at (-1, -1), outside the client area (default, what 05 does)
  --blur-empty    the same with a really empty region, CreateRectRgn(0, 0, 0, 0)
  --blur-null     DwmEnableBlurBehindWindow with a NULL region
  --extend-frame  DwmExtendFrameIntoClientArea with -1 margins ("sheet of glass")
  --none          nothing, so DWM ignores alpha
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <cstring>

#include "checkerboard.h"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            // 0x80800000 is red at 50% alpha, premultiplied (A = 0x80, R = 0x80).
            constexpr int SIZE = 150;
            BITMAPINFO bi = {};
            bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
            bi.bmiHeader.biWidth = SIZE;
            bi.bmiHeader.biHeight = -SIZE;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;
            DWORD* pixels = nullptr;
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP bitmap = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, (void**) &pixels, nullptr, 0);
            for (int i = 0; i < SIZE * SIZE; ++i) pixels[i] = 0x80800000;
            HGDIOBJ old = SelectObject(memDC, bitmap);

            RECT client;
            GetClientRect(hwnd, &client);
            BitBlt(hdc, (client.right - SIZE) / 2, (client.bottom - SIZE) / 2, SIZE, SIZE, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, old);
            DeleteObject(bitmap);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR cmdLine, int nCmdShow) {
    const wchar_t CLASS_NAME[] = L"BasicWindowClass";
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    RegisterClass(&wc);

    constexpr DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    RECT rect = {0, 0, 300, 300};
    AdjustWindowRect(&rect, dwStyle, FALSE);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"DWM opt-in",
        dwStyle,
        100, 100, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd) {
        return 1;
    }

    if (strstr(cmdLine, "--blur-null")) {
        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE;
        bb.fEnable = TRUE;
        DwmEnableBlurBehindWindow(hwnd, &bb);
    } else if (strstr(cmdLine, "--extend-frame")) {
        MARGINS margins = {-1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    } else if (!strstr(cmdLine, "--none")) {
        HRGN region = strstr(cmdLine, "--blur-empty") ? CreateRectRgn(0, 0, 0, 0) : CreateRectRgn(0, 0, -1, -1);
        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.fEnable = TRUE;
        bb.hRgnBlur = region;
        DwmEnableBlurBehindWindow(hwnd, &bb);
        DeleteObject(region);
    }

    ShowCheckerboardBehind(hwnd);
    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

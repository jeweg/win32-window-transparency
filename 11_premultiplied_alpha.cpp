/*
DWM composes with premultiplied alpha:

  on screen = window color + background * (1 - window alpha)

Both squares show the same red disc, opaque in the middle and fading to
transparent at the edge. The left one is stored with straight alpha (full red
everywhere, only alpha fades), the right one premultiplied (red scaled down
with alpha). Window setup as in 05, the pixels are copied from a 32bpp DIB
(see 07).

The straight one comes out wrong: DWM adds the full red on top of the
background even where alpha is 0, so the whole square stays red, and the
disc's edge is too bright. The premultiplied one fades into the background.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <cmath>

#include "checkerboard.h"

constexpr int DISC = 160;
constexpr int GAP = 40;

void PaintDiscs(HDC hdc) {
    const int width = 2 * DISC + GAP;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -DISC;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    DWORD* pixels = nullptr;
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP bitmap = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, (void**) &pixels, nullptr, 0);
    HGDIOBJ old = SelectObject(memDC, bitmap);

    // The DIB section starts out zeroed, so the gap is transparent.
    for (int y = 0; y < DISC; ++y) {
        for (int x = 0; x < DISC; ++x) {
            float dx = x + 0.5f - DISC / 2.0f;
            float dy = y + 0.5f - DISC / 2.0f;
            float alpha = 1.0f - sqrtf(dx * dx + dy * dy) / (DISC / 2.0f);
            if (alpha < 0.0f) alpha = 0.0f;
            DWORD a = (DWORD) (alpha * 255.0f + 0.5f);

            // 0xAARRGGBB
            pixels[y * width + x] = (a << 24) | (255 << 16);
            pixels[y * width + DISC + GAP + x] = (a << 24) | (a << 16);
        }
    }

    BitBlt(hdc, 20, 20, width, DISC, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, old);
    DeleteObject(bitmap);
    DeleteDC(memDC);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintDiscs(hdc);
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
    wc.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    RegisterClass(&wc);

    constexpr DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    RECT rect = {0, 0, 2 * DISC + GAP + 40, DISC + 40};
    AdjustWindowRect(&rect, dwStyle, FALSE);

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Straight vs. premultiplied alpha",
        dwStyle,
        100, 100, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd) {
        return 1;
    }

    HRGN region = CreateRectRgn(0, 0, -1, -1);
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable = TRUE;
    bb.hRgnBlur = region;
    DwmEnableBlurBehindWindow(hwnd, &bb);
    DeleteObject(region);

    ShowCheckerboardBehind(hwnd);
    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

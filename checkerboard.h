/*
Used by all demos: puts a checkerboard window right behind the demo window.
That way you can see immediately what's transparent and what isn't, and the
screenshots always have the same known background instead of whatever is on
my desktop.

The checkerboard doesn't follow the demo window. Move the demo window off it
and you see your desktop again.
*/

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static bool g_checkerboardFlipped = false;

static LRESULT CALLBACK CheckerboardWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT client;
        GetClientRect(hwnd, &client);
        HBRUSH light = CreateSolidBrush(RGB(170, 170, 170));
        HBRUSH dark = CreateSolidBrush(RGB(85, 85, 85));
        constexpr int SIZE = 16;
        for (int y = 0; y < client.bottom; y += SIZE) {
            for (int x = 0; x < client.right; x += SIZE) {
                RECT r = {x, y, x + SIZE, y + SIZE};
                bool odd = ((x + y) / SIZE) % 2 != 0;
                FillRect(hdc, &r, odd != g_checkerboardFlipped ? light : dark);
            }
        }
        DeleteObject(light);
        DeleteObject(dark);
        EndPaint(hwnd, &ps);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Call this after creating the demo window and before showing it.
static HWND ShowCheckerboardBehind(HWND hwnd) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = CheckerboardWndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"CheckerboardBackdrop";
    RegisterClass(&wc);

    RECT rect;
    GetWindowRect(hwnd, &rect);
    constexpr int MARGIN = 40;

    // WS_EX_NOACTIVATE so that clicking it never brings it in front of the demo window.
    // If the demo window is topmost, the checkerboard has to be, too.
    DWORD exStyle = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | (GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST);
    HWND checkerboard = CreateWindowEx(
        exStyle, wc.lpszClassName, L"", WS_POPUP,
        rect.left - MARGIN, rect.top - MARGIN,
        rect.right - rect.left + 2 * MARGIN, rect.bottom - rect.top + 2 * MARGIN,
        nullptr, nullptr, wc.hInstance, nullptr
    );
    SetWindowPos(checkerboard, hwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    return checkerboard;
}

// Swaps the two colors. A window that shows what's behind it swaps along, a
// window that shows a copy of it doesn't (used by 09).
static void FlipCheckerboard(HWND checkerboard) {
    g_checkerboardFlipped = !g_checkerboardFlipped;
    RedrawWindow(checkerboard, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
}

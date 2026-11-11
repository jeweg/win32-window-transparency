/*
What GDI writes into the alpha channel of the redirection surface.

GDI brushes take a COLORREF, which has no alpha. But the redirection surface
has 32 bits per pixel, and with blur-behind DWM composes it with per-pixel
premultiplied alpha. So what matters is which bytes a GDI call leaves in the
surface.

The window is a row of nine cells. First all of them are made opaque white
with PatBlt(WHITENESS), which sets all 32 bits. Without that, a cell that
starts out zeroed would look transparent no matter what the operation does.
Then each cell gets one operation, from left to right:

  1. nothing
  2. FillRect with the stock BLACK_BRUSH
  3. FillRect with CreateSolidBrush(RGB(0, 0, 0))
  4. PatBlt(BLACKNESS)
  5. nothing, but it's a child window with a BLACK_BRUSH class brush, and we
     let DefWindowProc erase it
  6. FillRect with CreateSolidBrush(RGB(128, 128, 128))
  7. FillRect with (HBRUSH) (COLOR_WINDOW + 1), the default window background
  8. BitBlt from a 32bpp DIB filled with 50% red, premultiplied (0x80800000)
  9. FillRect with a pattern brush made from such a DIB (CreateDIBPatternBrushPt)

For the center of each cell it prints the surface bytes before and after the
operation, the checkerboard pixel behind it, and the pixel on screen. Reading
through the window DC gives you the surface including alpha, reading through
the screen DC gives you what DWM composed.

  --no-blur   no DwmEnableBlurBehindWindow, so DWM ignores alpha
  --layered   WS_EX_LAYERED with SetLayeredWindowAttributes(255, LWA_ALPHA)
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include <cstdio>
#include <cstring>

#include "checkerboard.h"

constexpr int CELL = 128;
constexpr int CELLS = 9;
constexpr int X = 100, Y = 100;

const char* LABELS[CELLS] = {
    "nothing",
    "FillRect, stock BLACK_BRUSH",
    "FillRect, RGB(0, 0, 0) brush",
    "PatBlt(BLACKNESS)",
    "BLACK_BRUSH class brush erase",
    "FillRect, RGB(128, 128, 128) brush",
    "FillRect, COLOR_WINDOW + 1",
    "BitBlt, 32bpp DIB 0x80800000",
    "FillRect, 32bpp DIB pattern brush",
};

// A memory DC with a 32bpp top-down DIB section selected into it.
HDC CreateDibDC(int width, int height, DWORD** pixels) {
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    HDC dc = CreateCompatibleDC(nullptr);
    SelectObject(dc, CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void**) pixels, nullptr, 0));
    return dc;
}

void DeleteDibDC(HDC dc) {
    HGDIOBJ bitmap = GetCurrentObject(dc, OBJ_BITMAP);
    DeleteDC(dc);
    DeleteObject(bitmap);
}

// All 32 bits of one pixel, as 0xAARRGGBB. GetPixel would drop the alpha.
// For the screen DC, pass SRCCOPY | CAPTUREBLT to include layered windows.
DWORD ReadPixel(HDC hdc, int x, int y, DWORD rop = SRCCOPY) {
    DWORD* pixel;
    HDC dib = CreateDibDC(1, 1, &pixel);
    BitBlt(dib, 0, 0, 1, 1, hdc, x, y, rop);
    GdiFlush();
    DWORD result = *pixel;
    DeleteDibDC(dib);
    return result;
}

void PrintRgb(DWORD pixel) {
    printf("%3lu,%3lu,%3lu  ", (pixel >> 16) & 0xFF, (pixel >> 8) & 0xFF, pixel & 0xFF);
}

// Let the windows handle their messages and DWM compose a few frames.
void Settle() {
    for (int i = 0; i < 5; ++i) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        DwmFlush();
        Sleep(50);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            // The cells are painted once from main(), don't touch them here.
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

int main(int argc, char** argv) {
    bool blur = true, layered = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--no-blur")) blur = false;
        else if (!strcmp(argv[i], "--layered")) layered = true;
    }

    // So that window and screen coordinates are the same pixels.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"GdiAlphaTest";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = NULL;
    RegisterClass(&wc);

    WNDCLASS childClass = {};
    childClass.lpfnWndProc = DefWindowProc;
    childClass.hInstance = hInstance;
    childClass.lpszClassName = L"BlackBrushChild";
    childClass.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
    RegisterClass(&childClass);

    // Topmost, so that what we read from the screen is our window even if
    // another one has the focus.
    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST | (layered ? WS_EX_LAYERED : 0), wc.lpszClassName, L"GDI alpha",
        WS_POPUP,
        X, Y, CELLS * CELL, CELL,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd) {
        return 1;
    }
    HWND child = CreateWindowEx(
        0, childClass.lpszClassName, L"",
        WS_CHILD | WS_VISIBLE,
        4 * CELL, 0, CELL, CELL,
        hwnd, nullptr, hInstance, nullptr
    );

    if (layered) {
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
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

    // Read the checkerboard before the window covers it. The center of
    // every cell is on a dark square.
    ShowCheckerboardBehind(hwnd);
    Settle();
    HDC screen = GetDC(nullptr);
    DWORD behind[CELLS];
    for (int i = 0; i < CELLS; ++i) {
        behind[i] = ReadPixel(screen, X + i * CELL + CELL / 2, Y + CELL / 2, SRCCOPY | CAPTUREBLT);
    }

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    Settle();

    HDC hdc = GetDC(hwnd);
    PatBlt(hdc, 0, 0, CELLS * CELL, CELL, WHITENESS);
    DWORD before[CELLS];
    for (int i = 0; i < CELLS; ++i) {
        before[i] = ReadPixel(hdc, i * CELL + CELL / 2, CELL / 2);
    }

    RECT r;
    SetRect(&r, 1 * CELL, 0, 2 * CELL, CELL);
    FillRect(hdc, &r, (HBRUSH) GetStockObject(BLACK_BRUSH));

    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    SetRect(&r, 2 * CELL, 0, 3 * CELL, CELL);
    FillRect(hdc, &r, black);
    DeleteObject(black);

    PatBlt(hdc, 3 * CELL, 0, CELL, CELL, BLACKNESS);

    HBRUSH gray = CreateSolidBrush(RGB(128, 128, 128));
    SetRect(&r, 5 * CELL, 0, 6 * CELL, CELL);
    FillRect(hdc, &r, gray);
    DeleteObject(gray);

    SetRect(&r, 6 * CELL, 0, 7 * CELL, CELL);
    FillRect(hdc, &r, (HBRUSH) (COLOR_WINDOW + 1));

    DWORD* pixels;
    HDC dib = CreateDibDC(CELL, CELL, &pixels);
    for (int i = 0; i < CELL * CELL; ++i) pixels[i] = 0x80800000;
    BitBlt(hdc, 7 * CELL, 0, CELL, CELL, dib, 0, 0, SRCCOPY);
    DeleteDibDC(dib);

    // A brush is just as happy with 32bpp pixels: a packed DIB, header and bits.
    struct {
        BITMAPINFOHEADER header;
        DWORD bits[8 * 8];
    } packed = {};
    packed.header.biSize = sizeof(packed.header);
    packed.header.biWidth = 8;
    packed.header.biHeight = 8;
    packed.header.biPlanes = 1;
    packed.header.biBitCount = 32;
    packed.header.biCompression = BI_RGB;
    for (DWORD& bit : packed.bits) bit = 0x80800000;
    HBRUSH pattern = CreateDIBPatternBrushPt(&packed, DIB_RGB_COLORS);
    SetRect(&r, 8 * CELL, 0, 9 * CELL, CELL);
    FillRect(hdc, &r, pattern);
    DeleteObject(pattern);
    ReleaseDC(hwnd, hdc);

    // The same erase that DefWindowProc does for any window with a class brush.
    RedrawWindow(child, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    Settle();

    printf("blur-behind %s, WS_EX_LAYERED %s\n\n", blur ? "on" : "off", layered ? "on" : "off");
    printf("%-36s %-9s %-9s %-12s %s\n", "", "before", "after", "behind", "on screen");
    hdc = GetDC(hwnd);
    for (int i = 0; i < CELLS; ++i) {
        DWORD after = ReadPixel(hdc, i * CELL + CELL / 2, CELL / 2);
        DWORD shown = ReadPixel(screen, X + i * CELL + CELL / 2, Y + CELL / 2, SRCCOPY | CAPTUREBLT);
        printf("%-36s %08lX  %08lX  ", LABELS[i], before[i], after);
        PrintRgb(behind[i]);
        PrintRgb(shown);
        printf("\n");
    }
    ReleaseDC(hwnd, hdc);
    ReleaseDC(nullptr, screen);
    printf("\nSurface bytes are ARGB. Press Escape in the window to exit.\n");
    fflush(stdout);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

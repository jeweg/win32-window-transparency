/*
Used by 09 and 10: reads one pixel of a window's redirection surface, all 32
bits of it. GetPixel would drop the alpha byte, BitBlt into a 32bpp DIB keeps
it. 07 does the same with its own helpers.
*/

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Client coordinates, returns 0xAARRGGBB.
static DWORD ReadSurfacePixel(HWND hwnd, int x, int y) {
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = 1;
    bi.bmiHeader.biHeight = -1;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    DWORD* pixel = nullptr;
    HDC memDC = CreateCompatibleDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, (void**) &pixel, nullptr, 0);
    HGDIOBJ old = SelectObject(memDC, bitmap);

    HDC hdc = GetDC(hwnd);
    BitBlt(memDC, 0, 0, 1, 1, hdc, x, y, SRCCOPY);
    ReleaseDC(hwnd, hdc);
    GdiFlush();
    DWORD result = *pixel;

    SelectObject(memDC, old);
    DeleteObject(bitmap);
    DeleteDC(memDC);
    return result;
}

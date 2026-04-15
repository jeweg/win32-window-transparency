/*
Translucent client area. This one uses a NULL background brush, but then we must
handle WM_ERASEBKGND and WM_PAINT in a very specific way.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

BOOL SetDwmAlphaCompositing(HWND hWnd, BOOL enable)
{
    if (enable) {
        // Note that specifying an empty region is not the same as
        // a null region: the latter will apply blur or shadowing (OS-dependent)
        // behind the client area. This would factor into (and spoil) compositing.
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
            // Clear to 0 bits (important: BLACKNESS is a bitwise op, not "RGB black").
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

    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

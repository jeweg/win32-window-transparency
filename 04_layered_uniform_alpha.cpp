/*
Full-window translucency with WS_EX_LAYERED.
Legacy approach, affects the entire window including decorations.
This will not get you per-pixel transparency.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
    constexpr DWORD dwExStyle = WS_EX_LAYERED;

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

    constexpr DWORD WHOLE_WINDOW_ALPHA = 128; // 0..255
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), WHOLE_WINDOW_ALPHA, LWA_ALPHA);
    
    ShowWindow(hwnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

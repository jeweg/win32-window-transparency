/*
D3D11 content in a window, presented in different ways. The swapchain is
cleared to transparent, with a 50% red square in the middle (premultiplied).
Window setup is the one from 05 (black brush + blur-behind) unless changed:

  --composition  CreateSwapChainForComposition with premultiplied alpha, as the
                 content of a DirectComposition visual (default)
  --flip         CreateSwapChainForHwnd, flip model (DXGI_SWAP_EFFECT_FLIP_DISCARD)
  --blit         CreateSwapChainForHwnd, bitblt model (DXGI_SWAP_EFFECT_DISCARD)

  --null-brush             NULL class brush, nothing clears the redirection surface (like GLFW)
  --no-blur                no DwmEnableBlurBehindWindow
  --no-redirection-bitmap  WS_EX_NOREDIRECTIONBITMAP

The HWND swapchains try premultiplied alpha first and print what DXGI says.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <wrl/client.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dwmapi.lib")

#include <cstdio>
#include <cstring>

#include "checkerboard.h"

using Microsoft::WRL::ComPtr;

static bool g_running = true;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CLOSE:
            g_running = false;
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) g_running = false;
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int main(int argc, char** argv) {
    bool flip = false, blit = false, nullBrush = false, noBlur = false, noRedirectionBitmap = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--flip")) flip = true;
        else if (!strcmp(argv[i], "--blit")) blit = true;
        else if (!strcmp(argv[i], "--null-brush")) nullBrush = true;
        else if (!strcmp(argv[i], "--no-blur")) noBlur = true;
        else if (!strcmp(argv[i], "--no-redirection-bitmap")) noRedirectionBitmap = true;
    }
    const bool composition = !flip && !blit;

    HINSTANCE hInstance = GetModuleHandle(nullptr);
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DxgiPresentTest";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullBrush ? NULL : (HBRUSH) GetStockObject(BLACK_BRUSH);
    RegisterClass(&wc);

    // No resizing, so the swapchain never needs to be recreated.
    const DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    const DWORD dwExStyle = noRedirectionBitmap ? WS_EX_NOREDIRECTIONBITMAP : 0;
    constexpr int SIZE = 300;
    RECT rect = {0, 0, SIZE, SIZE};
    AdjustWindowRectEx(&rect, dwStyle, FALSE, dwExStyle);

    HWND hwnd = CreateWindowEx(
        dwExStyle, wc.lpszClassName, L"DXGI present",
        dwStyle,
        100, 100, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hwnd) {
        return 1;
    }

    if (!noBlur) {
        HRGN region = CreateRectRgn(0, 0, -1, -1);
        DWM_BLURBEHIND bb = {};
        bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.fEnable = TRUE;
        bb.hRgnBlur = region;
        DwmEnableBlurBehindWindow(hwnd, &bb);
        DeleteObject(region);
    }

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, &context);
    if (FAILED(hr)) {
        printf("D3D11CreateDevice failed: 0x%08X\n", (unsigned) hr);
        return 1;
    }
    ComPtr<ID3D11DeviceContext1> context1;
    context.As(&context1);
    ComPtr<IDXGIDevice> dxgiDevice;
    device.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> adapter;
    dxgiDevice->GetAdapter(&adapter);
    ComPtr<IDXGIFactory2> factory;
    adapter->GetParent(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = SIZE;
    desc.Height = SIZE;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    ComPtr<IDXGISwapChain1> swapchain;
    ComPtr<IDCompositionDevice> dcomp;
    ComPtr<IDCompositionTarget> target;
    ComPtr<IDCompositionVisual> visual;
    if (composition) {
        desc.BufferCount = 2;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        hr = factory->CreateSwapChainForComposition(device.Get(), &desc, nullptr, &swapchain);
        printf("CreateSwapChainForComposition, premultiplied: 0x%08X\n", (unsigned) hr);
        if (FAILED(hr)) return 1;

        DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&dcomp));
        hr = dcomp->CreateTargetForHwnd(hwnd, TRUE, &target);
        printf("CreateTargetForHwnd: 0x%08X\n", (unsigned) hr);
        dcomp->CreateVisual(&visual);
        visual->SetContent(swapchain.Get());
        target->SetRoot(visual.Get());
        dcomp->Commit();
    } else {
        desc.BufferCount = flip ? 2 : 1;
        desc.SwapEffect = flip ? DXGI_SWAP_EFFECT_FLIP_DISCARD : DXGI_SWAP_EFFECT_DISCARD;
        hr = factory->CreateSwapChainForHwnd(device.Get(), hwnd, &desc, nullptr, nullptr, &swapchain);
        printf("CreateSwapChainForHwnd (%s), premultiplied: 0x%08X\n", flip ? "flip" : "bitblt", (unsigned) hr);
        if (FAILED(hr)) {
            desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
            hr = factory->CreateSwapChainForHwnd(device.Get(), hwnd, &desc, nullptr, nullptr, &swapchain);
            printf("CreateSwapChainForHwnd (%s), unspecified: 0x%08X\n", flip ? "flip" : "bitblt", (unsigned) hr);
            if (FAILED(hr)) return 1;
        }
    }
    fflush(stdout);

    ShowCheckerboardBehind(hwnd);
    ShowWindow(hwnd, SW_SHOW);

    ComPtr<ID3D11Texture2D> backbuffer;
    ComPtr<ID3D11RenderTargetView> rtv;
    while (g_running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // The bitblt model has only one buffer, but the flip models rotate,
        // so get the current one every frame.
        swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
        device->CreateRenderTargetView(backbuffer.Get(), nullptr, &rtv);
        const float transparent[4] = {0, 0, 0, 0};
        const float halfRed[4] = {0.5f, 0, 0, 0.5f};
        const D3D11_RECT square = {75, 75, 225, 225};
        context->ClearRenderTargetView(rtv.Get(), transparent);
        context1->ClearView(rtv.Get(), halfRed, &square, 1);
        swapchain->Present(1, 0);
    }
    return 0;
}

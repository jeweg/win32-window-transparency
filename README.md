# Windows Desktop Transparency — Reference Demos

Minimal, self-contained Win32 programs that demonstrate how window transparency
works on Windows 10/11 under DWM (Desktop Window Manager) composition.

These demos focus on the **window setup** side of transparency. 
With a properly configured window, APIs like [DirectComposition](https://learn.microsoft.com/en-us/windows/win32/directcomp/directcomposition-portal) or supported rendering APIs like Vulkan can present translucent content into it, with per-pixel alpha blending against underlying windows or the desktop.

## Background

On modern Windows, every window has a **DWM redirection surface** — a backing
bitmap that DWM composites onto the desktop. Transparency against content underneath the window depends on getting the alpha channel of this surface (and anything layered on top) right.

There are several mechanisms that interact:

| Mechanism | What it does |
|-----------|-------------|
| `hbrBackground` brush | Paints the client area on `WM_ERASEBKGND`. A `BLACK_BRUSH` writes zero alpha, making the redirection surface fully transparent. |
| `DwmEnableBlurBehindWindow` | Tells DWM to use the alpha channel of the redirection surface for per-pixel compositing. Without this, DWM treats the window as opaque regardless of alpha values. |
| `WS_EX_LAYERED` | Legacy whole-window alpha (uniform opacity). Simple but no per-pixel control. |
| `WS_EX_NOREDIRECTIONBITMAP` | Removes the redirection surface entirely. The window has no GDI backing. **Caution**: this breaks any rendering path that blits into the redirection surface (e.g., GDI, some Vulkan present paths). |
| `DWMWA_SYSTEMBACKDROP_TYPE` | Win11-only API for system backdrop effects (Mica, Acrylic). |

### The recommended setup for Vulkan/D3D transparency

1. Use `BLACK_BRUSH` as the window class background brush (zeroes alpha in the
   redirection surface).
2. Call `DwmEnableBlurBehindWindow` with `DWM_BB_ENABLE | DWM_BB_BLURREGION` and
   a full-window region (`CreateRectRgn(0, 0, -1, -1)`).
3. Create your swapchain with pre-multiplied alpha compositing
   (e.g., `VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR` for Vulkan).

Note that graphics driver implementations may choose to activate `DwmEnableBlurBehindWindow` automatically for a window 
if a non-opaque composite mode is requested -- I still recommend calling it anyway since you should not count on that.

### Do not use `WS_EX_NOREDIRECTIONBITMAP`

The Vulkan specification does not define the mechanism by which swapchain images
are presented to the desktop compositor. Some implementations may rely on the
window's redirection surface for presentation. Removing it with
`WS_EX_NOREDIRECTIONBITMAP` can cause swapchain content to not appear at all,
resulting in a fully transparent window. The recommended setup above
(`BLACK_BRUSH` + `DwmEnableBlurBehindWindow`) achieves proper transparency
without interfering with any presentation path.

### GLFW note

GLFW's `GLFW_TRANSPARENT_FRAMEBUFFER` hint calls `DwmEnableBlurBehindWindow`
internally, which is correct. However, as of GLFW 3.4, it does not properly
clear the redirection surface to zero alpha — the surface may start with
undefined alpha values, causing incorrect compositing. 

This problem is demonstrated [here](https://github.com/jeweg/glfw_repro1) and a fix was
submitted as [PR #2815](https://github.com/glfw/glfw/pull/2815). Until merged, consider avoiding GLFW
for window creation in scenarios where you need per-pixel translucency. 
I recommend using the reference code in `05_perpixel_alpha.cpp` instead.

## Test programs

Each program demonstrates a specific aspect of window transparency, building
progressively from a basic opaque window to per-pixel alpha compositing
with Vulkan.

| Program | Description |
|---------|-------------|
| `01_basic_opaque` | Basic opaque window with red background. Baseline reference. |
| `02_popup_no_decorations` | Same but with `WS_POPUP` style (no title bar / borders). |
| `03_win11_backdrop` | Win11 system backdrop API (`DWMWA_SYSTEMBACKDROP_TYPE`). |
| `04_layered_uniform_alpha` | Whole-window alpha via `WS_EX_LAYERED` + `SetLayeredWindowAttributes`. Uniform opacity applied to the entire window including decorations. Simple, but no per-pixel control — not sufficient for Vulkan/D3D `compositeAlpha` transparency. |
| `05_perpixel_alpha` | **Per-pixel translucent client area.** Uses `BLACK_BRUSH` + `DwmEnableBlurBehindWindow`. This is the recommended approach — it enables per-pixel alpha compositing in DWM and ensures the redirection surface is fully transparent. Required foundation for Vulkan `VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR`. |
| `06_perpixel_alpha_patblt` | Variant of 05: uses `NULL` background brush but manually clears to zero alpha in `WM_PAINT` via `PatBlt(BLACKNESS)`. Equivalent result, more control over when the clear happens. |
| `07_vulkan_perpixel_alpha` | **05 + Vulkan swapchain.** Builds on 05's window setup and adds a minimal Vulkan swapchain with `VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR`, clearing to 50% transparent black. Both prerequisites are visible: per-pixel compositing is enabled (via `DwmEnableBlurBehindWindow`), and the redirection surface is transparent (via `BLACK_BRUSH`). The complete, minimal reference for Vulkan transparency on Windows. |
| `08_glfw_transparency_bug` | GLFW window with `GLFW_TRANSPARENT_FRAMEBUFFER`. Demonstrates the GLFW transparency bug: GLFW enables per-pixel compositing via `DwmEnableBlurBehindWindow` but does not clear the redirection surface to zero alpha, so undefined alpha values cause incorrect compositing. |

## Building

Requires CMake 3.20+ and a C++17 compiler. GLFW 3.4 is fetched automatically.
The Vulkan test requires the Vulkan SDK to be installed.

```sh
cmake -S . -B build
cmake --build build
```

All programs exit on **Escape**.

#include "corelib.h"

#ifdef PLATFORM_WINDOWS

#include <Windows.h>
#include <Windowsx.h>

#include "imgui_impl_win32.h"

int platform_window_width;
int platform_window_height;
bool platform_window_is_open;

HWND g_hwnd;
WINDOWPLACEMENT prev_wp;
bool was_resized;

#define WINDOW_CLASS_NAME L"GraphicsWin32WindowClass"

static LARGE_INTEGER global_perf_freq;
static u64 nanoseconds_per_tick;

static LRESULT CALLBACK platform_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
        return true;
    
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW *cs = (CREATESTRUCTW *)lparam;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        } break;
        
        case WM_CLOSE:
        case WM_DESTROY: {
            platform_window_is_open = false;
        } break;

        case WM_SIZE: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            platform_window_width  = rect.right  - rect.left;
            platform_window_height = rect.bottom - rect.top;

            was_resized = true;
        } break;

        case WM_SYSCHAR: {
            // Prevent windows from beeping when pressing an alt-key combo.
        } break;

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            Key_Code key_code = (Key_Code)wparam;
            bool is_down = (msg == WM_KEYDOWN) || (msg == WM_SYSKEYDOWN);

            set_key_state(key_code, is_down);
        } break;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP: {
            bool is_down = msg == WM_LBUTTONDOWN;

            set_mouse_button_state(MOUSE_BUTTON_LEFT, is_down);
        } break;

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP: {
            bool is_down = msg == WM_RBUTTONDOWN;

            set_mouse_button_state(MOUSE_BUTTON_RIGHT, is_down);
        } break;

        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP: {
            bool is_down = msg == WM_MBUTTONDOWN;

            set_mouse_button_state(MOUSE_BUTTON_MIDDLE, is_down);
        } break;

        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP: {
            int button = GET_XBUTTON_WPARAM(wparam);

            Mouse_Button mouse_button;
            if (button & XBUTTON1) {
                mouse_button = MOUSE_BUTTON_X1;
            } else if (button & XBUTTON2) {
                mouse_button = MOUSE_BUTTON_X2;
            } else {
                Assert(!"Invalid mouse button");
                mouse_button = (Mouse_Button)0;
            }

            bool is_down = msg == WM_XBUTTONDOWN;
            
            set_mouse_button_state(mouse_button, is_down);
            
            return TRUE;
        } break;

        case WM_MOUSEMOVE: {
            int x_pos = GET_X_LPARAM(lparam);
            int y_pos = GET_Y_LPARAM(lparam);

            mouse_cursor_x_delta = x_pos - mouse_cursor_x;
            mouse_cursor_y_delta = mouse_cursor_y - y_pos;

            mouse_cursor_x = x_pos;
            mouse_cursor_y = y_pos;
        } break;

        case WM_MOUSEWHEEL: {
            int total_wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);

            mouse_scroll_wheel_y_delta = total_wheel_delta / WHEEL_DELTA; // 120
        } break;

        case WM_MOUSEHWHEEL: {
            int total_wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);

            mouse_scroll_wheel_x_delta = total_wheel_delta / WHEEL_DELTA; // 120
        } break;
            
        default: {
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        } break;
    }

    return 0;
}

bool platform_window_create(int width, int height, String title) {
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = platform_wnd_proc;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.hIcon         = LoadIconW(NULL, IDI_APPLICATION);
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = WINDOW_CLASS_NAME;
    wc.hIconSm       = LoadIconW(NULL, IDI_APPLICATION);

    if (RegisterClassExW(&wc) == 0) {
        logprintf("Failed to register the window class!\n");
        return false;
    }

    if (width <= 0 || height <= 0) {
        HWND desktop = GetDesktopWindow();
        RECT desktop_rect;
        GetWindowRect(desktop, &desktop_rect);

        int desktop_width  = desktop_rect.right - desktop_rect.left;
        int desktop_height = desktop_rect.bottom - desktop_rect.top;

        width  = (int)((double)desktop_width  * 2.0/3.0);
        height = (int)((double)desktop_height * 2.0/3.0);
    }

    DWORD window_style = WS_OVERLAPPEDWINDOW;
    
    RECT  window_rect  = {0, 0, width, height};
    AdjustWindowRect(&window_rect, window_style, FALSE);

    int window_width   = window_rect.right  - window_rect.left;
    int window_height  = window_rect.bottom - window_rect.top;

    wchar_t wide_title[4096] = {};
    MultiByteToWideChar(CP_UTF8, 0, title.data, title.length, wide_title, ArrayCount(wide_title));
    g_hwnd = CreateWindowExW(0, WINDOW_CLASS_NAME, wide_title, window_style,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             window_width, window_height,
                             NULL, NULL, GetModuleHandleW(NULL), NULL);
    if (g_hwnd == NULL) {
        logprintf("Failed to create window!\n");
        return false;
    }

    MONITORINFOEXW mi = { sizeof(mi) };
    GetMonitorInfoW(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);

    int monitor_width_without_taskbar  = mi.rcWork.right  - mi.rcWork.left;
    int monitor_height_without_taskbar = mi.rcWork.bottom - mi.rcWork.top;

    int window_x = mi.rcWork.left + ((monitor_width_without_taskbar  - window_width)  / 2);
    int window_y = mi.rcWork.top  + ((monitor_height_without_taskbar - window_height) / 2);

    SetWindowPos(g_hwnd, HWND_TOP, window_x, window_y, 0, 0, SWP_NOSIZE);

    UpdateWindow(g_hwnd);
    ShowWindow(g_hwnd, SW_SHOWDEFAULT);

    platform_window_width   = width;
    platform_window_height  = height;
    platform_window_is_open = true;

    return true;
}

void platform_poll_events() {
    clear_key_states();
    clear_mouse_button_states();
    
    mouse_cursor_x_delta = 0;
    mouse_cursor_y_delta = 0;

    mouse_scroll_wheel_x_delta = 0;
    mouse_scroll_wheel_y_delta = 0;
    
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void *platform_window_get_native() {
    return (void *)g_hwnd;
}

bool platform_window_was_resized() {
    bool result = was_resized;
    was_resized = false;
    return result;
}

// From https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
void platform_window_toggle_fullscreen() {
    DWORD style = GetWindowLong(g_hwnd, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(g_hwnd, &prev_wp) &&
            GetMonitorInfo(MonitorFromWindow(g_hwnd,
                                             MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(g_hwnd, GWL_STYLE,
                          style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(g_hwnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLong(g_hwnd, GWL_STYLE,
                      style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(g_hwnd, &prev_wp);
        SetWindowPos(g_hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }

    RECT rect;
    GetClientRect(g_hwnd, &rect);
    platform_window_width  = rect.right  - rect.left;
    platform_window_height = rect.bottom - rect.top;

    //was_resized = true;
}

void platform_init() {
    timeBeginPeriod(1);

    // Try Windows 10+ per-monitor V2 awareness
    HMODULE shcore = LoadLibraryA("Shcore.dll");
    if (shcore) {
        typedef HRESULT(WINAPI *SetProcessDpiAwarenessFn)(int);
        SetProcessDpiAwarenessFn SetProcessDpiAwarenessPtr =
            (SetProcessDpiAwarenessFn)GetProcAddress(shcore, "SetProcessDpiAwareness");
        if (SetProcessDpiAwarenessPtr) {
            // 2 - PROCESS_PER_MONITOR_DPI_AWARE
            SetProcessDpiAwarenessPtr(2);
            FreeLibrary(shcore);
            return;
        }
        FreeLibrary(shcore);
    }

    // Try Windows 8.1+ API (SetProcessDpiAwarenessContext)
    HMODULE user32 = LoadLibraryA("User32.dll");
    if (user32) {
        typedef BOOL(WINAPI *SetProcessDpiAwarenessContextFn)(HANDLE);
        SetProcessDpiAwarenessContextFn SetProcessDpiAwarenessContextPtr =
            (SetProcessDpiAwarenessContextFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (SetProcessDpiAwarenessContextPtr) {
            SetProcessDpiAwarenessContextPtr(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            FreeLibrary(user32);
            return;
        }
        FreeLibrary(user32);
    }

    // Fallback for Windows 7 and older
    HMODULE user32_old = LoadLibraryA("User32.dll");
    if (user32_old) {
        typedef BOOL(WINAPI *SetProcessDPIAwareFn)(void);
        SetProcessDPIAwareFn SetProcessDPIAwarePtr =
            (SetProcessDPIAwareFn)GetProcAddress(user32_old, "SetProcessDPIAware");
        if (SetProcessDPIAwarePtr) {
            SetProcessDPIAwarePtr();
        }
        FreeLibrary(user32_old);
    }
}

void platform_shutdown() {
    timeEndPeriod(1);
}

u64 platform_get_time_in_nanoseconds() {
    if (!global_perf_freq.QuadPart) {
        QueryPerformanceFrequency(&global_perf_freq);
        nanoseconds_per_tick = 1000000000 / global_perf_freq.QuadPart;
    }
    
    LARGE_INTEGER perf_counter;
    QueryPerformanceCounter(&perf_counter);
    
    return perf_counter.QuadPart * nanoseconds_per_tick;
}

//
// OpenGL
//

extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#define LoadGLFunc(func) func = reinterpret_cast <decltype(func)>(wglGetProcAddress(#func))

#define WGL_CONTEXT_DEBUG_BIT_ARB         0x00000001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002
#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB       0x2093
#define WGL_CONTEXT_FLAGS_ARB             0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB  0x20A9
#define WGL_SAMPLE_BUFFERS_ARB            0x2041
#define WGL_SAMPLES_ARB                   0x2042
#define WGL_NUMBER_PIXEL_FORMATS_ARB      0x2000
#define WGL_DRAW_TO_WINDOW_ARB            0x2001
#define WGL_DRAW_TO_BITMAP_ARB            0x2002
#define WGL_ACCELERATION_ARB              0x2003
#define WGL_NEED_PALETTE_ARB              0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB       0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB        0x2006
#define WGL_SWAP_METHOD_ARB               0x2007
#define WGL_NUMBER_OVERLAYS_ARB           0x2008
#define WGL_NUMBER_UNDERLAYS_ARB          0x2009
#define WGL_TRANSPARENT_ARB               0x200A
#define WGL_TRANSPARENT_RED_VALUE_ARB     0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB   0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB    0x2039
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB   0x203A
#define WGL_TRANSPARENT_INDEX_VALUE_ARB   0x203B
#define WGL_SHARE_DEPTH_ARB               0x200C
#define WGL_SHARE_STENCIL_ARB             0x200D
#define WGL_SHARE_ACCUM_ARB               0x200E
#define WGL_SUPPORT_GDI_ARB               0x200F
#define WGL_SUPPORT_OPENGL_ARB            0x2010
#define WGL_DOUBLE_BUFFER_ARB             0x2011
#define WGL_STEREO_ARB                    0x2012
#define WGL_PIXEL_TYPE_ARB                0x2013
#define WGL_COLOR_BITS_ARB                0x2014
#define WGL_RED_BITS_ARB                  0x2015
#define WGL_RED_SHIFT_ARB                 0x2016
#define WGL_GREEN_BITS_ARB                0x2017
#define WGL_GREEN_SHIFT_ARB               0x2018
#define WGL_BLUE_BITS_ARB                 0x2019
#define WGL_BLUE_SHIFT_ARB                0x201A
#define WGL_ALPHA_BITS_ARB                0x201B
#define WGL_ALPHA_SHIFT_ARB               0x201C
#define WGL_ACCUM_BITS_ARB                0x201D
#define WGL_ACCUM_RED_BITS_ARB            0x201E
#define WGL_ACCUM_GREEN_BITS_ARB          0x201F
#define WGL_ACCUM_BLUE_BITS_ARB           0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB          0x2021
#define WGL_DEPTH_BITS_ARB                0x2022
#define WGL_STENCIL_BITS_ARB              0x2023
#define WGL_AUX_BUFFERS_ARB               0x2024
#define WGL_NO_ACCELERATION_ARB           0x2025
#define WGL_GENERIC_ACCELERATION_ARB      0x2026
#define WGL_FULL_ACCELERATION_ARB         0x2027
#define WGL_SWAP_EXCHANGE_ARB             0x2028
#define WGL_SWAP_COPY_ARB                 0x2029
#define WGL_SWAP_UNDEFINED_ARB            0x202A
#define WGL_TYPE_RGBA_ARB                 0x202B
#define WGL_TYPE_COLORINDEX_ARB           0x202C

//static BOOL(*wglMakeContextCurrentARB)(HDC hDrawDC, HDC hReadDC, HGLRC hglrc);
static HGLRC(*wglCreateContextAttribsARB)(HDC hDC, HGLRC hShareContext, const int *attribList);
static BOOL(*wglChoosePixelFormatARB)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
static BOOL(*wglSwapIntervalEXT)(int interval);

void opengl_create_context(int version_major, int version_minor, bool core_profile, bool require_srgb) {
    // Get wgl functions
    {
        HWND dummy = CreateWindowExW(0, L"STATIC", L"DummyWindow", WS_OVERLAPPED,
                                 0, 0, 0, 0, NULL, NULL, NULL, NULL);
        HDC dc = GetDC(dummy);
        
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;

        int format = ChoosePixelFormat(dc, &pfd);
        if (!format) {
            // TODO: Log error
        }

        DescribePixelFormat(dc, format, sizeof(pfd), &pfd);
        SetPixelFormat(dc, format, &pfd);

        HGLRC rc = wglCreateContext(dc);
        wglMakeCurrent(dc, rc);

        LoadGLFunc(wglChoosePixelFormatARB);
        LoadGLFunc(wglCreateContextAttribsARB);
        LoadGLFunc(wglSwapIntervalEXT);

        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(rc);
        ReleaseDC(dummy, dc);
        DestroyWindow(dummy);
    }
    
    // Set pixel format for OpenGL context
    HDC dc = GetDC(g_hwnd);

    {
        int attrib[] = {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB,     24,
            WGL_DEPTH_BITS_ARB,     24,
            WGL_STENCIL_BITS_ARB,   8,

            // uncomment for sRGB framebuffer, from WGL_ARB_framebuffer_sRGB extension
            // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_framebuffer_sRGB.txt
            WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
        
            // uncomment for multisampeld framebuffer, from WGL_ARB_multisample extension
            // https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_multisample.txt
            //WGL_SAMPLE_BUFFERS_ARB, 1,
            //WGL_SAMPLES_ARB,        4, // 4x MSAA

            0,
        };

        if (!require_srgb) {
            attrib[14] = 0;
            attrib[15] = 0;
        }

        int format;
        UINT formats;
        wglChoosePixelFormatARB(dc, attrib, NULL, 1, &format, &formats);

        PIXELFORMATDESCRIPTOR desc = {};
        desc.nSize = sizeof(desc);
        DescribePixelFormat(dc, format, sizeof(desc), &desc);
        SetPixelFormat(dc, format, &desc);
    }

    // Create modern OpenGL context
    {
        int attrib[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, version_major,
            WGL_CONTEXT_MINOR_VERSION_ARB, version_minor,
            WGL_CONTEXT_PROFILE_MASK_ARB,  core_profile ? WGL_CONTEXT_CORE_PROFILE_BIT_ARB : WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#ifndef NDEBUG
            // ask for debug context for non "Release" builds
            // this is so we can enable debug callback
            //WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
            0,
        };

        HGLRC rc = wglCreateContextAttribsARB(dc, NULL, attrib);        
        wglMakeCurrent(dc, rc);
    }
}

bool opengl_set_vsync(bool vsync) {
    if (!wglSwapIntervalEXT) return false;

    wglSwapIntervalEXT(vsync ? 1 : 0);

    return true;
}

void opengl_swap_buffers() {
    HDC dc = GetDC(g_hwnd);
    SwapBuffers(dc);
}

void platform_show_and_unlock_cursor() {
    // Unlock cursor from any clipping region
    ClipCursor(NULL);

    // Make sure cursor is visible
    while (ShowCursor(TRUE) < 0) {
        // ShowCursor uses an internal counter; loop until visible
    }
}

void platform_hide_and_lock_cursor() {
    HWND hwnd = g_hwnd;

    // Hide cursor
    while (ShowCursor(FALSE) >= 0) {
        // loop until actually hidden
    }

    // Get client rect and convert to screen coords
    RECT rect;
    GetClientRect(hwnd, &rect);

    POINT ul = { rect.left, rect.top };
    POINT lr = { rect.right, rect.bottom };

    ClientToScreen(hwnd, &ul);
    ClientToScreen(hwnd, &lr);

    rect.left   = ul.x;
    rect.top    = ul.y;
    rect.right  = lr.x;
    rect.bottom = lr.y;

    // Lock cursor to window client area
    ClipCursor(&rect);

    POINT old_point;
    GetCursorPos(&old_point);
    
    int center_x = (rect.left + rect.right) / 2;
    int center_y = (rect.top + rect.bottom) / 2;
    SetCursorPos(center_x, center_y);

    mouse_cursor_x_delta = old_point.x - center_x;
    mouse_cursor_y_delta = center_y - old_point.y;
}

void platform_imgui_init() {
    ImGui_ImplWin32_Init(g_hwnd);
}

void platform_imgui_begin_frame() {
    ImGui_ImplWin32_NewFrame();
}

float platform_imgui_get_scale() {
    ImGui_ImplWin32_EnableDpiAwareness();
    float result = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTOPRIMARY));
    return result;
}

#endif

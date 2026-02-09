#include "corelib.h"

#ifdef PLATFORM_WINDOWS

#include <Windows.h>
#include <Windowsx.h>

struct Platform_Window_Windows : public Platform_Window {
    HWND hwnd;
    WINDOWPLACEMENT prev_wp;
    bool was_resized;
};

static Platform_Window_Windows created_windows[MAX_PLATFORM_WINDOWS];
static int num_created_windows;

#define WINDOW_CLASS_NAME L"GraphicsWin32WindowClass"
static bool window_class_initted;

static LARGE_INTEGER global_perf_freq;

static Platform_Window_Windows *add_window() {
    Assert(num_created_windows < MAX_PLATFORM_WINDOWS);
    
    Platform_Window_Windows *result = &created_windows[num_created_windows++];

    result->prev_wp.length = sizeof(WINDOWPLACEMENT);
    result->was_resized    = false;

    return result;
}

static Platform_Window_Windows *pop_last_window() {
    if (num_created_windows <= 0) return NULL;

    Platform_Window_Windows *result = &created_windows[num_created_windows--];
    return result;
}

static LRESULT CALLBACK platform_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Platform_Window_Windows *window = (Platform_Window_Windows *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCTW *cs = (CREATESTRUCTW *)lparam;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        } break;
        
        case WM_CLOSE:
        case WM_DESTROY: {
            Assert(window);
            window->is_open = false;
        } break;

        case WM_SIZE: {
            Assert(window);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            window->width  = rect.right  - rect.left;
            window->height = rect.bottom - rect.top;

            window->was_resized = true;
        } break;

        case WM_SYSCHAR: {
            // Prevent windows from beeping when pressing an alt-key combo.
        } break;

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            Assert(window);

            Key_Code key_code = (Key_Code)wparam;
            bool is_down = (msg == WM_KEYDOWN) || (msg == WM_SYSKEYDOWN);

            Keyboard *keyboard = &window->keyboard;
            set_key_state(keyboard, key_code, is_down);
        } break;

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP: {
            Assert(window);

            bool is_down = msg == WM_LBUTTONDOWN;

            Mouse *mouse = &window->mouse;
            set_mouse_button_state(mouse, MOUSE_BUTTON_LEFT, is_down);
        } break;

        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP: {
            Assert(window);

            bool is_down = msg == WM_RBUTTONDOWN;

            Mouse *mouse = &window->mouse;
            set_mouse_button_state(mouse, MOUSE_BUTTON_RIGHT, is_down);
        } break;

        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP: {
            Assert(window);

            bool is_down = msg == WM_MBUTTONDOWN;

            Mouse *mouse = &window->mouse;
            set_mouse_button_state(mouse, MOUSE_BUTTON_MIDDLE, is_down);
        } break;

        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP: {
            Assert(window);
            
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
            
            Mouse *mouse = &window->mouse;
            set_mouse_button_state(mouse, mouse_button, is_down);
            
            return TRUE;
        } break;

        case WM_MOUSEMOVE: {
            Assert(window);

            int x_pos = GET_X_LPARAM(lparam);
            int y_pos = GET_Y_LPARAM(lparam);

            Mouse *mouse = &window->mouse;

            mouse->cursor_x_delta = x_pos - mouse->cursor_x;
            mouse->cursor_y_delta = mouse->cursor_y - y_pos;

            mouse->cursor_x = x_pos;
            mouse->cursor_y = window->height - y_pos;
        } break;

        case WM_MOUSEWHEEL: {
            Assert(window);

            int total_wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);

            Mouse *mouse = &window->mouse;
            mouse->scroll_wheel_y_delta = total_wheel_delta / WHEEL_DELTA; // 120
        } break;

        case WM_MOUSEHWHEEL: {
            Assert(window);

            int total_wheel_delta = GET_WHEEL_DELTA_WPARAM(wparam);

            Mouse *mouse = &window->mouse;
            mouse->scroll_wheel_x_delta = total_wheel_delta / WHEEL_DELTA; // 120
        } break;
            
        default: {
            return DefWindowProcW(hwnd, msg, wparam, lparam);
        } break;
    }

    return 0;
}

static void init_window_class() {
    if (window_class_initted) return;

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
        return;
    }
    
    window_class_initted = true;
}

Platform_Window *platform_window_create(int width, int height, String title) {
    if (!window_class_initted) {
        init_window_class();
        if (!window_class_initted) return NULL;
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

    Platform_Window_Windows *result = add_window();

    DWORD window_style = WS_OVERLAPPEDWINDOW;
    
    RECT  window_rect  = {0, 0, width, height};
    AdjustWindowRect(&window_rect, window_style, FALSE);

    int window_width   = window_rect.right  - window_rect.left;
    int window_height  = window_rect.bottom - window_rect.top;

    wchar_t wide_title[4096] = {};
    MultiByteToWideChar(CP_UTF8, 0, title.data, title.length, wide_title, ArrayCount(wide_title));
    result->hwnd = CreateWindowExW(0, WINDOW_CLASS_NAME, wide_title, window_style,
                                   CW_USEDEFAULT, CW_USEDEFAULT,
                                   window_width, window_height,
                                   NULL, NULL, GetModuleHandleW(NULL), result);
    if (result->hwnd == NULL) {
        logprintf("Failed to create window!\n");
        pop_last_window();
        return NULL;
    }

    MONITORINFOEXW mi = { sizeof(mi) };
    GetMonitorInfoW(MonitorFromWindow(result->hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);

    int monitor_width_without_taskbar  = mi.rcWork.right  - mi.rcWork.left;
    int monitor_height_without_taskbar = mi.rcWork.bottom - mi.rcWork.top;

    int window_x = mi.rcWork.left + ((monitor_width_without_taskbar  - window_width)  / 2);
    int window_y = mi.rcWork.top  + ((monitor_height_without_taskbar - window_height) / 2);

    SetWindowPos(result->hwnd, HWND_TOP, window_x, window_y, 0, 0, SWP_NOSIZE);

    UpdateWindow(result->hwnd);
    ShowWindow(result->hwnd, SW_SHOWDEFAULT);

    result->width   = width;
    result->height  = height;
    result->is_open = true;

    return result;
}

void platform_poll_events() {
    for (int i = 0; i < num_created_windows; i++) {
        Platform_Window *window = &created_windows[i];
        clear_key_states(&window->keyboard);
        clear_mouse_button_states(&window->mouse);

        window->mouse.cursor_x_delta = 0;
        window->mouse.cursor_y_delta = 0;

        window->mouse.scroll_wheel_x_delta = 0;
        window->mouse.scroll_wheel_y_delta = 0;
    }
    
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

void *platform_window_get_native(Platform_Window *_window) {
    Assert(_window);
    Platform_Window_Windows *window = (Platform_Window_Windows *)_window;
    return (void *)window->hwnd;
}

bool platform_window_was_resized(Platform_Window *_window) {
    Assert(_window);
    Platform_Window_Windows *window = (Platform_Window_Windows *)_window;

    bool result = window->was_resized;
    window->was_resized = false;
    return result;
}

// From https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
void platform_window_toggle_fullscreen(Platform_Window *_window) {
    Assert(_window);
    Platform_Window_Windows *window = (Platform_Window_Windows *)_window;
    
    DWORD style = GetWindowLong(window->hwnd, GWL_STYLE);
    if (style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO mi = { sizeof(mi) };
        if (GetWindowPlacement(window->hwnd, &window->prev_wp) &&
            GetMonitorInfo(MonitorFromWindow(window->hwnd,
                                             MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(window->hwnd, GWL_STYLE,
                          style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(window->hwnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLong(window->hwnd, GWL_STYLE,
                      style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(window->hwnd, &window->prev_wp);
        SetWindowPos(window->hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
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
    }

    LARGE_INTEGER perf_counter;
    QueryPerformanceCounter(&perf_counter);

    return (u64)((perf_counter.QuadPart + 1000000000ULL) / global_perf_freq.QuadPart);
}

#endif

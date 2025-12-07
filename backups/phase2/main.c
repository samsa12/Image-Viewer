/*
 * Pure C Image Viewer
 * Main Entry Point
 *
 * Controls:
 *   O              - Open file dialog
 *   Left/Right     - Previous/Next image
 *   F11 / F        - Toggle fullscreen
 *   Escape         - Exit fullscreen or quit
 *   0              - Fit to window
 *   1              - Actual size (100%)
 *   +/-            - Zoom in/out
 *   Mouse Wheel    - Zoom at cursor position
 *   Left Drag      - Pan image
 *   Drag & Drop    - Open dropped image
 */

#include "file_browser.h"
#include "image_loader.h"
#include "renderer.h"
#include <shellapi.h>
#include <stdio.h>
#include <windows.h>
#include <windowsx.h>


// Window class name
#define WINDOW_CLASS "PureCImageViewer"
#define WINDOW_TITLE "Image Viewer"

// Global state
static ImageData g_image = {0};
static Renderer g_renderer = {0};
static FileBrowser g_browser = {0};
static BOOL g_fullscreen = FALSE;
static WINDOWPLACEMENT g_prevPlacement = {sizeof(g_prevPlacement)};

// Panning state
static BOOL g_isPanning = FALSE;
static int g_panStartX = 0;
static int g_panStartY = 0;
static int g_offsetStartX = 0;
static int g_offsetStartY = 0;

// Function prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void LoadImageFile(HWND hwnd, const char *filepath);
void UpdateWindowTitle(HWND hwnd);
void ToggleFullscreen(HWND hwnd);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  (void)hPrevInstance;

  // Initialize components
  Renderer_Init(&g_renderer);
  FileBrowser_Init(&g_browser);

  // Register window class
  WNDCLASSEXA wc = {0};
  wc.cbSize = sizeof(WNDCLASSEXA);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  wc.lpszClassName = WINDOW_CLASS;
  wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

  if (!RegisterClassExA(&wc)) {
    MessageBoxA(NULL, "Failed to register window class", "Error", MB_ICONERROR);
    return 1;
  }

  // Create window
  HWND hwnd = CreateWindowExA(WS_EX_ACCEPTFILES, // Accept drag & drop
                              WINDOW_CLASS, WINDOW_TITLE, WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, NULL,
                              NULL, hInstance, NULL);

  if (!hwnd) {
    MessageBoxA(NULL, "Failed to create window", "Error", MB_ICONERROR);
    return 1;
  }

  // Check if file was passed as command line argument
  if (lpCmdLine && lpCmdLine[0] != '\0') {
    // Remove quotes if present
    char filepath[MAX_PATH];
    if (lpCmdLine[0] == '"') {
      strncpy(filepath, lpCmdLine + 1, MAX_PATH - 1);
      char *endQuote = strchr(filepath, '"');
      if (endQuote)
        *endQuote = '\0';
    } else {
      strncpy(filepath, lpCmdLine, MAX_PATH - 1);
    }
    LoadImageFile(hwnd, filepath);
  }

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  // Message loop
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  // Cleanup
  ImageLoader_Free(&g_image);
  Renderer_Cleanup(&g_renderer);

  return (int)msg.wParam;
}

void LoadImageFile(HWND hwnd, const char *filepath) {
  // Free previous image
  ImageLoader_Free(&g_image);
  Renderer_Cleanup(&g_renderer);

  // Load new image
  if (ImageLoader_Load(filepath, &g_image)) {
    // Load directory for navigation
    FileBrowser_LoadDirectory(&g_browser, filepath);

    // Create bitmap for rendering
    HDC hdc = GetDC(hwnd);
    Renderer_CreateBitmap(&g_renderer, hdc, &g_image);

    // Fit to window
    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    Renderer_FitToWindow(&g_renderer, &clientRect, &g_image);

    ReleaseDC(hwnd, hdc);

    UpdateWindowTitle(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
  } else {
    char msg[512];
    snprintf(msg, sizeof(msg), "Failed to load image:\n%s\n\nError: %s",
             filepath, ImageLoader_GetError());
    MessageBoxA(hwnd, msg, "Error", MB_ICONERROR);
  }
}

void UpdateWindowTitle(HWND hwnd) {
  if (g_image.pixels) {
    char title[512];
    const char *filename = strrchr(g_image.filepath, '\\');
    if (!filename)
      filename = strrchr(g_image.filepath, '/');
    filename = filename ? filename + 1 : g_image.filepath;

    int zoomPercent = (int)(g_renderer.scale * 100.0f);
    snprintf(title, sizeof(title), "%s - %dx%d - %d%% - [%d/%d] - Image Viewer",
             filename, g_image.width, g_image.height, zoomPercent,
             g_browser.currentIndex + 1, g_browser.fileCount);
    SetWindowTextA(hwnd, title);
  } else {
    SetWindowTextA(hwnd, WINDOW_TITLE);
  }
}

void ToggleFullscreen(HWND hwnd) {
  DWORD style = GetWindowLong(hwnd, GWL_STYLE);

  if (!g_fullscreen) {
    // Save current window placement
    GetWindowPlacement(hwnd, &g_prevPlacement);

    // Remove window decorations
    SetWindowLong(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);

    // Get monitor info
    MONITORINFO mi = {sizeof(mi)};
    GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);

    // Set fullscreen size
    SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left,
                 mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

    g_fullscreen = TRUE;
  } else {
    // Restore window style
    SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
    SetWindowPlacement(hwnd, &g_prevPlacement);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                     SWP_FRAMECHANGED);

    g_fullscreen = FALSE;
  }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);

    Renderer_Paint(&g_renderer, hdc, &clientRect, &g_image);

    EndPaint(hwnd, &ps);
    return 0;
  }

  case WM_SIZE: {
    if (g_image.pixels && g_renderer.fitToWindow) {
      RECT clientRect;
      GetClientRect(hwnd, &clientRect);
      Renderer_FitToWindow(&g_renderer, &clientRect, &g_image);
      InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
  }

  case WM_KEYDOWN: {
    switch (wParam) {
    case 'O': // Open file
      if (FileBrowser_OpenDialog(&g_browser, hwnd)) {
        LoadImageFile(hwnd, FileBrowser_GetCurrent(&g_browser));
      }
      break;

    case VK_LEFT: // Previous image
    case VK_UP: {
      const char *prev = FileBrowser_Previous(&g_browser);
      if (prev)
        LoadImageFile(hwnd, prev);
      break;
    }

    case VK_RIGHT: // Next image
    case VK_DOWN: {
      const char *next = FileBrowser_Next(&g_browser);
      if (next)
        LoadImageFile(hwnd, next);
      break;
    }

    case VK_F11:
    case 'F': // Toggle fullscreen
      ToggleFullscreen(hwnd);
      break;

    case VK_ESCAPE: // Exit fullscreen or quit
      if (g_fullscreen) {
        ToggleFullscreen(hwnd);
      } else {
        PostQuitMessage(0);
      }
      break;

    case '0': { // Fit to window
      RECT clientRect;
      GetClientRect(hwnd, &clientRect);
      Renderer_FitToWindow(&g_renderer, &clientRect, &g_image);
      UpdateWindowTitle(hwnd);
      InvalidateRect(hwnd, NULL, TRUE);
      break;
    }

    case '1': // Actual size
      Renderer_SetScale(&g_renderer, 1.0f);
      {
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        Renderer_CenterImage(&g_renderer, &clientRect, &g_image);
      }
      UpdateWindowTitle(hwnd);
      InvalidateRect(hwnd, NULL, TRUE);
      break;

    case VK_OEM_PLUS:
    case VK_ADD: // Zoom in
      Renderer_SetScale(&g_renderer, g_renderer.scale * 1.25f);
      {
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        Renderer_CenterImage(&g_renderer, &clientRect, &g_image);
      }
      UpdateWindowTitle(hwnd);
      InvalidateRect(hwnd, NULL, TRUE);
      break;

    case VK_OEM_MINUS:
    case VK_SUBTRACT: // Zoom out
      Renderer_SetScale(&g_renderer, g_renderer.scale * 0.8f);
      {
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        Renderer_CenterImage(&g_renderer, &clientRect, &g_image);
      }
      UpdateWindowTitle(hwnd);
      InvalidateRect(hwnd, NULL, TRUE);
      break;
    }
    return 0;
  }

  case WM_MOUSEWHEEL: {
    // Zoom at cursor position
    if (!g_image.pixels)
      return 0;

    // Get cursor position in client coordinates
    POINT pt;
    pt.x = GET_X_LPARAM(lParam);
    pt.y = GET_Y_LPARAM(lParam);
    ScreenToClient(hwnd, &pt);

    int delta = GET_WHEEL_DELTA_WPARAM(wParam);
    float oldScale = g_renderer.scale;
    float newScale = (delta > 0) ? oldScale * 1.15f : oldScale * 0.87f;

    // Clamp scale
    if (newScale < 0.05f)
      newScale = 0.05f;
    if (newScale > 50.0f)
      newScale = 50.0f;

    // Zoom toward cursor position
    float zoomRatio = newScale / oldScale;
    g_renderer.offsetX = (int)(pt.x - (pt.x - g_renderer.offsetX) * zoomRatio);
    g_renderer.offsetY = (int)(pt.y - (pt.y - g_renderer.offsetY) * zoomRatio);
    g_renderer.scale = newScale;
    g_renderer.fitToWindow = FALSE;

    UpdateWindowTitle(hwnd);
    InvalidateRect(hwnd, NULL, TRUE);
    return 0;
  }

  case WM_LBUTTONDOWN: {
    // Start panning
    if (g_image.pixels) {
      g_isPanning = TRUE;
      g_panStartX = GET_X_LPARAM(lParam);
      g_panStartY = GET_Y_LPARAM(lParam);
      g_offsetStartX = g_renderer.offsetX;
      g_offsetStartY = g_renderer.offsetY;
      SetCapture(hwnd);
      SetCursor(LoadCursor(NULL, IDC_SIZEALL));
    }
    return 0;
  }

  case WM_LBUTTONUP: {
    // Stop panning
    if (g_isPanning) {
      g_isPanning = FALSE;
      ReleaseCapture();
      SetCursor(LoadCursor(NULL, IDC_ARROW));
    }
    return 0;
  }

  case WM_MOUSEMOVE: {
    // Pan image while dragging
    if (g_isPanning && g_image.pixels) {
      int dx = GET_X_LPARAM(lParam) - g_panStartX;
      int dy = GET_Y_LPARAM(lParam) - g_panStartY;
      g_renderer.offsetX = g_offsetStartX + dx;
      g_renderer.offsetY = g_offsetStartY + dy;
      g_renderer.fitToWindow = FALSE;
      InvalidateRect(hwnd, NULL, TRUE);
    }
    return 0;
  }

  case WM_DROPFILES: {
    HDROP hDrop = (HDROP)wParam;
    char filepath[MAX_PATH];

    if (DragQueryFileA(hDrop, 0, filepath, MAX_PATH)) {
      LoadImageFile(hwnd, filepath);
    }

    DragFinish(hDrop);
    return 0;
  }

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

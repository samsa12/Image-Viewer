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
 *   S              - Start/stop slideshow
 *   I              - Toggle info panel
 *   T              - Toggle dark/light theme
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

// Timer IDs
#define TIMER_SLIDESHOW 1

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

// Slideshow state
static BOOL g_slideshowActive = FALSE;
static int g_slideshowInterval = 3000; // 3 seconds default

// UI state
static BOOL g_showInfo = FALSE;
static BOOL g_darkTheme = TRUE;

// Theme colors
static COLORREF g_bgColor = RGB(30, 30, 30);
static COLORREF g_textColor = RGB(200, 200, 200);
static COLORREF g_panelBgColor = RGB(40, 40, 40);

// Function prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void LoadImageFile(HWND hwnd, const char *filepath);
void UpdateWindowTitle(HWND hwnd);
void ToggleFullscreen(HWND hwnd);
void ToggleSlideshow(HWND hwnd);
void DrawInfoPanel(HDC hdc, RECT *clientRect);
void ToggleTheme(void);

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

    if (g_slideshowActive) {
      snprintf(title, sizeof(title),
               "%s - %dx%d - %d%% - [%d/%d] - SLIDESHOW - Image Viewer",
               filename, g_image.width, g_image.height, zoomPercent,
               g_browser.currentIndex + 1, g_browser.fileCount);
    } else {
      snprintf(title, sizeof(title),
               "%s - %dx%d - %d%% - [%d/%d] - Image Viewer", filename,
               g_image.width, g_image.height, zoomPercent,
               g_browser.currentIndex + 1, g_browser.fileCount);
    }
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

void ToggleSlideshow(HWND hwnd) {
  if (g_slideshowActive) {
    // Stop slideshow
    KillTimer(hwnd, TIMER_SLIDESHOW);
    g_slideshowActive = FALSE;
  } else {
    // Start slideshow
    if (g_browser.fileCount > 1) {
      SetTimer(hwnd, TIMER_SLIDESHOW, g_slideshowInterval, NULL);
      g_slideshowActive = TRUE;
    }
  }
  UpdateWindowTitle(hwnd);
}

void ToggleTheme(void) {
  g_darkTheme = !g_darkTheme;
  if (g_darkTheme) {
    g_bgColor = RGB(30, 30, 30);
    g_textColor = RGB(200, 200, 200);
    g_panelBgColor = RGB(40, 40, 40);
  } else {
    g_bgColor = RGB(240, 240, 240);
    g_textColor = RGB(30, 30, 30);
    g_panelBgColor = RGB(255, 255, 255);
  }
}

void DrawInfoPanel(HDC hdc, RECT *clientRect) {
  if (!g_showInfo || !g_image.pixels)
    return;

  // Panel dimensions
  int panelWidth = 280;
  int panelHeight = 180;
  int margin = 15;
  int padding = 12;

  RECT panelRect = {clientRect->right - panelWidth - margin, margin,
                    clientRect->right - margin, margin + panelHeight};

  // Draw semi-transparent panel background
  HBRUSH panelBrush = CreateSolidBrush(g_panelBgColor);
  FillRect(hdc, &panelRect, panelBrush);
  DeleteObject(panelBrush);

  // Draw border
  HPEN borderPen = CreatePen(
      PS_SOLID, 1, g_darkTheme ? RGB(80, 80, 80) : RGB(180, 180, 180));
  HPEN oldPen = SelectObject(hdc, borderPen);
  Rectangle(hdc, panelRect.left, panelRect.top, panelRect.right,
            panelRect.bottom);
  SelectObject(hdc, oldPen);
  DeleteObject(borderPen);

  // Setup text
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, g_textColor);

  HFONT font =
      CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, "Segoe UI");
  HFONT oldFont = SelectObject(hdc, font);

  // Get filename
  const char *filename = strrchr(g_image.filepath, '\\');
  if (!filename)
    filename = strrchr(g_image.filepath, '/');
  filename = filename ? filename + 1 : g_image.filepath;

  // Calculate file size
  WIN32_FILE_ATTRIBUTE_DATA fileInfo;
  DWORD fileSize = 0;
  if (GetFileAttributesExA(g_image.filepath, GetFileExInfoStandard,
                           &fileInfo)) {
    fileSize = fileInfo.nFileSizeLow;
  }

  char sizeStr[32];
  if (fileSize >= 1024 * 1024) {
    snprintf(sizeStr, sizeof(sizeStr), "%.2f MB", fileSize / (1024.0 * 1024.0));
  } else if (fileSize >= 1024) {
    snprintf(sizeStr, sizeof(sizeStr), "%.1f KB", fileSize / 1024.0);
  } else {
    snprintf(sizeStr, sizeof(sizeStr), "%lu bytes", fileSize);
  }

  // Draw info lines
  int y = panelRect.top + padding;
  int lineHeight = 22;
  int labelX = panelRect.left + padding;

  // Title
  HFONT boldFont =
      CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, "Segoe UI");
  SelectObject(hdc, boldFont);
  TextOutA(hdc, labelX, y, "Image Information", 17);
  y += lineHeight + 5;

  SelectObject(hdc, font);

  char buffer[256];

  // Filename (truncated if too long)
  char shortName[30];
  if (strlen(filename) > 28) {
    strncpy(shortName, filename, 25);
    strcpy(shortName + 25, "...");
  } else {
    strcpy(shortName, filename);
  }
  snprintf(buffer, sizeof(buffer), "Name: %s", shortName);
  TextOutA(hdc, labelX, y, buffer, (int)strlen(buffer));
  y += lineHeight;

  // Dimensions
  snprintf(buffer, sizeof(buffer), "Size: %d x %d pixels", g_image.width,
           g_image.height);
  TextOutA(hdc, labelX, y, buffer, (int)strlen(buffer));
  y += lineHeight;

  // File size
  snprintf(buffer, sizeof(buffer), "File: %s", sizeStr);
  TextOutA(hdc, labelX, y, buffer, (int)strlen(buffer));
  y += lineHeight;

  // Zoom level
  snprintf(buffer, sizeof(buffer), "Zoom: %d%%",
           (int)(g_renderer.scale * 100.0f));
  TextOutA(hdc, labelX, y, buffer, (int)strlen(buffer));
  y += lineHeight;

  // Position in folder
  snprintf(buffer, sizeof(buffer), "Position: %d of %d",
           g_browser.currentIndex + 1, g_browser.fileCount);
  TextOutA(hdc, labelX, y, buffer, (int)strlen(buffer));

  SelectObject(hdc, oldFont);
  DeleteObject(font);
  DeleteObject(boldFont);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);

    // Fill background with theme color
    HBRUSH bgBrush = CreateSolidBrush(g_bgColor);
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Draw image
    if (g_image.pixels && g_renderer.hMemDC) {
      int scaledWidth = (int)(g_image.width * g_renderer.scale);
      int scaledHeight = (int)(g_image.height * g_renderer.scale);

      SetStretchBltMode(hdc, HALFTONE);
      SetBrushOrgEx(hdc, 0, 0, NULL);

      StretchBlt(hdc, g_renderer.offsetX, g_renderer.offsetY, scaledWidth,
                 scaledHeight, g_renderer.hMemDC, 0, 0, g_image.width,
                 g_image.height, SRCCOPY);
    } else {
      // No image - draw help text
      SetBkMode(hdc, TRANSPARENT);
      SetTextColor(hdc, g_textColor);

      const char *msg = "Drag & drop an image or press O to open";
      RECT textRect = clientRect;
      DrawTextA(hdc, msg, -1, &textRect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Draw info panel overlay
    DrawInfoPanel(hdc, &clientRect);

    // Draw slideshow indicator
    if (g_slideshowActive) {
      SetBkMode(hdc, TRANSPARENT);
      SetTextColor(hdc, RGB(0, 200, 100));
      HFONT font =
          CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
      HFONT oldFont = SelectObject(hdc, font);
      TextOutA(hdc, 15, 15, "▶ SLIDESHOW", 12);
      SelectObject(hdc, oldFont);
      DeleteObject(font);
    }

    EndPaint(hwnd, &ps);
    return 0;
  }

  case WM_TIMER: {
    if (wParam == TIMER_SLIDESHOW && g_slideshowActive) {
      // Advance to next image
      const char *next = FileBrowser_Next(&g_browser);
      if (next) {
        LoadImageFile(hwnd, next);
      }
    }
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
    case VK_DOWN:
    case VK_SPACE: { // Space also advances
      const char *next = FileBrowser_Next(&g_browser);
      if (next)
        LoadImageFile(hwnd, next);
      break;
    }

    case VK_F11:
    case 'F': // Toggle fullscreen
      ToggleFullscreen(hwnd);
      break;

    case 'S': // Toggle slideshow
      ToggleSlideshow(hwnd);
      InvalidateRect(hwnd, NULL, TRUE);
      break;

    case 'I': // Toggle info panel
      g_showInfo = !g_showInfo;
      InvalidateRect(hwnd, NULL, TRUE);
      break;

    case 'T': // Toggle theme
      ToggleTheme();
      InvalidateRect(hwnd, NULL, TRUE);
      break;

    case VK_ESCAPE: // Exit fullscreen or quit
      if (g_slideshowActive) {
        ToggleSlideshow(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
      } else if (g_fullscreen) {
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
    if (g_slideshowActive) {
      KillTimer(hwnd, TIMER_SLIDESHOW);
    }
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

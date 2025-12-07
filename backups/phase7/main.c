/*
 * Pure C Image Viewer
 * Main Entry Point
 *
 * Controls:
 *   O              - Open file dialog
 *   Left/Right     - Previous/Next image
 *   F11 / F        - Toggle fullscreen
 *   0 / 1          - Fit to window / Actual size
 *   +/-            - Zoom in/out (or slideshow speed)
 *   Mouse Wheel    - Zoom at cursor
 *   Left Drag      - Pan image
 *   S              - Start/stop slideshow
 *   Ctrl+S         - Save edited image (BMP)
 *   I              - Toggle info panel
 *   T              - Toggle dark/light theme
 *   R / L          - Rotate right/left 90 degrees
 *   H / V          - Flip horizontal/vertical
 *   Ctrl+C         - Copy to clipboard
 *   Delete         - Delete to recycle bin
 *   E              - Open in Explorer
 *   W              - Set as wallpaper
 *   P              - Print image
 *   Escape         - Exit fullscreen/slideshow or quit
 */

#include "file_browser.h"
#include "image_loader.h"
#include "renderer.h"
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <windows.h>
#include <windowsx.h>

// Window class name
#define WINDOW_CLASS "PureCImageViewer"
#define WINDOW_TITLE "Image Viewer"

// Timer IDs
#define TIMER_SLIDESHOW 1
#define TIMER_ANIMATION 2

// Slideshow speed limits (milliseconds)
#define SLIDESHOW_MIN_INTERVAL 500
#define SLIDESHOW_MAX_INTERVAL 30000

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
static BOOL g_showThumbnails = FALSE;

// Thumbnail strip
#define THUMB_SIZE 80
#define THUMB_PADDING 5
#define THUMB_STRIP_HEIGHT (THUMB_SIZE + THUMB_PADDING * 2)

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
void CopyImageToClipboard(HWND hwnd);
void DeleteCurrentImage(HWND hwnd);
void OpenInExplorer(void);
void SetAsWallpaper(void);
void PrintImage(HWND hwnd);
void SaveImage(HWND hwnd);
void DrawThumbnailStrip(HWND hwnd, HDC hdc, RECT *clientRect);

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
  // Stop any existing animation
  KillTimer(hwnd, TIMER_ANIMATION);

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

    // Start animation timer for animated GIFs
    if (g_image.isAnimated) {
      int delay = ImageLoader_GetFrameDelay(&g_image);
      SetTimer(hwnd, TIMER_ANIMATION, delay, NULL);
    }

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
      float seconds = g_slideshowInterval / 1000.0f;
      snprintf(title, sizeof(title),
               "%s - %dx%d - [%d/%d] - SLIDESHOW (%.1fs) - Image Viewer",
               filename, g_image.width, g_image.height,
               g_browser.currentIndex + 1, g_browser.fileCount, seconds);
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

void CopyImageToClipboard(HWND hwnd) {
  if (!g_image.pixels)
    return;

  // Create a DIB (Device Independent Bitmap) for clipboard
  int width = g_image.width;
  int height = g_image.height;

  BITMAPINFOHEADER bi = {0};
  bi.biSize = sizeof(BITMAPINFOHEADER);
  bi.biWidth = width;
  bi.biHeight = height; // Positive = bottom-up
  bi.biPlanes = 1;
  bi.biBitCount = 24; // 24-bit for clipboard compatibility
  bi.biCompression = BI_RGB;

  int rowSize = ((width * 3 + 3) / 4) * 4; // Align to 4 bytes
  int dataSize = rowSize * height;

  HGLOBAL hMem =
      GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + dataSize);
  if (!hMem)
    return;

  BYTE *pData = (BYTE *)GlobalLock(hMem);
  memcpy(pData, &bi, sizeof(BITMAPINFOHEADER));

  BYTE *pPixels = pData + sizeof(BITMAPINFOHEADER);

  // Convert RGBA to BGR (bottom-up for BMP)
  for (int y = 0; y < height; y++) {
    BYTE *srcRow = g_image.pixels + (height - 1 - y) * width * 4;
    BYTE *dstRow = pPixels + y * rowSize;
    for (int x = 0; x < width; x++) {
      dstRow[x * 3 + 0] = srcRow[x * 4 + 2]; // B
      dstRow[x * 3 + 1] = srcRow[x * 4 + 1]; // G
      dstRow[x * 3 + 2] = srcRow[x * 4 + 0]; // R
    }
  }

  GlobalUnlock(hMem);

  if (OpenClipboard(hwnd)) {
    EmptyClipboard();
    SetClipboardData(CF_DIB, hMem);
    CloseClipboard();
  } else {
    GlobalFree(hMem);
  }
}

void DeleteCurrentImage(HWND hwnd) {
  if (!g_image.pixels || g_browser.fileCount == 0)
    return;

  char filepath[MAX_PATH];
  strncpy(filepath, g_image.filepath, MAX_PATH - 1);
  filepath[MAX_PATH - 1] = '\0';

  // Use SHFileOperation to move to recycle bin
  SHFILEOPSTRUCTA fileOp = {0};
  char doubleNullPath[MAX_PATH + 2] = {0};
  strncpy(doubleNullPath, filepath, MAX_PATH);

  fileOp.hwnd = hwnd;
  fileOp.wFunc = FO_DELETE;
  fileOp.pFrom = doubleNullPath;
  fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;

  // Get next file before deleting
  const char *nextFile = NULL;
  if (g_browser.fileCount > 1) {
    int nextIdx = (g_browser.currentIndex + 1) % g_browser.fileCount;
    nextFile = g_browser.files[nextIdx];
  }

  // Free current image first
  ImageLoader_Free(&g_image);
  Renderer_Cleanup(&g_renderer);

  if (SHFileOperationA(&fileOp) == 0) {
    // Success - reload directory and show next image
    if (nextFile) {
      char nextPath[MAX_PATH];
      strncpy(nextPath, nextFile, MAX_PATH - 1);
      LoadImageFile(hwnd, nextPath);
    } else {
      InvalidateRect(hwnd, NULL, TRUE);
      UpdateWindowTitle(hwnd);
    }
  } else {
    MessageBoxA(hwnd, "Failed to delete file", "Error", MB_ICONERROR);
  }
}

void OpenInExplorer(void) {
  if (!g_image.pixels)
    return;

  // Select file in explorer
  char cmd[MAX_PATH + 32];
  snprintf(cmd, sizeof(cmd), "/select,\"%s\"", g_image.filepath);
  ShellExecuteA(NULL, "open", "explorer.exe", cmd, NULL, SW_SHOW);
}

void SetAsWallpaper(void) {
  if (!g_image.pixels)
    return;

  // Windows requires BMP format for wallpaper via SystemParametersInfo
  // We'll set it using the file path (works for JPG/PNG on modern Windows)
  if (SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, (void *)g_image.filepath,
                            SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) {
    // Success
  } else {
    // Fallback message
    MessageBoxA(NULL,
                "Could not set as wallpaper.\nTry using a JPG or BMP file.",
                "Wallpaper", MB_ICONINFORMATION);
  }
}

void PrintImage(HWND hwnd) {
  if (!g_image.pixels)
    return;

  PRINTDLGA pd = {0};
  pd.lStructSize = sizeof(pd);
  pd.hwndOwner = hwnd;
  pd.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION;
  pd.nCopies = 1;

  if (!PrintDlgA(&pd))
    return;

  HDC printerDC = pd.hDC;

  DOCINFOA di = {0};
  di.cbSize = sizeof(di);
  di.lpszDocName = "Image Viewer Print";

  if (StartDocA(printerDC, &di) > 0) {
    StartPage(printerDC);

    // Get printer page size
    int pageWidth = GetDeviceCaps(printerDC, HORZRES);
    int pageHeight = GetDeviceCaps(printerDC, VERTRES);

    // Calculate scaled size maintaining aspect ratio
    float imgAspect = (float)g_image.width / g_image.height;
    float pageAspect = (float)pageWidth / pageHeight;

    int printWidth, printHeight;
    if (imgAspect > pageAspect) {
      printWidth = pageWidth;
      printHeight = (int)(pageWidth / imgAspect);
    } else {
      printHeight = pageHeight;
      printWidth = (int)(pageHeight * imgAspect);
    }

    // Center on page
    int x = (pageWidth - printWidth) / 2;
    int y = (pageHeight - printHeight) / 2;

    // Create DIB for printing
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_image.width;
    bmi.bmiHeader.biHeight = -g_image.height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // Convert RGBA to BGRA for Windows
    BYTE *pixels = (BYTE *)malloc(g_image.width * g_image.height * 4);
    for (int i = 0; i < g_image.width * g_image.height; i++) {
      pixels[i * 4 + 0] = g_image.pixels[i * 4 + 2]; // B
      pixels[i * 4 + 1] = g_image.pixels[i * 4 + 1]; // G
      pixels[i * 4 + 2] = g_image.pixels[i * 4 + 0]; // R
      pixels[i * 4 + 3] = g_image.pixels[i * 4 + 3]; // A
    }

    SetStretchBltMode(printerDC, HALFTONE);
    StretchDIBits(printerDC, x, y, printWidth, printHeight, 0, 0, g_image.width,
                  g_image.height, pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);

    free(pixels);

    EndPage(printerDC);
    EndDoc(printerDC);
  }

  DeleteDC(printerDC);
}

void SaveImage(HWND hwnd) {
  if (!g_image.pixels)
    return;

  char filename[MAX_PATH] = "edited_image.bmp";

  OPENFILENAMEA ofn = {0};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFilter = "BMP Image\0*.bmp\0All Files\0*.*\0";
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrDefExt = "bmp";
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

  if (!GetSaveFileNameA(&ofn))
    return;

  // Create BMP file
  FILE *f = fopen(filename, "wb");
  if (!f) {
    MessageBoxA(hwnd, "Failed to create file", "Error", MB_ICONERROR);
    return;
  }

  int width = g_image.width;
  int height = g_image.height;
  int rowSize = ((width * 3 + 3) / 4) * 4;
  int dataSize = rowSize * height;

  // BMP File Header
  BITMAPFILEHEADER bfh = {0};
  bfh.bfType = 0x4D42; // "BM"
  bfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dataSize;
  bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

  // BMP Info Header
  BITMAPINFOHEADER bih = {0};
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = width;
  bih.biHeight = height;
  bih.biPlanes = 1;
  bih.biBitCount = 24;
  bih.biCompression = BI_RGB;

  fwrite(&bfh, sizeof(bfh), 1, f);
  fwrite(&bih, sizeof(bih), 1, f);

  // Write pixel data (BGR, bottom-up)
  BYTE *row = (BYTE *)malloc(rowSize);
  for (int y = height - 1; y >= 0; y--) {
    memset(row, 0, rowSize);
    for (int x = 0; x < width; x++) {
      int srcIdx = (y * width + x) * 4;
      row[x * 3 + 0] = g_image.pixels[srcIdx + 2]; // B
      row[x * 3 + 1] = g_image.pixels[srcIdx + 1]; // G
      row[x * 3 + 2] = g_image.pixels[srcIdx + 0]; // R
    }
    fwrite(row, rowSize, 1, f);
  }

  free(row);
  fclose(f);

  MessageBoxA(hwnd, "Image saved successfully!", "Save", MB_ICONINFORMATION);
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

void DrawThumbnailStrip(HWND hwnd, HDC hdc, RECT *clientRect) {
  if (!g_showThumbnails || g_browser.fileCount < 2)
    return;

  int stripY = clientRect->bottom - THUMB_STRIP_HEIGHT;
  int stripWidth = clientRect->right - clientRect->left;

  // Draw strip background
  RECT stripRect = {0, stripY, stripWidth, clientRect->bottom};
  HBRUSH stripBrush = CreateSolidBrush(RGB(20, 20, 20));
  FillRect(hdc, &stripRect, stripBrush);
  DeleteObject(stripBrush);

  // Draw border at top
  HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
  HPEN oldPen = SelectObject(hdc, borderPen);
  MoveToEx(hdc, 0, stripY, NULL);
  LineTo(hdc, stripWidth, stripY);
  SelectObject(hdc, oldPen);
  DeleteObject(borderPen);

  // Calculate how many thumbnails fit
  int thumbsVisible =
      (stripWidth - THUMB_PADDING) / (THUMB_SIZE + THUMB_PADDING);
  if (thumbsVisible < 1)
    thumbsVisible = 1;

  // Center the strip around current image
  int startIdx = g_browser.currentIndex - thumbsVisible / 2;
  if (startIdx < 0)
    startIdx = 0;
  if (startIdx + thumbsVisible > g_browser.fileCount) {
    startIdx = g_browser.fileCount - thumbsVisible;
    if (startIdx < 0)
      startIdx = 0;
  }

  // Draw thumbnail placeholders with filenames
  int x = THUMB_PADDING;
  for (int i = 0; i < thumbsVisible && (startIdx + i) < g_browser.fileCount;
       i++) {
    int idx = startIdx + i;
    int thumbY = stripY + THUMB_PADDING;

    // Highlight current image
    COLORREF bgColor =
        (idx == g_browser.currentIndex) ? RGB(70, 130, 180) : RGB(50, 50, 50);
    HBRUSH thumbBrush = CreateSolidBrush(bgColor);
    RECT thumbRect = {x, thumbY, x + THUMB_SIZE, thumbY + THUMB_SIZE};
    FillRect(hdc, &thumbRect, thumbBrush);
    DeleteObject(thumbBrush);

    // Draw border
    HPEN thumbPen = CreatePen(
        PS_SOLID, 1,
        (idx == g_browser.currentIndex) ? RGB(100, 180, 255) : RGB(80, 80, 80));
    SelectObject(hdc, thumbPen);
    Rectangle(hdc, x, thumbY, x + THUMB_SIZE, thumbY + THUMB_SIZE);
    DeleteObject(thumbPen);

    // Draw file number
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(200, 200, 200));
    char numStr[16];
    snprintf(numStr, sizeof(numStr), "%d", idx + 1);
    RECT numRect = thumbRect;
    DrawTextA(hdc, numStr, -1, &numRect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    x += THUMB_SIZE + THUMB_PADDING;
  }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                            LPARAM lParam) {
  switch (uMsg) {
  case WM_ERASEBKGND:
    // Prevent flicker by not erasing background
    // We'll draw the background ourselves in WM_PAINT
    return 1;

  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT clientRect;
    GetClientRect(hwnd, &clientRect);
    int width = clientRect.right - clientRect.left;
    int height = clientRect.bottom - clientRect.top;

    // Create double buffer
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
    HBITMAP oldBitmap = SelectObject(memDC, memBitmap);

    // Fill background with theme color
    HBRUSH bgBrush = CreateSolidBrush(g_bgColor);
    FillRect(memDC, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    // Draw image to buffer
    if (g_image.pixels && g_renderer.hMemDC) {
      int scaledWidth = (int)(g_image.width * g_renderer.scale);
      int scaledHeight = (int)(g_image.height * g_renderer.scale);

      SetStretchBltMode(memDC, HALFTONE);
      SetBrushOrgEx(memDC, 0, 0, NULL);

      StretchBlt(memDC, g_renderer.offsetX, g_renderer.offsetY, scaledWidth,
                 scaledHeight, g_renderer.hMemDC, 0, 0, g_image.width,
                 g_image.height, SRCCOPY);
    } else {
      // No image - draw help text
      SetBkMode(memDC, TRANSPARENT);
      SetTextColor(memDC, g_textColor);

      const char *msg = "Drag & drop an image or press O to open";
      RECT textRect = clientRect;
      DrawTextA(memDC, msg, -1, &textRect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Draw info panel overlay to buffer
    DrawInfoPanel(memDC, &clientRect);

    // Draw slideshow indicator to buffer
    if (g_slideshowActive) {
      SetBkMode(memDC, TRANSPARENT);
      SetTextColor(memDC, RGB(0, 200, 100));
      HFONT font =
          CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
      HFONT oldFont = SelectObject(memDC, font);
      TextOutA(memDC, 15, 15, "▶ SLIDESHOW", 12);
      SelectObject(memDC, oldFont);
      DeleteObject(font);
    }

    // Draw thumbnail strip at bottom
    DrawThumbnailStrip(hwnd, memDC, &clientRect);

    // Copy buffer to screen in one operation (no flicker!)
    BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

    // Cleanup double buffer
    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);

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
    } else if (wParam == TIMER_ANIMATION && g_image.isAnimated) {
      // Advance GIF frame
      if (ImageLoader_NextFrame(&g_image)) {
        // Recreate bitmap with new frame
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        ReleaseDC(hwnd, hdc);
        InvalidateRect(hwnd, NULL, FALSE);

        // Set timer for next frame
        int delay = ImageLoader_GetFrameDelay(&g_image);
        SetTimer(hwnd, TIMER_ANIMATION, delay, NULL);
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

    case 'I': // Toggle info panel
      g_showInfo = !g_showInfo;
      InvalidateRect(hwnd, NULL, TRUE);
      break;

    case 'T': // Toggle theme
      ToggleTheme();
      InvalidateRect(hwnd, NULL, TRUE);
      break;

    case 'G': // Toggle thumbnail strip
      g_showThumbnails = !g_showThumbnails;
      InvalidateRect(hwnd, NULL, TRUE);
      break;

    case 'R': { // Rotate right 90°
      if (g_image.pixels) {
        ImageLoader_RotateRight(&g_image);
        // Recreate bitmap
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        Renderer_FitToWindow(&g_renderer, &clientRect, &g_image);
        ReleaseDC(hwnd, hdc);
        UpdateWindowTitle(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;
    }

    case 'L': { // Rotate left 90°
      if (g_image.pixels) {
        ImageLoader_RotateLeft(&g_image);
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        Renderer_FitToWindow(&g_renderer, &clientRect, &g_image);
        ReleaseDC(hwnd, hdc);
        UpdateWindowTitle(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;
    }

    case 'H': { // Flip horizontal
      if (g_image.pixels) {
        ImageLoader_FlipHorizontal(&g_image);
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        ReleaseDC(hwnd, hdc);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;
    }

    case 'V': { // Flip vertical
      if (g_image.pixels) {
        ImageLoader_FlipVertical(&g_image);
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        ReleaseDC(hwnd, hdc);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;
    }

    case 'C': // Copy to clipboard (with Ctrl)
      if (GetKeyState(VK_CONTROL) & 0x8000) {
        CopyImageToClipboard(hwnd);
      }
      break;

    case VK_DELETE: // Delete to recycle bin
      DeleteCurrentImage(hwnd);
      break;

    case 'E': // Open in Explorer
      OpenInExplorer();
      break;

    case 'W': // Set as wallpaper
      SetAsWallpaper();
      break;

    case 'P': // Print image
      PrintImage(hwnd);
      break;

    case 'S': // Toggle slideshow OR Save (with Ctrl)
      if (GetKeyState(VK_CONTROL) & 0x8000) {
        SaveImage(hwnd);
      } else {
        ToggleSlideshow(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case 'B': // Increase brightness
      if (g_image.pixels) {
        ImageLoader_AdjustBrightness(&g_image, 10);
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        ReleaseDC(hwnd, hdc);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case 'N': // Decrease brightness (N for "night")
      if (g_image.pixels) {
        ImageLoader_AdjustBrightness(&g_image, -10);
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        ReleaseDC(hwnd, hdc);
        InvalidateRect(hwnd, NULL, TRUE);
      }
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
    case VK_ADD: // Zoom in OR decrease slideshow interval
      if (g_slideshowActive) {
        // Decrease interval (faster slideshow)
        g_slideshowInterval -= 500;
        if (g_slideshowInterval < SLIDESHOW_MIN_INTERVAL)
          g_slideshowInterval = SLIDESHOW_MIN_INTERVAL;
        KillTimer(hwnd, TIMER_SLIDESHOW);
        SetTimer(hwnd, TIMER_SLIDESHOW, g_slideshowInterval, NULL);
        UpdateWindowTitle(hwnd);
      } else {
        Renderer_SetScale(&g_renderer, g_renderer.scale * 1.25f);
        {
          RECT clientRect;
          GetClientRect(hwnd, &clientRect);
          Renderer_CenterImage(&g_renderer, &clientRect, &g_image);
        }
        UpdateWindowTitle(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case VK_OEM_MINUS:
    case VK_SUBTRACT: // Zoom out OR increase slideshow interval
      if (g_slideshowActive) {
        // Increase interval (slower slideshow)
        g_slideshowInterval += 500;
        if (g_slideshowInterval > SLIDESHOW_MAX_INTERVAL)
          g_slideshowInterval = SLIDESHOW_MAX_INTERVAL;
        KillTimer(hwnd, TIMER_SLIDESHOW);
        SetTimer(hwnd, TIMER_SLIDESHOW, g_slideshowInterval, NULL);
        UpdateWindowTitle(hwnd);
      } else {
        Renderer_SetScale(&g_renderer, g_renderer.scale * 0.8f);
        {
          RECT clientRect;
          GetClientRect(hwnd, &clientRect);
          Renderer_CenterImage(&g_renderer, &clientRect, &g_image);
        }
        UpdateWindowTitle(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
      }
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

// pure c image viewer
// main entry point
//
// controls:
//   o              open file
//   left/right     prev/next image
//   f11 or f       fullscreen
//   0 / 1          fit to window / actual size
//   +/-            zoom (or slideshow speed)
//   scroll         zoom at cursor
//   drag           pan around
//   s              slideshow
//   ctrl+s         save as png/jpg/bmp
//   ctrl+z         undo
//   i              info panel
//   t              theme toggle
//   r / l          rotate
//   h / v          flip
//   ctrl+c         copy
//   del            trash
//   e              open in explorer
//   w              set wallpaper
//   p              print
//   esc            exit

#include "../lib/stb_image_write.h"
#include "file_browser.h"
#include "image_loader.h"
#include "renderer.h"
#include "settings.h"
#include "ui.h"
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <windows.h>
#include <windowsx.h>

// Window class name
#define WINDOW_CLASS "PureCImageViewer"
#define WINDOW_TITLE "pix"

// Timer IDs
#define TIMER_SLIDESHOW 1
#define TIMER_ANIMATION 2

// Slideshow speed limits (milliseconds)
#define SLIDESHOW_MIN_INTERVAL 500
#define SLIDESHOW_MAX_INTERVAL 30000

// Global state (shared via app_state.h)
ImageData g_image = {0};
Renderer g_renderer = {0};
FileBrowser g_browser = {0};
BOOL g_fullscreen = FALSE;
static WINDOWPLACEMENT g_prevPlacement = {sizeof(g_prevPlacement)};

// Panning state (local to this file)
static BOOL g_isPanning = FALSE;
static int g_panStartX = 0;
static int g_panStartY = 0;
static int g_offsetStartX = 0;
static int g_offsetStartY = 0;

// Slideshow state (shared)
BOOL g_slideshowActive = FALSE;
int g_slideshowInterval = 3000; // 3 seconds default
DWORD g_slideshowStartTime = 0;

// UI state (shared)
BOOL g_showInfo = FALSE;
BOOL g_darkTheme = TRUE;
BOOL g_showThumbnails = FALSE;
BOOL g_showStatusBar = TRUE;
BOOL g_showEditPanel = FALSE;
BOOL g_showZoom = TRUE;      // zoom % overlay
BOOL g_showHelp = FALSE;     // keyboard help popup
BOOL g_showSettings = FALSE; // settings panel

// Selection mode for cropping
BOOL g_selectMode = FALSE;
RECT g_selection = {0, 0, 0, 0};
static BOOL g_selectDragging = FALSE;
static int g_selectDragX = 0;
static int g_selectDragY = 0;

// Edit mode state (shared)
int g_editBrightness = 0;      // -100 to +100
float g_editContrast = 1.0f;   // 0.5 to 2.0
float g_editSaturation = 1.0f; // 0.0 to 2.0
int g_editSelection = 0;       // 0=brightness, 1=contrast, 2=saturation

// UI constants (now in app_state.h)
#define THUMB_SIZE 80
#define THUMB_PADDING 5
#define THUMB_STRIP_HEIGHT (THUMB_SIZE + THUMB_PADDING * 2)
#define STATUS_BAR_HEIGHT 28
#define SHADOW_SIZE 8
#define EDIT_PANEL_WIDTH 200

// Modern theme colors (shared)
COLORREF g_bgColor = RGB(18, 18, 18);        // Darker background
COLORREF g_textColor = RGB(220, 220, 220);   // Brighter text
COLORREF g_panelBgColor = RGB(28, 28, 30);   // Subtle panel
COLORREF g_accentColor = RGB(70, 130, 180);  // Steel blue accent
COLORREF g_statusBarColor = RGB(24, 24, 26); // Status bar bg

// Function prototypes
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void LoadImageFile(HWND hwnd, const char *filepath);
void UpdateWindowTitle(HWND hwnd);
void ToggleFullscreen(HWND hwnd);
void ToggleSlideshow(HWND hwnd);
void ToggleTheme(void);
void CopyImageToClipboard(HWND hwnd);
void DeleteCurrentImage(HWND hwnd);
void OpenInExplorer(void);
void SetAsWallpaper(void);
void PrintImage(HWND hwnd);
void SaveImage(HWND hwnd);
void ApplyEdits(HWND hwnd);

// Batch processing mode (returns 1 if batch mode was used, 0 for normal GUI)
int RunBatchMode(int argc, char *argv[]) {
  if (argc < 3)
    return 0;

  // Check for --batch-upscale
  if (strcmp(argv[1], "--batch-upscale") == 0) {
    const char *folder = argv[2];
    int scale = (argc > 3) ? atoi(argv[3]) : 2; // default 2x

    // Attach console for output
    AttachConsole(ATTACH_PARENT_PROCESS);
    FILE *con = freopen("CONOUT$", "w", stdout);

    printf("\npix batch upscale\n");
    printf("folder: %s\n", folder);
    printf("scale: %dx\n", scale);
    printf("--------------------------------\n");

    // Create output folder
    char outFolder[MAX_PATH];
    snprintf(outFolder, sizeof(outFolder), "%s\\upscaled", folder);
    CreateDirectoryA(outFolder, NULL);

    // Find all images
    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\*.*", folder);

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);
    int processed = 0;

    if (hFind != INVALID_HANDLE_VALUE) {
      do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
          // Check if image file
          const char *ext = strrchr(findData.cFileName, '.');
          if (ext &&
              (_stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0 ||
               _stricmp(ext, ".png") == 0 || _stricmp(ext, ".bmp") == 0)) {

            char inputPath[MAX_PATH];
            snprintf(inputPath, sizeof(inputPath), "%s\\%s", folder,
                     findData.cFileName);

            printf("processing: %s ... ", findData.cFileName);
            fflush(stdout);

            // Load image
            ImageData img = {0};
            if (ImageLoader_Load(inputPath, &img)) {
              // Upscale
              int newW = img.width * scale;
              int newH = img.height * scale;
              ImageLoader_ResizeLanczos(&img, newW, newH);

              // Save to output folder
              char outputPath[MAX_PATH];
              snprintf(outputPath, sizeof(outputPath), "%s\\%s", outFolder,
                       findData.cFileName);

              // Use stbi_write for PNG output
              stbi_write_png(outputPath, img.width, img.height, 4, img.pixels,
                             img.width * 4);

              ImageLoader_Free(&img);
              printf("done (%dx%d)\n", newW, newH);
              processed++;
            } else {
              printf("failed to load\n");
            }
          }
        }
      } while (FindNextFileA(hFind, &findData));
      FindClose(hFind);
    }

    printf("--------------------------------\n");
    printf("processed %d images\n", processed);
    printf("output: %s\n\n", outFolder);

    if (con)
      fclose(con);
    return 1; // batch mode was used
  }

  return 0; // not batch mode
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
  (void)hPrevInstance;

  // Check for batch mode (command-line operations)
  int argc;
  LPWSTR *argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (argvW && argc >= 2) {
    // Convert to char* for our batch function
    char **argv = (char **)malloc(argc * sizeof(char *));
    for (int i = 0; i < argc; i++) {
      int len =
          WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
      argv[i] = (char *)malloc(len);
      WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, argv[i], len, NULL, NULL);
    }
    LocalFree(argvW);

    // Load settings for batch mode too
    Settings_Load(&g_settings);
    Settings_ApplyThreads(&g_settings);

    if (RunBatchMode(argc, argv)) {
      // Batch mode completed, exit
      for (int i = 0; i < argc; i++)
        free(argv[i]);
      free(argv);
      return 0;
    }
    for (int i = 0; i < argc; i++)
      free(argv[i]);
    free(argv);
  } else if (argvW) {
    LocalFree(argvW);
  }

  // Load settings from pix.ini
  Settings_Load(&g_settings);
  Settings_ApplyThreads(&g_settings);

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
  // load custom icon from resources
  wc.hIcon = LoadIconA(hInstance, "IDI_ICON1");
  wc.hIconSm = LoadIconA(hInstance, "IDI_ICON1");
  // fallback to default if custom icon not found
  if (!wc.hIcon)
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  if (!wc.hIconSm)
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
               "%s - %dx%d - [%d/%d] - SLIDESHOW (%.1fs) - pix", filename,
               g_image.width, g_image.height, g_browser.currentIndex + 1,
               g_browser.fileCount, seconds);
    } else {
      snprintf(title, sizeof(title), "%s - %dx%d - %d%% - [%d/%d] - pix",
               filename, g_image.width, g_image.height, zoomPercent,
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
    g_bgColor = RGB(18, 18, 18);
    g_textColor = RGB(220, 220, 220);
    g_panelBgColor = RGB(28, 28, 30);
    g_statusBarColor = RGB(24, 24, 26);
    g_accentColor = RGB(70, 130, 180);
  } else {
    g_bgColor = RGB(240, 240, 240);
    g_textColor = RGB(40, 40, 40);
    g_panelBgColor = RGB(250, 250, 252);
    g_statusBarColor = RGB(235, 235, 238);
    g_accentColor = RGB(0, 100, 180);
  }
}

void CopyImageToClipboard(HWND hwnd) {
  if (!g_image.pixels)
    return;

  int width = g_image.width;
  int height = g_image.height;

  // Use standard 24-bit DIB for maximum compatibility
  int rowBytes = ((width * 3 + 3) / 4) * 4; // 4-byte aligned
  int imageSize = rowBytes * height;
  int totalSize = sizeof(BITMAPINFOHEADER) + imageSize;

  HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, totalSize);
  if (!hMem)
    return;

  BYTE *pData = (BYTE *)GlobalLock(hMem);
  if (!pData) {
    GlobalFree(hMem);
    return;
  }

  // Setup bitmap header
  BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)pData;
  bih->biSize = sizeof(BITMAPINFOHEADER);
  bih->biWidth = width;
  bih->biHeight = height; // Bottom-up (positive height)
  bih->biPlanes = 1;
  bih->biBitCount = 24;
  bih->biCompression = BI_RGB;
  bih->biSizeImage = imageSize;

  // Copy pixels (RGBA -> BGR, flip vertically)
  BYTE *dst = pData + sizeof(BITMAPINFOHEADER);
  for (int y = 0; y < height; y++) {
    BYTE *srcRow = g_image.pixels + (height - 1 - y) * width * 4;
    BYTE *dstRow = dst + y * rowBytes;
    for (int x = 0; x < width; x++) {
      dstRow[x * 3 + 0] = srcRow[x * 4 + 2]; // B
      dstRow[x * 3 + 1] = srcRow[x * 4 + 1]; // G
      dstRow[x * 3 + 2] = srcRow[x * 4 + 0]; // R
    }
  }

  GlobalUnlock(hMem);

  if (OpenClipboard(hwnd)) {
    EmptyClipboard();
    if (SetClipboardData(CF_DIB, hMem) == NULL) {
      GlobalFree(hMem);
    }
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
  di.lpszDocName = "pix Print";

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

  char filename[MAX_PATH] = "edited_image.png";

  OPENFILENAMEA ofn = {0};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFilter = "PNG Image\0*.png\0JPEG Image\0*.jpg;*.jpeg\0BMP "
                    "Image\0*.bmp\0All Files\0*.*\0";
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrDefExt = "png";
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

  if (!GetSaveFileNameA(&ofn))
    return;

  int width = g_image.width;
  int height = g_image.height;
  int success = 0;

  // determine format from extension
  const char *ext = strrchr(filename, '.');

  if (ext && (_stricmp(ext, ".png") == 0)) {
    // save as png
    success =
        stbi_write_png(filename, width, height, 4, g_image.pixels, width * 4);
  } else if (ext &&
             (_stricmp(ext, ".jpg") == 0 || _stricmp(ext, ".jpeg") == 0)) {
    // save as jpg (quality 90)
    success = stbi_write_jpg(filename, width, height, 4, g_image.pixels, 90);
  } else if (ext && (_stricmp(ext, ".bmp") == 0)) {
    // save as bmp
    success = stbi_write_bmp(filename, width, height, 4, g_image.pixels);
  } else {
    // default to png
    success =
        stbi_write_png(filename, width, height, 4, g_image.pixels, width * 4);
  }

  if (success) {
    MessageBoxA(hwnd, "Image saved successfully!", "Save", MB_ICONINFORMATION);
  } else {
    MessageBoxA(hwnd, "Failed to save image", "Error", MB_ICONERROR);
  }
}

// Draw functions are now in ui.c

void ApplyEdits(HWND hwnd) {
  if (!g_image.pixels)
    return;

  // Apply all edits
  if (g_editBrightness != 0) {
    ImageLoader_AdjustBrightness(&g_image, g_editBrightness);
  }
  if (g_editContrast != 1.0f) {
    ImageLoader_AdjustContrast(&g_image, g_editContrast);
  }
  if (g_editSaturation != 1.0f) {
    ImageLoader_AdjustSaturation(&g_image, g_editSaturation);
  }

  // Reset edit values
  g_editBrightness = 0;
  g_editContrast = 1.0f;
  g_editSaturation = 1.0f;

  // Recreate bitmap
  HDC hdc = GetDC(hwnd);
  Renderer_Cleanup(&g_renderer);
  Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
  ReleaseDC(hwnd, hdc);

  g_showEditPanel = FALSE;
  InvalidateRect(hwnd, NULL, TRUE);
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

      // use nearest-neighbor when zoomed in (sharp pixels)
      // use halftone when zoomed out (smooth downscaling)
      if (g_renderer.scale >= 1.0f) {
        SetStretchBltMode(memDC, COLORONCOLOR);
      } else {
        SetStretchBltMode(memDC, HALFTONE);
        SetBrushOrgEx(memDC, 0, 0, NULL);
      }

      StretchBlt(memDC, g_renderer.offsetX, g_renderer.offsetY, scaledWidth,
                 scaledHeight, g_renderer.hMemDC, 0, 0, g_image.width,
                 g_image.height, SRCCOPY);

      // Draw selection rectangle if in crop mode
      if (g_selectMode) {
        // Convert image coords to screen coords
        int selX =
            g_renderer.offsetX + (int)(g_selection.left * g_renderer.scale);
        int selY =
            g_renderer.offsetY + (int)(g_selection.top * g_renderer.scale);
        int selW =
            (int)((g_selection.right - g_selection.left) * g_renderer.scale);
        int selH =
            (int)((g_selection.bottom - g_selection.top) * g_renderer.scale);

        // Dim area outside selection
        HBRUSH dimBrush = CreateSolidBrush(RGB(0, 0, 0));
        RECT topDim = {g_renderer.offsetX, g_renderer.offsetY,
                       g_renderer.offsetX + scaledWidth, selY};
        RECT bottomDim = {g_renderer.offsetX, selY + selH,
                          g_renderer.offsetX + scaledWidth,
                          g_renderer.offsetY + scaledHeight};
        RECT leftDim = {g_renderer.offsetX, selY, selX, selY + selH};
        RECT rightDim = {selX + selW, selY, g_renderer.offsetX + scaledWidth,
                         selY + selH};
        // Draw with 50% transparency effect via pattern
        SetBkColor(memDC, RGB(0, 0, 0));
        FillRect(memDC, &topDim, dimBrush);
        FillRect(memDC, &bottomDim, dimBrush);
        FillRect(memDC, &leftDim, dimBrush);
        FillRect(memDC, &rightDim, dimBrush);
        DeleteObject(dimBrush);

        // Draw selection border
        HPEN selPen = CreatePen(PS_DASH, 2, RGB(255, 255, 255));
        HPEN oldPen = SelectObject(memDC, selPen);
        HBRUSH oldBrush = SelectObject(memDC, GetStockObject(NULL_BRUSH));
        Rectangle(memDC, selX, selY, selX + selW, selY + selH);
        SelectObject(memDC, oldBrush);
        SelectObject(memDC, oldPen);
        DeleteObject(selPen);

        // Draw crop size text
        char cropText[64];
        snprintf(cropText, sizeof(cropText),
                 "Crop: %ldx%ld  (C to crop, ESC to cancel)",
                 g_selection.right - g_selection.left,
                 g_selection.bottom - g_selection.top);
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(255, 255, 255));
        TextOutA(memDC, selX + 5, selY + 5, cropText, (int)strlen(cropText));
      }
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

    // Draw slideshow progress bar at top
    DrawSlideshowProgress(memDC, &clientRect);

    // Draw zoom percentage overlay
    DrawZoomOverlay(memDC, &clientRect);

    // Draw slideshow indicator to buffer
    if (g_slideshowActive) {
      SetBkMode(memDC, TRANSPARENT);
      SetTextColor(memDC, g_accentColor);
      HFONT font =
          CreateFontA(14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
      HFONT oldFont = SelectObject(memDC, font);
      TextOutA(memDC, 15, 10, "SLIDESHOW", 9);
      SelectObject(memDC, oldFont);
      DeleteObject(font);
    }

    // Draw status bar (hide in fullscreen for cleaner look)
    if (!g_fullscreen) {
      DrawStatusBar(memDC, &clientRect);
    }

    // Draw edit panel
    DrawEditPanel(memDC, &clientRect);

    // Draw help overlay (on top of everything)
    DrawHelpOverlay(memDC, &clientRect);

    // Draw settings overlay
    DrawSettingsOverlay(memDC, &clientRect);

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

    case 'I': // Toggle info panel
      g_showInfo = !g_showInfo;
      InvalidateRect(hwnd, NULL, TRUE);
      break;

    case 'G': // Toggle thumbnail strip
      g_showThumbnails = !g_showThumbnails;
      InvalidateRect(hwnd, NULL, TRUE);
      break;

    case VK_LEFT: // Adjust edit value or previous image
      if (g_showEditPanel) {
        if (g_editSelection == 0) {
          g_editBrightness -= 5;
          if (g_editBrightness < -100)
            g_editBrightness = -100;
        } else if (g_editSelection == 1) {
          g_editContrast -= 0.1f;
          if (g_editContrast < 0.5f)
            g_editContrast = 0.5f;
        } else if (g_editSelection == 2) {
          g_editSaturation -= 0.1f;
          if (g_editSaturation < 0.0f)
            g_editSaturation = 0.0f;
        }
        InvalidateRect(hwnd, NULL, TRUE);
      } else {
        const char *prev = FileBrowser_Previous(&g_browser);
        if (prev)
          LoadImageFile(hwnd, prev);
      }
      break;

    case VK_RIGHT: // Adjust edit value or next image
      if (g_showEditPanel) {
        if (g_editSelection == 0) {
          g_editBrightness += 5;
          if (g_editBrightness > 100)
            g_editBrightness = 100;
        } else if (g_editSelection == 1) {
          g_editContrast += 0.1f;
          if (g_editContrast > 2.0f)
            g_editContrast = 2.0f;
        } else if (g_editSelection == 2) {
          g_editSaturation += 0.1f;
          if (g_editSaturation > 2.0f)
            g_editSaturation = 2.0f;
        }
        InvalidateRect(hwnd, NULL, TRUE);
      } else {
        const char *next = FileBrowser_Next(&g_browser);
        if (next)
          LoadImageFile(hwnd, next);
      }
      break;

    case VK_SPACE: { // Next image
      const char *next = FileBrowser_Next(&g_browser);
      if (next)
        LoadImageFile(hwnd, next);
      break;
    }

    case 'E': // Toggle edit panel (Shift+E for explorer)
      if (GetKeyState(VK_SHIFT) & 0x8000) {
        OpenInExplorer();
      } else {
        g_showEditPanel = !g_showEditPanel;
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case VK_RETURN: // Apply edits
      if (g_showEditPanel) {
        ApplyEdits(hwnd);
      }
      break;

    case VK_UP: // Edit panel navigation
      if (g_showEditPanel) {
        g_editSelection = (g_editSelection + 2) % 3;
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case VK_DOWN:
      if (g_showEditPanel) {
        g_editSelection = (g_editSelection + 1) % 3;
        InvalidateRect(hwnd, NULL, TRUE);
      }
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

    case 'C': // Copy (Ctrl+C) or Crop (Shift+C or just C)
      if (GetKeyState(VK_CONTROL) & 0x8000) {
        CopyImageToClipboard(hwnd);
      } else if (GetKeyState(VK_SHIFT) & 0x8000) {
        // Shift+C = Toggle selection mode
        g_selectMode = !g_selectMode;
        if (g_selectMode && g_image.pixels) {
          // Initialize selection to center 50% of image
          int imgW = g_image.width;
          int imgH = g_image.height;
          g_selection.left = imgW / 4;
          g_selection.top = imgH / 4;
          g_selection.right = imgW * 3 / 4;
          g_selection.bottom = imgH * 3 / 4;
        }
        InvalidateRect(hwnd, NULL, TRUE);
      } else if (g_selectMode && g_image.pixels) {
        // C = Crop to selection
        int cropX = g_selection.left;
        int cropY = g_selection.top;
        int cropW = g_selection.right - g_selection.left;
        int cropH = g_selection.bottom - g_selection.top;
        if (cropW > 0 && cropH > 0) {
          ImageLoader_SaveUndo(&g_image);
          ImageLoader_Crop(&g_image, cropX, cropY, cropW, cropH);
          HDC hdc = GetDC(hwnd);
          Renderer_Cleanup(&g_renderer);
          Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
          RECT clientRect;
          GetClientRect(hwnd, &clientRect);
          Renderer_FitToWindow(&g_renderer, &clientRect, &g_image);
          ReleaseDC(hwnd, hdc);
          UpdateWindowTitle(hwnd);
          g_selectMode = FALSE;
          InvalidateRect(hwnd, NULL, TRUE);
        }
      }
      break;

    case 'Z': { // Undo (with Ctrl) or toggle zoom overlay
      if (GetKeyState(VK_CONTROL) & 0x8000) {
        // Ctrl+Z = Undo
        if (g_image.pixels && ImageLoader_Undo(&g_image)) {
          // Recreate bitmap with restored state
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
      } else {
        // Z alone = toggle zoom overlay
        g_showZoom = !g_showZoom;
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;
    }

    case VK_DELETE: // Delete to recycle bin
      DeleteCurrentImage(hwnd);
      break;

    case VK_OEM_2: // ? key (/ with shift) or / without - toggle help
      if (GetKeyState(VK_SHIFT) & 0x8000) {
        g_showHelp = !g_showHelp;
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case VK_F1: // F1 also toggles help
      g_showHelp = !g_showHelp;
      InvalidateRect(hwnd, NULL, TRUE);
      break;

    case VK_F2: // F2 opens settings panel
      g_showSettings = !g_showSettings;
      InvalidateRect(hwnd, NULL, TRUE);
      break;

    case 'M': // Cycle max image size (when settings panel is open)
      if (g_showSettings) {
        Settings_CycleMaxSize(&g_settings);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case 'W': // Toggle warnings OR Set as wallpaper
      if (g_showSettings) {
        // When settings panel is open, toggle warnings
        g_settings.showWarnings = !g_settings.showWarnings;
        Settings_Save(&g_settings);
        InvalidateRect(hwnd, NULL, TRUE);
      } else {
        // Otherwise set as wallpaper
        SetAsWallpaper();
      }
      break;

    case 'T': // Toggle theme OR cycle CPU threads
      if (g_showSettings) {
        // When settings panel is open, cycle threads
        Settings_CycleThreads(&g_settings);
        InvalidateRect(hwnd, NULL, TRUE);
      } else {
        // Otherwise toggle theme
        ToggleTheme();
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case 'P': // Print image OR Reset (with Shift)
      if (GetKeyState(VK_SHIFT) & 0x8000) {
        // Shift+P = Reset to original (reloads from disk)
        if (g_image.pixels && ImageLoader_Reset(&g_image)) {
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
      } else {
        PrintImage(hwnd);
      }
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

    case 'A': // Auto-levels
      if (g_image.pixels && !g_showEditPanel) {
        ImageLoader_AutoLevels(&g_image);
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        ReleaseDC(hwnd, hdc);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case 'X': // Invert colors
      if (g_image.pixels) {
        ImageLoader_Invert(&g_image);
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        ReleaseDC(hwnd, hdc);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case 'U': // Blur
      if (g_image.pixels) {
        ImageLoader_Blur(&g_image);
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        ReleaseDC(hwnd, hdc);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case 'Y': // Sharpen
      if (g_image.pixels) {
        ImageLoader_Sharpen(&g_image);
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        ReleaseDC(hwnd, hdc);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case 'J': // Sepia/Vintage
      if (g_image.pixels) {
        ImageLoader_Sepia(&g_image);
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        ReleaseDC(hwnd, hdc);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case 'Q': // Upscale 2x using Lanczos (high quality)
      if (g_image.pixels) {
        int newW = g_image.width * 2;
        int newH = g_image.height * 2;
        // use settings for max size limit (supports up to 32K)
        if (newW <= g_settings.maxImageSize &&
            newH <= g_settings.maxImageSize) {
          // warn if operation will use lots of memory
          size_t memNeeded = Settings_EstimateMemory(newW, newH);
          if (Settings_WarnIfLarge(hwnd, memNeeded)) {
            ImageLoader_ResizeLanczos(&g_image, newW, newH);
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
        }
      }
      break;

    case 'K': // Grayscale
      if (g_image.pixels) {
        ImageLoader_Grayscale(&g_image);
        HDC hdc = GetDC(hwnd);
        Renderer_Cleanup(&g_renderer);
        Renderer_CreateBitmap(&g_renderer, hdc, &g_image);
        ReleaseDC(hwnd, hdc);
        InvalidateRect(hwnd, NULL, TRUE);
      }
      break;

    case VK_ESCAPE: // Close panels, exit fullscreen, or quit
      if (g_selectMode) {
        g_selectMode = FALSE;
        InvalidateRect(hwnd, NULL, TRUE);
      } else if (g_showSettings) {
        g_showSettings = FALSE;
        InvalidateRect(hwnd, NULL, TRUE);
      } else if (g_showHelp) {
        g_showHelp = FALSE;
        InvalidateRect(hwnd, NULL, TRUE);
      } else if (g_showEditPanel) {
        g_showEditPanel = FALSE;
        g_editBrightness = 0;
        g_editContrast = 1.0f;
        g_editSaturation = 1.0f;
        InvalidateRect(hwnd, NULL, TRUE);
      } else if (g_slideshowActive) {
        ToggleSlideshow(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
      } else if (g_fullscreen) {
        ToggleFullscreen(hwnd);
      } else {
        PostQuitMessage(0);
      }
      break;

    case VK_F11: // Toggle fullscreen
    case 'F':    // F also toggles fullscreen
      ToggleFullscreen(hwnd);
      if (g_image.pixels) {
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        Renderer_FitToWindow(&g_renderer, &clientRect, &g_image);
        InvalidateRect(hwnd, NULL, TRUE);
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
    int mouseX = GET_X_LPARAM(lParam);
    int mouseY = GET_Y_LPARAM(lParam);

    if (g_selectMode && g_image.pixels) {
      // Selection mode - start drawing selection
      int imgX = (int)((mouseX - g_renderer.offsetX) / g_renderer.scale);
      int imgY = (int)((mouseY - g_renderer.offsetY) / g_renderer.scale);

      if (imgX >= 0 && imgX < g_image.width && imgY >= 0 &&
          imgY < g_image.height) {
        g_selectDragging = TRUE;
        g_selectDragX = imgX;
        g_selectDragY = imgY;
        g_selection.left = imgX;
        g_selection.top = imgY;
        g_selection.right = imgX;
        g_selection.bottom = imgY;
        SetCapture(hwnd);
      }
    } else if (g_image.pixels) {
      // Normal mode - start panning
      g_isPanning = TRUE;
      g_panStartX = mouseX;
      g_panStartY = mouseY;
      g_offsetStartX = g_renderer.offsetX;
      g_offsetStartY = g_renderer.offsetY;
      SetCapture(hwnd);
      SetCursor(LoadCursor(NULL, IDC_SIZEALL));
    }
    return 0;
  }

  case WM_LBUTTONUP: {
    if (g_selectDragging) {
      // Stop selection
      g_selectDragging = FALSE;
      ReleaseCapture();

      // Ensure minimum selection size
      int w = g_selection.right - g_selection.left;
      int h = g_selection.bottom - g_selection.top;
      if (w < 10 || h < 10) {
        g_selection.left = g_image.width / 4;
        g_selection.top = g_image.height / 4;
        g_selection.right = g_image.width * 3 / 4;
        g_selection.bottom = g_image.height * 3 / 4;
      }
      InvalidateRect(hwnd, NULL, FALSE);
    } else if (g_isPanning) {
      // Stop panning
      g_isPanning = FALSE;
      ReleaseCapture();
      SetCursor(LoadCursor(NULL, IDC_ARROW));
    }
    return 0;
  }

  case WM_MOUSEMOVE: {
    int mouseX = GET_X_LPARAM(lParam);
    int mouseY = GET_Y_LPARAM(lParam);

    if (g_selectDragging && g_selectMode) {
      // Selection mode - update selection rectangle
      int imgX = (int)((mouseX - g_renderer.offsetX) / g_renderer.scale);
      int imgY = (int)((mouseY - g_renderer.offsetY) / g_renderer.scale);

      // Clamp to image bounds
      if (imgX < 0)
        imgX = 0;
      if (imgY < 0)
        imgY = 0;
      if (imgX > g_image.width)
        imgX = g_image.width;
      if (imgY > g_image.height)
        imgY = g_image.height;

      // Handle drag in any direction
      g_selection.left = (imgX < g_selectDragX) ? imgX : g_selectDragX;
      g_selection.right = (imgX > g_selectDragX) ? imgX : g_selectDragX;
      g_selection.top = (imgY < g_selectDragY) ? imgY : g_selectDragY;
      g_selection.bottom = (imgY > g_selectDragY) ? imgY : g_selectDragY;

      InvalidateRect(hwnd, NULL, FALSE);
    } else if (g_isPanning && g_image.pixels) {
      // Normal mode - pan image
      int dx = mouseX - g_panStartX;
      int dy = mouseY - g_panStartY;
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

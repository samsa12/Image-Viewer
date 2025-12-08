// ui.c - ui drawing functions for pix
// extracted from main.c for better organization

#include "ui.h"
#include <stdio.h>
#include <string.h>

// draws the info panel with image details and exif data
void DrawInfoPanel(HDC hdc, RECT *clientRect) {
  if (!g_showInfo || !g_image.pixels)
    return;

  // panel dimensions - taller if we have exif
  int panelWidth = 280;
  int panelHeight = g_image.exif.hasExif ? 280 : 180;
  int margin = 15;
  int padding = 12;

  RECT panelRect = {clientRect->right - panelWidth - margin, margin,
                    clientRect->right - margin, margin + panelHeight};

  // draw semi-transparent panel background
  HBRUSH panelBrush = CreateSolidBrush(g_panelBgColor);
  FillRect(hdc, &panelRect, panelBrush);
  DeleteObject(panelBrush);

  // draw border
  HPEN borderPen = CreatePen(
      PS_SOLID, 1, g_darkTheme ? RGB(80, 80, 80) : RGB(180, 180, 180));
  HPEN oldPen = SelectObject(hdc, borderPen);
  Rectangle(hdc, panelRect.left, panelRect.top, panelRect.right,
            panelRect.bottom);
  SelectObject(hdc, oldPen);
  DeleteObject(borderPen);

  // setup text
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, g_textColor);

  HFONT font =
      CreateFontA(15, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, "Segoe UI");
  HFONT oldFont = SelectObject(hdc, font);

  const char *filename = strrchr(g_image.filepath, '\\');
  if (!filename)
    filename = strrchr(g_image.filepath, '/');
  filename = filename ? filename + 1 : g_image.filepath;

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

  int y = panelRect.top + padding;
  int lineHeight = 22;
  int labelX = panelRect.left + padding;

  HFONT boldFont =
      CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, "Segoe UI");
  SelectObject(hdc, boldFont);
  TextOutA(hdc, labelX, y, "Image Information", 17);
  y += lineHeight + 5;

  SelectObject(hdc, font);

  char buffer[256];

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

  snprintf(buffer, sizeof(buffer), "Size: %d x %d pixels", g_image.width,
           g_image.height);
  TextOutA(hdc, labelX, y, buffer, (int)strlen(buffer));
  y += lineHeight;

  snprintf(buffer, sizeof(buffer), "File: %s", sizeStr);
  TextOutA(hdc, labelX, y, buffer, (int)strlen(buffer));
  y += lineHeight;

  snprintf(buffer, sizeof(buffer), "Zoom: %d%%",
           (int)(g_renderer.scale * 100.0f));
  TextOutA(hdc, labelX, y, buffer, (int)strlen(buffer));
  y += lineHeight;

  snprintf(buffer, sizeof(buffer), "Position: %d of %d",
           g_browser.currentIndex + 1, g_browser.fileCount);
  TextOutA(hdc, labelX, y, buffer, (int)strlen(buffer));
  y += lineHeight;

  if (g_image.exif.hasExif) {
    y += 5;
    SetTextColor(hdc, g_accentColor);
    SelectObject(hdc, boldFont);
    TextOutA(hdc, labelX, y, "Camera Info", 11);
    y += lineHeight;

    SetTextColor(hdc, g_textColor);
    SelectObject(hdc, font);

    if (g_image.exif.camera[0]) {
      snprintf(buffer, sizeof(buffer), "Camera: %s", g_image.exif.camera);
      TextOutA(hdc, labelX, y, buffer, (int)strlen(buffer));
      y += lineHeight;
    }

    if (g_image.exif.dateTime[0]) {
      snprintf(buffer, sizeof(buffer), "Date: %s", g_image.exif.dateTime);
      TextOutA(hdc, labelX, y, buffer, (int)strlen(buffer));
      y += lineHeight;
    }

    char exposureLine[128] = {0};
    if (g_image.exif.exposure[0]) {
      strcat(exposureLine, g_image.exif.exposure);
      strcat(exposureLine, "s");
    }
    if (g_image.exif.aperture[0]) {
      if (exposureLine[0])
        strcat(exposureLine, "  ");
      strcat(exposureLine, g_image.exif.aperture);
    }
    if (g_image.exif.iso[0]) {
      if (exposureLine[0])
        strcat(exposureLine, "  ISO ");
      else
        strcat(exposureLine, "ISO ");
      strcat(exposureLine, g_image.exif.iso);
    }
    if (g_image.exif.focalLength[0]) {
      if (exposureLine[0])
        strcat(exposureLine, "  ");
      strcat(exposureLine, g_image.exif.focalLength);
    }
    if (exposureLine[0]) {
      TextOutA(hdc, labelX, y, exposureLine, (int)strlen(exposureLine));
    }
  }

  SelectObject(hdc, oldFont);
  DeleteObject(font);
  DeleteObject(boldFont);
}

// draws navigation thumbnail strip at bottom
void DrawThumbnailStrip(HWND hwnd, HDC hdc, RECT *clientRect) {
  (void)hwnd;
  if (!g_showThumbnails || g_browser.fileCount < 2)
    return;

  int stripY = clientRect->bottom - THUMB_STRIP_HEIGHT;
  int stripWidth = clientRect->right - clientRect->left;

  RECT stripRect = {0, stripY, stripWidth, clientRect->bottom};
  HBRUSH stripBrush = CreateSolidBrush(RGB(20, 20, 20));
  FillRect(hdc, &stripRect, stripBrush);
  DeleteObject(stripBrush);

  HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
  HPEN oldPen = SelectObject(hdc, borderPen);
  MoveToEx(hdc, 0, stripY, NULL);
  LineTo(hdc, stripWidth, stripY);
  SelectObject(hdc, oldPen);
  DeleteObject(borderPen);

  int thumbsVisible =
      (stripWidth - THUMB_PADDING) / (THUMB_SIZE + THUMB_PADDING);
  if (thumbsVisible < 1)
    thumbsVisible = 1;

  int startIdx = g_browser.currentIndex - thumbsVisible / 2;
  if (startIdx < 0)
    startIdx = 0;
  if (startIdx + thumbsVisible > g_browser.fileCount) {
    startIdx = g_browser.fileCount - thumbsVisible;
    if (startIdx < 0)
      startIdx = 0;
  }

  int x = THUMB_PADDING;
  for (int i = 0; i < thumbsVisible && (startIdx + i) < g_browser.fileCount;
       i++) {
    int idx = startIdx + i;
    int thumbY = stripY + THUMB_PADDING;

    COLORREF bgColor =
        (idx == g_browser.currentIndex) ? RGB(70, 130, 180) : RGB(50, 50, 50);
    HBRUSH thumbBrush = CreateSolidBrush(bgColor);
    RECT thumbRect = {x, thumbY, x + THUMB_SIZE, thumbY + THUMB_SIZE};
    FillRect(hdc, &thumbRect, thumbBrush);
    DeleteObject(thumbBrush);

    HPEN thumbPen = CreatePen(
        PS_SOLID, 1,
        (idx == g_browser.currentIndex) ? RGB(100, 180, 255) : RGB(80, 80, 80));
    SelectObject(hdc, thumbPen);
    Rectangle(hdc, x, thumbY, x + THUMB_SIZE, thumbY + THUMB_SIZE);
    DeleteObject(thumbPen);

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

// draws status bar at bottom of window
void DrawStatusBar(HDC hdc, RECT *clientRect) {
  if (!g_showStatusBar)
    return;

  int barY = clientRect->bottom - STATUS_BAR_HEIGHT;
  if (g_showThumbnails && g_browser.fileCount >= 2) {
    barY -= THUMB_STRIP_HEIGHT;
  }

  RECT barRect = {0, barY, clientRect->right, barY + STATUS_BAR_HEIGHT};
  HBRUSH barBrush = CreateSolidBrush(g_statusBarColor);
  FillRect(hdc, &barRect, barBrush);
  DeleteObject(barBrush);

  HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(45, 45, 48));
  HPEN oldPen = SelectObject(hdc, borderPen);
  MoveToEx(hdc, 0, barY, NULL);
  LineTo(hdc, clientRect->right, barY);
  SelectObject(hdc, oldPen);
  DeleteObject(borderPen);

  if (!g_image.pixels)
    return;

  HFONT font =
      CreateFontA(13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, "Segoe UI");
  HFONT oldFont = SelectObject(hdc, font);
  SetBkMode(hdc, TRANSPARENT);

  const char *filename = strrchr(g_image.filepath, '\\');
  filename = filename ? filename + 1 : g_image.filepath;

  char leftText[256];
  snprintf(leftText, sizeof(leftText), "  %s  |  %d × %d", filename,
           g_image.width, g_image.height);

  SetTextColor(hdc, g_textColor);
  TextOutA(hdc, 10, barY + 6, leftText, (int)strlen(leftText));

  char rightText[128];
  int zoom = (int)(g_renderer.scale * 100.0f);
  snprintf(rightText, sizeof(rightText), "%d%%  |  %d / %d  ", zoom,
           g_browser.currentIndex + 1, g_browser.fileCount);

  SIZE textSize;
  GetTextExtentPoint32A(hdc, rightText, (int)strlen(rightText), &textSize);
  TextOutA(hdc, clientRect->right - textSize.cx - 10, barY + 6, rightText,
           (int)strlen(rightText));

  SelectObject(hdc, oldFont);
  DeleteObject(font);
}

// draws subtle shadow around image
void DrawImageShadow(HDC hdc, int x, int y, int w, int h) {
  for (int i = SHADOW_SIZE; i > 0; i--) {
    HPEN shadowPen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
    HPEN oldPen = SelectObject(hdc, shadowPen);

    MoveToEx(hdc, x + i, y + h + i, NULL);
    LineTo(hdc, x + w + i, y + h + i);

    MoveToEx(hdc, x + w + i, y + i, NULL);
    LineTo(hdc, x + w + i, y + h + i);

    SelectObject(hdc, oldPen);
    DeleteObject(shadowPen);
  }
}

// draws slideshow progress bar at top
void DrawSlideshowProgress(HDC hdc, RECT *clientRect) {
  if (!g_slideshowActive)
    return;

  DWORD elapsed = GetTickCount() - g_slideshowStartTime;
  float progress = (float)elapsed / g_slideshowInterval;
  if (progress > 1.0f)
    progress = 1.0f;

  int barHeight = 4;
  int barWidth = (int)(clientRect->right * progress);

  RECT trackRect = {0, 0, clientRect->right, barHeight};
  HBRUSH trackBrush = CreateSolidBrush(RGB(50, 50, 55));
  FillRect(hdc, &trackRect, trackBrush);
  DeleteObject(trackBrush);

  if (barWidth > 0) {
    RECT progressRect = {0, 0, barWidth, barHeight};
    HBRUSH progressBrush = CreateSolidBrush(g_accentColor);
    FillRect(hdc, &progressRect, progressBrush);
    DeleteObject(progressBrush);

    RECT highlightRect = {0, 0, barWidth, 1};
    HBRUSH highlightBrush = CreateSolidBrush(RGB(120, 170, 210));
    FillRect(hdc, &highlightRect, highlightBrush);
    DeleteObject(highlightBrush);
  }
}

// draws zoom percentage in bottom left corner
void DrawZoomOverlay(HDC hdc, RECT *clientRect) {
  if (!g_image.pixels || !g_showZoom)
    return;

  int zoomPercent = (int)(g_renderer.scale * 100.0f);

  char zoomText[32];
  snprintf(zoomText, sizeof(zoomText), "%d%%", zoomPercent);

  HFONT font =
      CreateFontA(16, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, "Segoe UI");
  HFONT oldFont = SelectObject(hdc, font);

  SIZE textSize;
  GetTextExtentPoint32A(hdc, zoomText, (int)strlen(zoomText), &textSize);

  int padding = 6;
  int margin = 12;
  int x = margin;
  int y = clientRect->bottom - STATUS_BAR_HEIGHT - textSize.cy - margin -
          padding * 2;

  RECT bgRect = {x, y, x + textSize.cx + padding * 2,
                 y + textSize.cy + padding * 2};
  HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 35));
  FillRect(hdc, &bgRect, bgBrush);
  DeleteObject(bgBrush);

  HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 65));
  HPEN oldPen = SelectObject(hdc, borderPen);
  Rectangle(hdc, bgRect.left, bgRect.top, bgRect.right, bgRect.bottom);
  SelectObject(hdc, oldPen);
  DeleteObject(borderPen);

  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(180, 180, 180));
  TextOutA(hdc, x + padding, y + padding, zoomText, (int)strlen(zoomText));

  SelectObject(hdc, oldFont);
  DeleteObject(font);
}

// draws keyboard shortcuts help overlay
void DrawHelpOverlay(HDC hdc, RECT *clientRect) {
  if (!g_showHelp)
    return;

  const char *helpLines[] = {
      "keyboard shortcuts",       "",
      "o          open file",     "left/right prev/next image",
      "f11 / f    fullscreen",    "0 / 1      fit / actual size",
      "+/-        zoom",          "scroll     zoom at cursor",
      "s          slideshow",     "ctrl+z     undo",
      "r / l      rotate",        "h / v      flip",
      "q          upscale 2x",    "ctrl+s     save image",
      "shift+c    crop mode",     "c          crop",
      "i          info panel",    "t          toggle theme",
      "z          toggle zoom %", "?          this help",
      "esc        close / exit"};
  int lineCount = sizeof(helpLines) / sizeof(helpLines[0]);

  int lineHeight = 22;
  int panelWidth = 280;
  int panelHeight = lineCount * lineHeight + 30;
  int panelX = (clientRect->right - panelWidth) / 2;
  int panelY = (clientRect->bottom - panelHeight) / 2;

  RECT panelRect = {panelX, panelY, panelX + panelWidth, panelY + panelHeight};
  HBRUSH bgBrush = CreateSolidBrush(RGB(25, 25, 30));
  FillRect(hdc, &panelRect, bgBrush);
  DeleteObject(bgBrush);

  HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(70, 70, 80));
  HPEN oldPen = SelectObject(hdc, borderPen);
  Rectangle(hdc, panelRect.left, panelRect.top, panelRect.right,
            panelRect.bottom);
  SelectObject(hdc, oldPen);
  DeleteObject(borderPen);

  HFONT font =
      CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  FIXED_PITCH | FF_MODERN, "Consolas");
  HFONT boldFont =
      CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, "Segoe UI");
  HFONT oldFont = SelectObject(hdc, font);

  SetBkMode(hdc, TRANSPARENT);

  int y = panelY + 15;
  for (int i = 0; i < lineCount; i++) {
    if (i == 0) {
      SelectObject(hdc, boldFont);
      SetTextColor(hdc, g_accentColor);
    } else {
      SelectObject(hdc, font);
      SetTextColor(hdc, RGB(150, 150, 160));
    }
    TextOutA(hdc, panelX + 20, y, helpLines[i], (int)strlen(helpLines[i]));
    y += lineHeight;
  }

  SelectObject(hdc, oldFont);
  DeleteObject(font);
  DeleteObject(boldFont);
}

// draws settings overlay panel
void DrawSettingsOverlay(HDC hdc, RECT *clientRect) {
  if (!g_showSettings)
    return;

  int lineHeight = 24;
  int panelWidth = 320;
  int panelHeight = 180;
  int panelX = (clientRect->right - panelWidth) / 2;
  int panelY = (clientRect->bottom - panelHeight) / 2;

  RECT panelRect = {panelX, panelY, panelX + panelWidth, panelY + panelHeight};
  HBRUSH bgBrush = CreateSolidBrush(RGB(25, 25, 30));
  FillRect(hdc, &panelRect, bgBrush);
  DeleteObject(bgBrush);

  HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(70, 70, 80));
  HPEN oldPen = SelectObject(hdc, borderPen);
  Rectangle(hdc, panelRect.left, panelRect.top, panelRect.right,
            panelRect.bottom);
  SelectObject(hdc, oldPen);
  DeleteObject(borderPen);

  HFONT font =
      CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  FIXED_PITCH | FF_MODERN, "Consolas");
  HFONT boldFont =
      CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, "Segoe UI");
  HFONT oldFont = SelectObject(hdc, font);

  SetBkMode(hdc, TRANSPARENT);

  int y = panelY + 15;

  SelectObject(hdc, boldFont);
  SetTextColor(hdc, g_accentColor);
  TextOutA(hdc, panelX + 20, y, "pix settings", 12);
  y += lineHeight + 5;

  SelectObject(hdc, font);
  SetTextColor(hdc, RGB(150, 150, 160));

  char line1[64];
  snprintf(line1, sizeof(line1), "[M] Max size: %dK",
           g_settings.maxImageSize / 1024);
  TextOutA(hdc, panelX + 20, y, line1, (int)strlen(line1));
  y += lineHeight;

  char line2[64];
  if (g_settings.cpuThreads == 0) {
    snprintf(line2, sizeof(line2), "[T] CPU threads: auto (all cores)");
  } else if (g_settings.cpuThreads > 8) {
    snprintf(line2, sizeof(line2), "[T] CPU threads: %d (high!)",
             g_settings.cpuThreads);
  } else {
    snprintf(line2, sizeof(line2), "[T] CPU threads: %d",
             g_settings.cpuThreads);
  }
  TextOutA(hdc, panelX + 20, y, line2, (int)strlen(line2));
  y += lineHeight;

  char line3[64];
  snprintf(line3, sizeof(line3), "[W] Large op warnings: %s",
           g_settings.showWarnings ? "on" : "off");
  TextOutA(hdc, panelX + 20, y, line3, (int)strlen(line3));
  y += lineHeight + 10;

  SetTextColor(hdc, RGB(90, 90, 100));
  TextOutA(hdc, panelX + 20, y, "press key to change, ESC to close", 34);

  SelectObject(hdc, oldFont);
  DeleteObject(font);
  DeleteObject(boldFont);
}

// draws edit panel with brightness/contrast/saturation sliders
void DrawEditPanel(HDC hdc, RECT *clientRect) {
  if (!g_showEditPanel)
    return;

  int panelX = clientRect->right - EDIT_PANEL_WIDTH;
  int panelY = 50;
  int panelH = 280;

  RECT panelRect = {panelX, panelY, clientRect->right, panelY + panelH};
  HBRUSH panelBrush = CreateSolidBrush(RGB(35, 35, 38));
  FillRect(hdc, &panelRect, panelBrush);
  DeleteObject(panelBrush);

  HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(60, 60, 65));
  HPEN oldPen = SelectObject(hdc, borderPen);
  Rectangle(hdc, panelX, panelY, clientRect->right, panelY + panelH);
  SelectObject(hdc, oldPen);
  DeleteObject(borderPen);

  HFONT font =
      CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, "Segoe UI");
  HFONT boldFont =
      CreateFontA(15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                  DEFAULT_PITCH | FF_SWISS, "Segoe UI");
  HFONT oldFont = SelectObject(hdc, boldFont);
  SetBkMode(hdc, TRANSPARENT);

  int x = panelX + 15;
  int y = panelY + 15;
  int lineH = 28;
  int sliderW = EDIT_PANEL_WIDTH - 30;
  int sliderH = 6;

  SetTextColor(hdc, RGB(240, 240, 240));
  TextOutA(hdc, x, y, "Edit Image", 10);
  y += lineH + 5;

  SelectObject(hdc, font);

  // brightness
  SetTextColor(hdc, g_editSelection == 0 ? g_accentColor : g_textColor);
  char bStr[32];
  snprintf(bStr, sizeof(bStr), "Brightness: %d", g_editBrightness);
  TextOutA(hdc, x, y, bStr, (int)strlen(bStr));
  y += 20;

  RECT trackRect = {x, y, x + sliderW, y + sliderH};
  HBRUSH trackBrush = CreateSolidBrush(RGB(60, 60, 65));
  FillRect(hdc, &trackRect, trackBrush);
  DeleteObject(trackBrush);

  int bFill = (g_editBrightness + 100) * sliderW / 200;
  RECT fillRect = {x, y, x + bFill, y + sliderH};
  HBRUSH fillBrush = CreateSolidBrush(g_accentColor);
  FillRect(hdc, &fillRect, fillBrush);
  DeleteObject(fillBrush);
  y += lineH;

  // contrast
  SetTextColor(hdc, g_editSelection == 1 ? g_accentColor : g_textColor);
  char cStr[32];
  snprintf(cStr, sizeof(cStr), "Contrast: %.1f", g_editContrast);
  TextOutA(hdc, x, y, cStr, (int)strlen(cStr));
  y += 20;

  trackRect.top = y;
  trackRect.bottom = y + sliderH;
  trackBrush = CreateSolidBrush(RGB(60, 60, 65));
  FillRect(hdc, &trackRect, trackBrush);
  DeleteObject(trackBrush);

  int cFill = (int)((g_editContrast - 0.5f) * sliderW / 1.5f);
  fillRect.left = x;
  fillRect.right = x + cFill;
  fillRect.top = y;
  fillRect.bottom = y + sliderH;
  fillBrush = CreateSolidBrush(g_accentColor);
  FillRect(hdc, &fillRect, fillBrush);
  DeleteObject(fillBrush);
  y += lineH;

  // saturation
  SetTextColor(hdc, g_editSelection == 2 ? g_accentColor : g_textColor);
  char sStr[32];
  snprintf(sStr, sizeof(sStr), "Saturation: %.1f", g_editSaturation);
  TextOutA(hdc, x, y, sStr, (int)strlen(sStr));
  y += 20;

  trackRect.top = y;
  trackRect.bottom = y + sliderH;
  trackBrush = CreateSolidBrush(RGB(60, 60, 65));
  FillRect(hdc, &trackRect, trackBrush);
  DeleteObject(trackBrush);

  int sFill = (int)(g_editSaturation * sliderW / 2.0f);
  fillRect.left = x;
  fillRect.right = x + sFill;
  fillRect.top = y;
  fillRect.bottom = y + sliderH;
  fillBrush = CreateSolidBrush(g_accentColor);
  FillRect(hdc, &fillRect, fillBrush);
  DeleteObject(fillBrush);
  y += lineH + 10;

  SetTextColor(hdc, RGB(140, 140, 145));
  TextOutA(hdc, x, y, "Up/Down: Select", 15);
  y += 18;
  TextOutA(hdc, x, y, "Left/Right: Adjust", 18);
  y += 18;
  TextOutA(hdc, x, y, "Enter: Apply | Esc: Cancel", 26);

  SelectObject(hdc, oldFont);
  DeleteObject(font);
  DeleteObject(boldFont);
}

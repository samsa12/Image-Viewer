// ui.h - ui drawing functions for pix
// extracted from main.c for better organization

#ifndef UI_H
#define UI_H

#include "app_state.h"
#include "settings.h"
#include <windows.h>

// draw functions
void DrawInfoPanel(HDC hdc, RECT *clientRect);
void DrawThumbnailStrip(HWND hwnd, HDC hdc, RECT *clientRect);
void DrawStatusBar(HDC hdc, RECT *clientRect);
void DrawImageShadow(HDC hdc, int x, int y, int w, int h);
void DrawSlideshowProgress(HDC hdc, RECT *clientRect);
void DrawZoomOverlay(HDC hdc, RECT *clientRect);
void DrawHelpOverlay(HDC hdc, RECT *clientRect);
void DrawSettingsOverlay(HDC hdc, RECT *clientRect);
void DrawEditPanel(HDC hdc, RECT *clientRect);

#endif

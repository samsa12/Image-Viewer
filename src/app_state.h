/*
 * App State - Header
 * Shared state declarations for Pure C Image Viewer
 */

#ifndef APP_STATE_H
#define APP_STATE_H

#include "file_browser.h"
#include "image_loader.h"
#include "renderer.h"
#include <windows.h>


// Global state - defined in main.c
extern ImageData g_image;
extern Renderer g_renderer;
extern FileBrowser g_browser;
extern BOOL g_fullscreen;
extern BOOL g_showInfo;
extern BOOL g_darkTheme;
extern BOOL g_showThumbnails;
extern BOOL g_showStatusBar;
extern BOOL g_showEditPanel;
extern BOOL g_slideshowActive;
extern int g_slideshowInterval;
extern DWORD g_slideshowStartTime;

// Edit mode state
extern int g_editBrightness;
extern float g_editContrast;
extern float g_editSaturation;
extern int g_editSelection;

// Theme colors
extern COLORREF g_bgColor;
extern COLORREF g_textColor;
extern COLORREF g_panelBgColor;
extern COLORREF g_accentColor;
extern COLORREF g_statusBarColor;

// UI constants
#define THUMB_SIZE 80
#define THUMB_PADDING 5
#define THUMB_STRIP_HEIGHT (THUMB_SIZE + THUMB_PADDING * 2)
#define STATUS_BAR_HEIGHT 28
#define SHADOW_SIZE 8
#define EDIT_PANEL_WIDTH 200

#endif // APP_STATE_H

/*
 * pix - settings
 * user preferences and resource controls
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <windows.h>

// Settings structure for user preferences
typedef struct {
  int maxImageSize;    // max allowed size for upscaling (8192, 16384, 32768)
  int cpuThreads;      // 0 = auto (all cores), 1-32 = specific count
  int maxMemoryMB;     // 0 = unlimited, or cap in MB
  BOOL prefetchImages; // preload next/prev images in background
  BOOL showWarnings;   // warn before large memory operations
} Settings;

// Global settings instance
extern Settings g_settings;

// Functions
void Settings_SetDefaults(Settings *s);
void Settings_Load(Settings *s);
void Settings_Save(Settings *s);
void Settings_ApplyThreads(Settings *s);
int Settings_CycleMaxSize(Settings *s);
int Settings_CycleThreads(Settings *s);

// Memory estimation helper
size_t Settings_EstimateMemory(int width, int height);
BOOL Settings_WarnIfLarge(HWND hwnd, size_t memBytes);

#endif // SETTINGS_H

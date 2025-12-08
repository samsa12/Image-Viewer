/*
 * pix - settings implementation
 * INI file load/save and resource controls
 */

#include "settings.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global settings instance
Settings g_settings;

// Get path to pix.ini (same folder as exe)
static void GetIniPath(char *path, size_t pathSize) {
  GetModuleFileNameA(NULL, path, (DWORD)pathSize);
  // Replace .exe with .ini
  char *dot = strrchr(path, '.');
  if (dot) {
    strcpy(dot, ".ini");
  } else {
    strcat(path, ".ini");
  }
}

void Settings_SetDefaults(Settings *s) {
  s->maxImageSize = 8192;    // default: 8K limit
  s->cpuThreads = 0;         // 0 = auto (use all cores)
  s->maxMemoryMB = 0;        // 0 = unlimited
  s->prefetchImages = FALSE; // disabled by default
  s->showWarnings = TRUE;    // warn for large ops
}

void Settings_Load(Settings *s) {
  Settings_SetDefaults(s);

  char iniPath[MAX_PATH];
  GetIniPath(iniPath, sizeof(iniPath));

  // Check if file exists
  FILE *f = fopen(iniPath, "r");
  if (!f) {
    // Create default ini file
    Settings_Save(s);
    return;
  }

  char line[256];
  while (fgets(line, sizeof(line), f)) {
    // Skip comments and empty lines
    if (line[0] == ';' || line[0] == '#' || line[0] == '\n' || line[0] == '\r')
      continue;

    char key[64], value[64];
    if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
      // Trim whitespace from key
      char *k = key;
      while (*k == ' ')
        k++;
      char *end = k + strlen(k) - 1;
      while (end > k && *end == ' ')
        *end-- = '\0';

      if (strcmp(k, "maxImageSize") == 0) {
        s->maxImageSize = atoi(value);
        // Clamp to valid values
        if (s->maxImageSize < 4096)
          s->maxImageSize = 4096;
        if (s->maxImageSize > 32768)
          s->maxImageSize = 32768;
      } else if (strcmp(k, "cpuThreads") == 0) {
        s->cpuThreads = atoi(value);
        if (s->cpuThreads < 0)
          s->cpuThreads = 0;
        if (s->cpuThreads > 64)
          s->cpuThreads = 64;
      } else if (strcmp(k, "maxMemoryMB") == 0) {
        s->maxMemoryMB = atoi(value);
        if (s->maxMemoryMB < 0)
          s->maxMemoryMB = 0;
      } else if (strcmp(k, "prefetchImages") == 0) {
        s->prefetchImages = (atoi(value) != 0);
      } else if (strcmp(k, "showWarnings") == 0) {
        s->showWarnings = (atoi(value) != 0);
      }
    }
  }

  fclose(f);
}

void Settings_Save(Settings *s) {
  char iniPath[MAX_PATH];
  GetIniPath(iniPath, sizeof(iniPath));

  FILE *f = fopen(iniPath, "w");
  if (!f)
    return;

  fprintf(f, "; pix settings\n");
  fprintf(f, "; edit manually or use Ctrl+, in pix\n\n");
  fprintf(f, "[resources]\n");
  fprintf(f, "maxImageSize = %d\n", s->maxImageSize);
  fprintf(f, "cpuThreads = %d\n", s->cpuThreads);
  fprintf(f, "maxMemoryMB = %d\n", s->maxMemoryMB);
  fprintf(f, "\n[behavior]\n");
  fprintf(f, "prefetchImages = %d\n", s->prefetchImages);
  fprintf(f, "showWarnings = %d\n", s->showWarnings);

  fclose(f);
}

void Settings_ApplyThreads(Settings *s) {
  if (s->cpuThreads > 0) {
    omp_set_num_threads(s->cpuThreads);
  }
  // if 0, OpenMP uses default (all cores)
}

int Settings_CycleMaxSize(Settings *s) {
  switch (s->maxImageSize) {
  case 8192:
    s->maxImageSize = 16384;
    break;
  case 16384:
    s->maxImageSize = 32768;
    break;
  case 32768:
    s->maxImageSize = 8192;
    break;
  default:
    s->maxImageSize = 8192;
    break;
  }
  Settings_Save(s);
  return s->maxImageSize;
}

int Settings_CycleThreads(Settings *s) {
  // Fixed cycle: 0 (auto) -> 1 -> 2 -> 4 -> 8 -> 16 -> 32 -> 0
  switch (s->cpuThreads) {
  case 0:
    s->cpuThreads = 1;
    break;
  case 1:
    s->cpuThreads = 2;
    break;
  case 2:
    s->cpuThreads = 4;
    break;
  case 4:
    s->cpuThreads = 8;
    break;
  case 8:
    s->cpuThreads = 16;
    break;
  case 16:
    s->cpuThreads = 32;
    break;
  default:
    s->cpuThreads = 0;
    break; // back to auto
  }
  Settings_Save(s);
  Settings_ApplyThreads(s);
  return s->cpuThreads;
}

size_t Settings_EstimateMemory(int width, int height) {
  // RGBA pixels + working buffer for operations
  return (size_t)width * height * 4 * 2;
}

BOOL Settings_WarnIfLarge(HWND hwnd, size_t memBytes) {
  if (!g_settings.showWarnings)
    return TRUE; // proceed without warning

  // Warn if > 500MB
  if (memBytes > 500 * 1024 * 1024) {
    char msg[256];
    snprintf(msg, sizeof(msg),
             "This operation will use approximately %d MB of RAM.\n\n"
             "Continue?",
             (int)(memBytes / (1024 * 1024)));

    int result = MessageBoxA(hwnd, msg, "pix - Large Operation",
                             MB_YESNO | MB_ICONWARNING);

    return (result == IDYES);
  }

  return TRUE;
}

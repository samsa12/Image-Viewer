/*
 * File Browser - Implementation
 * pix - file browser
 */

#include "file_browser.h"
#include <commdlg.h>
#include <stdio.h>
#include <string.h>


// Supported image extensions
static const char *g_imageExtensions[] = {".jpg", ".jpeg", ".png", ".bmp",
                                          ".gif", ".tga",  ".psd", ".hdr",
                                          ".pic", ".pnm",  NULL};

void FileBrowser_Init(FileBrowser *browser) {
  browser->fileCount = 0;
  browser->currentIndex = -1;
  browser->currentDir[0] = '\0';
}

int FileBrowser_IsImageFile(const char *filename) {
  const char *ext = strrchr(filename, '.');
  if (!ext)
    return 0;

  // Convert to lowercase for comparison
  char lowerExt[16] = {0};
  for (int i = 0; ext[i] && i < 15; i++) {
    lowerExt[i] = (ext[i] >= 'A' && ext[i] <= 'Z') ? ext[i] + 32 : ext[i];
  }

  for (int i = 0; g_imageExtensions[i]; i++) {
    if (strcmp(lowerExt, g_imageExtensions[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

int FileBrowser_OpenDialog(FileBrowser *browser, HWND hwnd) {
  char filename[MAX_PATH] = {0};

  OPENFILENAMEA ofn = {0};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd;
  ofn.lpstrFilter =
      "Image Files\0*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tga;*.psd;*.hdr\0"
      "All Files\0*.*\0";
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

  if (GetOpenFileNameA(&ofn)) {
    return FileBrowser_LoadDirectory(browser, filename);
  }
  return 0;
}

int FileBrowser_LoadDirectory(FileBrowser *browser, const char *filepath) {
  if (!filepath)
    return 0;

  // Extract directory from filepath
  char dir[MAX_PATH];
  strncpy(dir, filepath, MAX_PATH - 1);

  char *lastSlash = strrchr(dir, '\\');
  if (!lastSlash)
    lastSlash = strrchr(dir, '/');

  if (lastSlash) {
    *lastSlash = '\0';
  } else {
    strcpy(dir, ".");
  }

  strncpy(browser->currentDir, dir, MAX_PATH - 1);

  // Clear file list
  browser->fileCount = 0;
  browser->currentIndex = -1;

  // Build search pattern
  char searchPattern[MAX_PATH];
  snprintf(searchPattern, MAX_PATH, "%s\\*.*", dir);

  // Find all image files in directory
  WIN32_FIND_DATAA findData;
  HANDLE hFind = FindFirstFileA(searchPattern, &findData);

  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        if (FileBrowser_IsImageFile(findData.cFileName)) {
          if (browser->fileCount < MAX_FILES) {
            snprintf(browser->files[browser->fileCount], MAX_PATH, "%s\\%s",
                     dir, findData.cFileName);
            browser->fileCount++;
          }
        }
      }
    } while (FindNextFileA(hFind, &findData) && browser->fileCount < MAX_FILES);

    FindClose(hFind);
  }

  // Find the index of the originally selected file
  if (browser->fileCount > 0) {
    for (int i = 0; i < browser->fileCount; i++) {
      if (_stricmp(browser->files[i], filepath) == 0) {
        browser->currentIndex = i;
        break;
      }
    }
    if (browser->currentIndex == -1) {
      browser->currentIndex = 0;
    }
  }

  return browser->fileCount > 0;
}

const char *FileBrowser_GetCurrent(FileBrowser *browser) {
  if (browser->currentIndex >= 0 &&
      browser->currentIndex < browser->fileCount) {
    return browser->files[browser->currentIndex];
  }
  return NULL;
}

const char *FileBrowser_Next(FileBrowser *browser) {
  if (browser->fileCount == 0)
    return NULL;

  browser->currentIndex++;
  if (browser->currentIndex >= browser->fileCount) {
    browser->currentIndex = 0; // Wrap around
  }
  return FileBrowser_GetCurrent(browser);
}

const char *FileBrowser_Previous(FileBrowser *browser) {
  if (browser->fileCount == 0)
    return NULL;

  browser->currentIndex--;
  if (browser->currentIndex < 0) {
    browser->currentIndex = browser->fileCount - 1; // Wrap around
  }
  return FileBrowser_GetCurrent(browser);
}

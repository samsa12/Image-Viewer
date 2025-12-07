/*
 * File Browser - Header
 * Pure C Image Viewer
 */

#ifndef FILE_BROWSER_H
#define FILE_BROWSER_H

#include <windows.h>

#define MAX_FILES 10000

// File browser state
typedef struct {
    char files[MAX_FILES][MAX_PATH];
    int fileCount;
    int currentIndex;
    char currentDir[MAX_PATH];
} FileBrowser;

// Function declarations
void FileBrowser_Init(FileBrowser* browser);
int FileBrowser_OpenDialog(FileBrowser* browser, HWND hwnd);
int FileBrowser_LoadDirectory(FileBrowser* browser, const char* filepath);
const char* FileBrowser_GetCurrent(FileBrowser* browser);
const char* FileBrowser_Next(FileBrowser* browser);
const char* FileBrowser_Previous(FileBrowser* browser);
int FileBrowser_IsImageFile(const char* filename);

#endif // FILE_BROWSER_H

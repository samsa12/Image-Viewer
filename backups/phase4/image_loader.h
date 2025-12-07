/*
 * Image Loader - Header
 * Pure C Image Viewer
 */

#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <windows.h>

// Image data structure
typedef struct {
  unsigned char *pixels; // RGBA pixel data
  int width;
  int height;
  int channels;
  char filepath[MAX_PATH];
} ImageData;

// Function declarations
int ImageLoader_Load(const char *filepath, ImageData *image);
void ImageLoader_Free(ImageData *image);
const char *ImageLoader_GetError(void);
void ImageLoader_RotateRight(ImageData *image);
void ImageLoader_RotateLeft(ImageData *image);
void ImageLoader_FlipHorizontal(ImageData *image);
void ImageLoader_FlipVertical(ImageData *image);

#endif // IMAGE_LOADER_H

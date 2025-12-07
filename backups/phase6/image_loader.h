/*
 * Image Loader - Header
 * Pure C Image Viewer
 */

#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <windows.h>

// Maximum GIF frames to support
#define MAX_GIF_FRAMES 500

// Image data structure
typedef struct {
  unsigned char *pixels; // Current frame RGBA pixel data
  int width;
  int height;
  int channels;
  char filepath[MAX_PATH];

  // Animation data (for GIFs)
  int isAnimated;
  int frameCount;
  int currentFrame;
  int *frameDelays;       // Delay for each frame in milliseconds
  unsigned char **frames; // Array of frame pixel data
} ImageData;

// Function declarations
int ImageLoader_Load(const char *filepath, ImageData *image);
void ImageLoader_Free(ImageData *image);
const char *ImageLoader_GetError(void);
void ImageLoader_RotateRight(ImageData *image);
void ImageLoader_RotateLeft(ImageData *image);
void ImageLoader_FlipHorizontal(ImageData *image);
void ImageLoader_FlipVertical(ImageData *image);
int ImageLoader_NextFrame(ImageData *image);
int ImageLoader_GetFrameDelay(ImageData *image);
void ImageLoader_AdjustBrightness(ImageData *image, int delta);
void ImageLoader_AdjustContrast(ImageData *image, float factor);

#endif // IMAGE_LOADER_H

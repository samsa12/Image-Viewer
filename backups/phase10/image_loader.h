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

// Editing functions
void ImageLoader_AdjustBrightness(ImageData *image, int delta);
void ImageLoader_AdjustContrast(ImageData *image, float factor);
void ImageLoader_AdjustSaturation(ImageData *image, float factor);
void ImageLoader_Grayscale(ImageData *image);
void ImageLoader_Crop(ImageData *image, int x, int y, int w, int h);
void ImageLoader_Invert(ImageData *image);
void ImageLoader_Resize(ImageData *image, int newWidth, int newHeight);
void ImageLoader_Sharpen(ImageData *image);
void ImageLoader_Blur(ImageData *image);
void ImageLoader_AutoLevels(ImageData *image);
void ImageLoader_Sepia(ImageData *image);

#endif // IMAGE_LOADER_H

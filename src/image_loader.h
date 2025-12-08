// image loader header
// handles loading images, editing, and undo

#ifndef IMAGE_LOADER_H
#define IMAGE_LOADER_H

#include <stdio.h>
#include <windows.h>

// gif stuff
#define MAX_GIF_FRAMES 500

// EXIF metadata from camera
typedef struct {
  char camera[64];      // camera make/model
  char dateTime[32];    // date taken
  char exposure[32];    // shutter speed
  char aperture[16];    // f-stop
  char iso[16];         // ISO value
  char focalLength[16]; // focal length mm
  int hasExif;          // 1 if exif data was found
} ExifData;

// main image data struct
typedef struct {
  unsigned char *pixels;   // current frame rgba data
  unsigned char *original; // original from disk for reset
  unsigned char *undo;     // previous state for ctrl+z
  int width;
  int height;
  int channels;
  char filepath[MAX_PATH];

  // EXIF metadata
  ExifData exif;

  // gif animation
  int isAnimated;
  int frameCount;
  int currentFrame;
  int *frameDelays;       // delay per frame in ms
  unsigned char **frames; // frame pixel data array
} ImageData;

// loading
int ImageLoader_Load(const char *filepath, ImageData *image);
void ImageLoader_Free(ImageData *image);
const char *ImageLoader_GetError(void);

// transforms
void ImageLoader_RotateRight(ImageData *image);
void ImageLoader_RotateLeft(ImageData *image);
void ImageLoader_FlipHorizontal(ImageData *image);
void ImageLoader_FlipVertical(ImageData *image);

// gif animation
int ImageLoader_NextFrame(ImageData *image);
int ImageLoader_GetFrameDelay(ImageData *image);

// undo system
void ImageLoader_SaveUndo(ImageData *image);
int ImageLoader_Undo(ImageData *image);
int ImageLoader_Reset(ImageData *image);

// editing
void ImageLoader_AdjustBrightness(ImageData *image, int delta);
void ImageLoader_AdjustContrast(ImageData *image, float factor);
void ImageLoader_AdjustSaturation(ImageData *image, float factor);
void ImageLoader_Grayscale(ImageData *image);
void ImageLoader_Crop(ImageData *image, int x, int y, int w, int h);
void ImageLoader_Invert(ImageData *image);
void ImageLoader_Resize(ImageData *image, int newWidth, int newHeight);
void ImageLoader_ResizeLanczos(ImageData *image, int newWidth, int newHeight);
void ImageLoader_Sharpen(ImageData *image);
void ImageLoader_Blur(ImageData *image);
void ImageLoader_AutoLevels(ImageData *image);
void ImageLoader_Sepia(ImageData *image);

#endif

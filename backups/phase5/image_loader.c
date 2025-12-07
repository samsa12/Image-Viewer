/*
 * Image Loader - Implementation
 * Pure C Image Viewer
 */

#define STB_IMAGE_IMPLEMENTATION
#include "image_loader.h"
#include "../lib/stb_image.h"
#include <stdlib.h>
#include <string.h>


static char g_lastError[256] = {0};

int ImageLoader_Load(const char *filepath, ImageData *image) {
  if (!filepath || !image) {
    strcpy(g_lastError, "Invalid parameters");
    return 0;
  }

  // Clear previous image data
  memset(image, 0, sizeof(ImageData));

  // Load image using stb_image (force RGBA)
  image->pixels =
      stbi_load(filepath, &image->width, &image->height, &image->channels, 4);

  if (!image->pixels) {
    snprintf(g_lastError, sizeof(g_lastError), "Failed to load: %s",
             stbi_failure_reason());
    return 0;
  }

  // Store filepath
  strncpy(image->filepath, filepath, MAX_PATH - 1);
  image->channels = 4; // We forced RGBA

  return 1;
}

void ImageLoader_Free(ImageData *image) {
  if (image && image->pixels) {
    stbi_image_free(image->pixels);
    image->pixels = NULL;
    image->width = 0;
    image->height = 0;
    image->channels = 0;
    image->filepath[0] = '\0';
  }
}

const char *ImageLoader_GetError(void) { return g_lastError; }

void ImageLoader_RotateRight(ImageData *image) {
  if (!image || !image->pixels)
    return;

  int oldW = image->width;
  int oldH = image->height;
  int newW = oldH;
  int newH = oldW;

  unsigned char *newPixels = (unsigned char *)malloc(newW * newH * 4);
  if (!newPixels)
    return;

  // Rotate 90 degrees clockwise
  for (int y = 0; y < oldH; y++) {
    for (int x = 0; x < oldW; x++) {
      int srcIdx = (y * oldW + x) * 4;
      int dstX = oldH - 1 - y;
      int dstY = x;
      int dstIdx = (dstY * newW + dstX) * 4;

      newPixels[dstIdx + 0] = image->pixels[srcIdx + 0];
      newPixels[dstIdx + 1] = image->pixels[srcIdx + 1];
      newPixels[dstIdx + 2] = image->pixels[srcIdx + 2];
      newPixels[dstIdx + 3] = image->pixels[srcIdx + 3];
    }
  }

  free(image->pixels);
  image->pixels = newPixels;
  image->width = newW;
  image->height = newH;
}

void ImageLoader_RotateLeft(ImageData *image) {
  if (!image || !image->pixels)
    return;

  int oldW = image->width;
  int oldH = image->height;
  int newW = oldH;
  int newH = oldW;

  unsigned char *newPixels = (unsigned char *)malloc(newW * newH * 4);
  if (!newPixels)
    return;

  // Rotate 90 degrees counter-clockwise
  for (int y = 0; y < oldH; y++) {
    for (int x = 0; x < oldW; x++) {
      int srcIdx = (y * oldW + x) * 4;
      int dstX = y;
      int dstY = oldW - 1 - x;
      int dstIdx = (dstY * newW + dstX) * 4;

      newPixels[dstIdx + 0] = image->pixels[srcIdx + 0];
      newPixels[dstIdx + 1] = image->pixels[srcIdx + 1];
      newPixels[dstIdx + 2] = image->pixels[srcIdx + 2];
      newPixels[dstIdx + 3] = image->pixels[srcIdx + 3];
    }
  }

  free(image->pixels);
  image->pixels = newPixels;
  image->width = newW;
  image->height = newH;
}

void ImageLoader_FlipHorizontal(ImageData *image) {
  if (!image || !image->pixels)
    return;

  int w = image->width;
  int h = image->height;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w / 2; x++) {
      int leftIdx = (y * w + x) * 4;
      int rightIdx = (y * w + (w - 1 - x)) * 4;

      // Swap pixels
      for (int c = 0; c < 4; c++) {
        unsigned char tmp = image->pixels[leftIdx + c];
        image->pixels[leftIdx + c] = image->pixels[rightIdx + c];
        image->pixels[rightIdx + c] = tmp;
      }
    }
  }
}

void ImageLoader_FlipVertical(ImageData *image) {
  if (!image || !image->pixels)
    return;

  int w = image->width;
  int h = image->height;
  int rowSize = w * 4;

  unsigned char *tempRow = (unsigned char *)malloc(rowSize);
  if (!tempRow)
    return;

  for (int y = 0; y < h / 2; y++) {
    unsigned char *topRow = image->pixels + y * rowSize;
    unsigned char *bottomRow = image->pixels + (h - 1 - y) * rowSize;

    memcpy(tempRow, topRow, rowSize);
    memcpy(topRow, bottomRow, rowSize);
    memcpy(bottomRow, tempRow, rowSize);
  }

  free(tempRow);
}

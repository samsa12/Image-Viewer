/*
 * Image Loader - Implementation
 * pix - image loader
 */

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "image_loader.h"
#include "../lib/stb_image.h"
#include "../lib/stb_image_write.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_lastError[256] = {0};

// Helper to check if file is GIF
static int IsGifFile(const char *filepath) {
  const char *ext = strrchr(filepath, '.');
  if (ext && (_stricmp(ext, ".gif") == 0)) {
    return 1;
  }
  return 0;
}

// Helper to load entire file into memory
static unsigned char *LoadFileToMemory(const char *filepath, int *outSize) {
  FILE *f = fopen(filepath, "rb");
  if (!f)
    return NULL;

  fseek(f, 0, SEEK_END);
  int size = ftell(f);
  fseek(f, 0, SEEK_SET);

  unsigned char *buffer = (unsigned char *)malloc(size);
  if (buffer) {
    fread(buffer, 1, size, f);
    *outSize = size;
  }
  fclose(f);
  return buffer;
}

// Parse EXIF data from JPEG file
static void ParseExifData(const char *filepath, ExifData *exif) {
  memset(exif, 0, sizeof(ExifData));

  FILE *f = fopen(filepath, "rb");
  if (!f)
    return;

  // Check JPEG magic
  unsigned char header[2];
  if (fread(header, 1, 2, f) != 2 || header[0] != 0xFF || header[1] != 0xD8) {
    fclose(f);
    return; // Not a JPEG
  }

  // Look for APP1 marker (EXIF)
  while (!feof(f)) {
    unsigned char marker[2];
    if (fread(marker, 1, 2, f) != 2)
      break;

    if (marker[0] != 0xFF)
      break;

    // Read segment length
    unsigned char lenBytes[2];
    if (fread(lenBytes, 1, 2, f) != 2)
      break;
    int segLen = (lenBytes[0] << 8) | lenBytes[1];

    if (marker[1] == 0xE1) { // APP1 = EXIF
      // Read EXIF header
      char exifHeader[6];
      if (fread(exifHeader, 1, 6, f) != 6)
        break;

      if (memcmp(exifHeader, "Exif\0\0", 6) != 0) {
        fseek(f, segLen - 8, SEEK_CUR);
        continue;
      }

      // Read TIFF header
      long tiffStart = ftell(f);
      unsigned char tiffHeader[8];
      if (fread(tiffHeader, 1, 8, f) != 8)
        break;

      int littleEndian = (tiffHeader[0] == 'I' && tiffHeader[1] == 'I');

// Helper macros for endianness
#define READ16(p)                                                              \
  (littleEndian ? ((p)[0] | ((p)[1] << 8)) : (((p)[0] << 8) | (p)[1]))
#define READ32(p)                                                              \
  (littleEndian ? ((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((p)[3] << 24))   \
                : (((p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3]))

      int ifdOffset = READ32(tiffHeader + 4);
      fseek(f, tiffStart + ifdOffset, SEEK_SET);

      unsigned char countBytes[2];
      if (fread(countBytes, 1, 2, f) != 2)
        break;
      int numEntries = READ16(countBytes);

      char make[32] = {0};
      char model[32] = {0};

      for (int i = 0; i < numEntries; i++) {
        unsigned char entry[12];
        if (fread(entry, 1, 12, f) != 12)
          break;

        int tag = READ16(entry);
        int type = READ16(entry + 2);
        int count = READ32(entry + 4);
        int valueOffset = READ32(entry + 8);

        long savePos = ftell(f);

        // Read string value
        if (type == 2 && count < 64) { // ASCII string
          long strOffset =
              (count > 4) ? (tiffStart + valueOffset) : (savePos - 4);
          fseek(f, strOffset, SEEK_SET);
          char strVal[64] = {0};
          fread(strVal, 1, count - 1, f);

          if (tag == 0x010F)
            strncpy(make, strVal, 31); // Make
          else if (tag == 0x0110)
            strncpy(model, strVal, 31); // Model
          else if (tag == 0x0132)
            strncpy(exif->dateTime, strVal, 31); // DateTime
          else if (tag == 0x9003)
            strncpy(exif->dateTime, strVal, 31); // DateTimeOriginal
        }

        // Read EXIF sub-IFD for more details
        if (tag == 0x8769) { // ExifIFDPointer
          fseek(f, tiffStart + valueOffset, SEEK_SET);
          unsigned char subCount[2];
          if (fread(subCount, 1, 2, f) == 2) {
            int subEntries = READ16(subCount);
            for (int j = 0; j < subEntries; j++) {
              unsigned char subEntry[12];
              if (fread(subEntry, 1, 12, f) != 12)
                break;

              int subTag = READ16(subEntry);
              int subType = READ16(subEntry + 2);
              (void)READ32(subEntry + 4); // subCount unused
              int subValue = READ32(subEntry + 8);

              long subSave = ftell(f);

              if (subTag == 0x829A && subType == 5) { // ExposureTime (rational)
                fseek(f, tiffStart + subValue, SEEK_SET);
                unsigned char rat[8];
                if (fread(rat, 1, 8, f) == 8) {
                  int num = READ32(rat);
                  int den = READ32(rat + 4);
                  if (den > 0) {
                    if (num == 1)
                      snprintf(exif->exposure, 31, "1/%d", den);
                    else
                      snprintf(exif->exposure, 31, "%d/%d", num, den);
                  }
                }
              } else if (subTag == 0x829D && subType == 5) { // FNumber
                fseek(f, tiffStart + subValue, SEEK_SET);
                unsigned char rat[8];
                if (fread(rat, 1, 8, f) == 8) {
                  int num = READ32(rat);
                  int den = READ32(rat + 4);
                  if (den > 0)
                    snprintf(exif->aperture, 15, "f/%.1f", (float)num / den);
                }
              } else if (subTag == 0x8827) { // ISO
                snprintf(exif->iso, 15, "%d", subValue);
              } else if (subTag == 0x920A && subType == 5) { // FocalLength
                fseek(f, tiffStart + subValue, SEEK_SET);
                unsigned char rat[8];
                if (fread(rat, 1, 8, f) == 8) {
                  int num = READ32(rat);
                  int den = READ32(rat + 4);
                  if (den > 0)
                    snprintf(exif->focalLength, 15, "%dmm", num / den);
                }
              } else if (subTag == 0x9003 && subType == 2) { // DateTimeOriginal
                fseek(f, tiffStart + subValue, SEEK_SET);
                char dt[32] = {0};
                fread(dt, 1, 19, f);
                strncpy(exif->dateTime, dt, 31);
              }

              fseek(f, subSave, SEEK_SET);
            }
          }
        }

        fseek(f, savePos, SEEK_SET);
      }

      // Combine make and model
      if (make[0] && model[0]) {
        // Skip make in model if it starts with make
        if (strstr(model, make) == model)
          strncpy(exif->camera, model, 63);
        else
          snprintf(exif->camera, 63, "%s %s", make, model);
      } else if (model[0]) {
        strncpy(exif->camera, model, 63);
      } else if (make[0]) {
        strncpy(exif->camera, make, 63);
      }

      exif->hasExif =
          (exif->camera[0] || exif->dateTime[0] || exif->exposure[0]);

#undef READ16
#undef READ32
      break;
    }

    // Skip to next segment
    fseek(f, segLen - 2, SEEK_CUR);
  }

  fclose(f);
}

int ImageLoader_Load(const char *filepath, ImageData *image) {
  if (!filepath || !image) {
    strcpy(g_lastError, "Invalid parameters");
    return 0;
  }

  // Clear previous image data
  memset(image, 0, sizeof(ImageData));

  // Check if GIF for animation
  if (IsGifFile(filepath)) {
    int fileSize;
    unsigned char *fileData = LoadFileToMemory(filepath, &fileSize);
    if (!fileData) {
      snprintf(g_lastError, sizeof(g_lastError), "Failed to read file");
      return 0;
    }

    int *delays = NULL;
    int width, height, frames, channels;

    unsigned char *gifData = stbi_load_gif_from_memory(
        fileData, fileSize, &delays, &width, &height, &frames, &channels, 4);

    free(fileData);

    if (gifData && frames > 1) {
      // Animated GIF
      image->width = width;
      image->height = height;
      image->channels = 4;
      image->isAnimated = 1;
      image->frameCount = frames;
      image->currentFrame = 0;

      // Allocate frame arrays
      image->frames =
          (unsigned char **)malloc(sizeof(unsigned char *) * frames);
      image->frameDelays = (int *)malloc(sizeof(int) * frames);

      int frameSize = width * height * 4;
      for (int i = 0; i < frames; i++) {
        image->frames[i] = (unsigned char *)malloc(frameSize);
        memcpy(image->frames[i], gifData + i * frameSize, frameSize);
        // GIF delays are in centiseconds, convert to milliseconds
        image->frameDelays[i] = delays ? (delays[i] * 10) : 100;
        if (image->frameDelays[i] < 20)
          image->frameDelays[i] = 100; // Min delay
      }

      // Set current pixels to first frame
      image->pixels = (unsigned char *)malloc(frameSize);
      memcpy(image->pixels, image->frames[0], frameSize);

      stbi_image_free(gifData);
      if (delays)
        stbi_image_free(delays);

      strncpy(image->filepath, filepath, MAX_PATH - 1);
      return 1;
    }

    // Not animated or failed - free and fall through to regular load
    if (gifData)
      stbi_image_free(gifData);
    if (delays)
      stbi_image_free(delays);
  }

  // Standard image load
  image->pixels =
      stbi_load(filepath, &image->width, &image->height, &image->channels, 4);

  if (!image->pixels) {
    snprintf(g_lastError, sizeof(g_lastError), "Failed to load: %s",
             stbi_failure_reason());
    return 0;
  }

  // no longer keeping original in ram - reset reloads from disk
  image->original = NULL;
  image->undo = NULL;

  // Store filepath
  strncpy(image->filepath, filepath, MAX_PATH - 1);
  image->channels = 4;
  image->isAnimated = 0;
  image->frameCount = 1;
  image->currentFrame = 0;

  // Parse EXIF data (for JPEGs)
  ParseExifData(filepath, &image->exif);

  return 1;
}

void ImageLoader_Free(ImageData *image) {
  if (!image)
    return;

  if (image->isAnimated && image->frames) {
    for (int i = 0; i < image->frameCount; i++) {
      if (image->frames[i])
        free(image->frames[i]);
    }
    free(image->frames);
    image->frames = NULL;
  }

  if (image->frameDelays) {
    free(image->frameDelays);
    image->frameDelays = NULL;
  }

  if (image->pixels) {
    if (!image->isAnimated) {
      stbi_image_free(image->pixels);
    } else {
      free(image->pixels);
    }
    image->pixels = NULL;
  }

  // Free undo buffers
  if (image->original) {
    free(image->original);
    image->original = NULL;
  }
  if (image->undo) {
    free(image->undo);
    image->undo = NULL;
  }

  image->width = 0;
  image->height = 0;
  image->channels = 0;
  image->filepath[0] = '\0';
  image->isAnimated = 0;
  image->frameCount = 0;
  image->currentFrame = 0;
}

const char *ImageLoader_GetError(void) { return g_lastError; }

int ImageLoader_NextFrame(ImageData *image) {
  if (!image || !image->isAnimated || image->frameCount <= 1)
    return 0;

  image->currentFrame = (image->currentFrame + 1) % image->frameCount;
  int frameSize = image->width * image->height * 4;
  memcpy(image->pixels, image->frames[image->currentFrame], frameSize);

  return 1;
}

int ImageLoader_GetFrameDelay(ImageData *image) {
  if (!image || !image->isAnimated || !image->frameDelays)
    return 100;
  return image->frameDelays[image->currentFrame];
}

void ImageLoader_SaveUndo(ImageData *image) {
  if (!image || !image->pixels)
    return;

  int pixelSize = image->width * image->height * 4;

  // Free old undo if exists
  if (image->undo) {
    free(image->undo);
  }

  // Copy current state to undo
  image->undo = (unsigned char *)malloc(pixelSize);
  if (image->undo) {
    memcpy(image->undo, image->pixels, pixelSize);
  }
}

int ImageLoader_Undo(ImageData *image) {
  if (!image || !image->undo)
    return 0;

  // Swap pixels and undo (so we can redo)
  unsigned char *temp = image->pixels;
  image->pixels = image->undo;
  image->undo = temp;

  return 1;
}

int ImageLoader_Reset(ImageData *image) {
  if (!image || !image->filepath[0])
    return 0;

  // save current as undo before reset
  ImageLoader_SaveUndo(image);

  // reload from disk (memory efficient - no need to keep original in ram)
  int w, h, c;
  unsigned char *fresh = stbi_load(image->filepath, &w, &h, &c, 4);
  if (!fresh)
    return 0;

  // free current pixels and replace
  if (image->pixels) {
    if (image->isAnimated) {
      free(image->pixels);
    } else {
      stbi_image_free(image->pixels);
    }
  }

  image->pixels = fresh;
  image->width = w;
  image->height = h;

  return 1;
}

void ImageLoader_RotateRight(ImageData *image) {
  if (!image || !image->pixels)
    return;

  ImageLoader_SaveUndo(image);

  int oldW = image->width;
  int oldH = image->height;
  int newW = oldH;
  int newH = oldW;

  unsigned char *newPixels = (unsigned char *)malloc(newW * newH * 4);
  if (!newPixels)
    return;

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

  ImageLoader_SaveUndo(image);

  int oldW = image->width;
  int oldH = image->height;
  int newW = oldH;
  int newH = oldW;

  unsigned char *newPixels = (unsigned char *)malloc(newW * newH * 4);
  if (!newPixels)
    return;

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

  ImageLoader_SaveUndo(image);

  int w = image->width;
  int h = image->height;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w / 2; x++) {
      int leftIdx = (y * w + x) * 4;
      int rightIdx = (y * w + (w - 1 - x)) * 4;

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

  ImageLoader_SaveUndo(image);

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

void ImageLoader_AdjustBrightness(ImageData *image, int delta) {
  if (!image || !image->pixels)
    return;

  int count = image->width * image->height * 4;
  for (int i = 0; i < count; i += 4) {
    for (int c = 0; c < 3; c++) {
      int val = image->pixels[i + c] + delta;
      if (val < 0)
        val = 0;
      if (val > 255)
        val = 255;
      image->pixels[i + c] = (unsigned char)val;
    }
  }
}

void ImageLoader_AdjustContrast(ImageData *image, float factor) {
  if (!image || !image->pixels)
    return;

  int count = image->width * image->height * 4;
  for (int i = 0; i < count; i += 4) {
    for (int c = 0; c < 3; c++) {
      float val = (image->pixels[i + c] - 128) * factor + 128;
      if (val < 0)
        val = 0;
      if (val > 255)
        val = 255;
      image->pixels[i + c] = (unsigned char)val;
    }
  }
}

void ImageLoader_AdjustSaturation(ImageData *image, float factor) {
  if (!image || !image->pixels)
    return;

  int count = image->width * image->height * 4;
  for (int i = 0; i < count; i += 4) {
    // Get RGB
    float r = image->pixels[i + 0];
    float g = image->pixels[i + 1];
    float b = image->pixels[i + 2];

    // Calculate luminance
    float gray = 0.299f * r + 0.587f * g + 0.114f * b;

    // Interpolate between gray and color
    r = gray + (r - gray) * factor;
    g = gray + (g - gray) * factor;
    b = gray + (b - gray) * factor;

    // Clamp
    if (r < 0)
      r = 0;
    if (r > 255)
      r = 255;
    if (g < 0)
      g = 0;
    if (g > 255)
      g = 255;
    if (b < 0)
      b = 0;
    if (b > 255)
      b = 255;

    image->pixels[i + 0] = (unsigned char)r;
    image->pixels[i + 1] = (unsigned char)g;
    image->pixels[i + 2] = (unsigned char)b;
  }
}

void ImageLoader_Grayscale(ImageData *image) {
  if (!image || !image->pixels)
    return;

  int count = image->width * image->height * 4;
  for (int i = 0; i < count; i += 4) {
    float r = image->pixels[i + 0];
    float g = image->pixels[i + 1];
    float b = image->pixels[i + 2];

    unsigned char gray = (unsigned char)(0.299f * r + 0.587f * g + 0.114f * b);

    image->pixels[i + 0] = gray;
    image->pixels[i + 1] = gray;
    image->pixels[i + 2] = gray;
  }
}

void ImageLoader_Crop(ImageData *image, int x, int y, int w, int h) {
  if (!image || !image->pixels)
    return;

  // Validate bounds
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x + w > image->width)
    w = image->width - x;
  if (y + h > image->height)
    h = image->height - y;
  if (w <= 0 || h <= 0)
    return;

  // Allocate new buffer
  unsigned char *newPixels = (unsigned char *)malloc(w * h * 4);
  if (!newPixels)
    return;

  // Copy cropped region
  for (int row = 0; row < h; row++) {
    unsigned char *src = image->pixels + ((y + row) * image->width + x) * 4;
    unsigned char *dst = newPixels + row * w * 4;
    memcpy(dst, src, w * 4);
  }

  // Replace old pixels
  free(image->pixels);
  image->pixels = newPixels;
  image->width = w;
  image->height = h;
}

void ImageLoader_Invert(ImageData *image) {
  if (!image || !image->pixels)
    return;

  int count = image->width * image->height * 4;
  for (int i = 0; i < count; i += 4) {
    image->pixels[i + 0] = 255 - image->pixels[i + 0];
    image->pixels[i + 1] = 255 - image->pixels[i + 1];
    image->pixels[i + 2] = 255 - image->pixels[i + 2];
  }
}

void ImageLoader_Resize(ImageData *image, int newWidth, int newHeight) {
  if (!image || !image->pixels || newWidth <= 0 || newHeight <= 0)
    return;

  unsigned char *newPixels = (unsigned char *)malloc(newWidth * newHeight * 4);
  if (!newPixels)
    return;

  // Bilinear interpolation
  float xRatio = (float)image->width / newWidth;
  float yRatio = (float)image->height / newHeight;

  for (int y = 0; y < newHeight; y++) {
    for (int x = 0; x < newWidth; x++) {
      float srcX = x * xRatio;
      float srcY = y * yRatio;
      int x0 = (int)srcX;
      int y0 = (int)srcY;
      int x1 = x0 + 1 < image->width ? x0 + 1 : x0;
      int y1 = y0 + 1 < image->height ? y0 + 1 : y0;
      float xFrac = srcX - x0;
      float yFrac = srcY - y0;

      for (int c = 0; c < 4; c++) {
        float top =
            image->pixels[(y0 * image->width + x0) * 4 + c] * (1 - xFrac) +
            image->pixels[(y0 * image->width + x1) * 4 + c] * xFrac;
        float bot =
            image->pixels[(y1 * image->width + x0) * 4 + c] * (1 - xFrac) +
            image->pixels[(y1 * image->width + x1) * 4 + c] * xFrac;
        float val = top * (1 - yFrac) + bot * yFrac;
        newPixels[(y * newWidth + x) * 4 + c] = (unsigned char)val;
      }
    }
  }

  free(image->pixels);
  image->pixels = newPixels;
  image->width = newWidth;
  image->height = newHeight;
}

// lanczos kernel - sinc(x) * sinc(x/a) windowed
static double lanczos_kernel(double x, int a) {
  if (x == 0.0)
    return 1.0;
  if (x < -a || x > a)
    return 0.0;

  double pi = 3.14159265358979323846;
  double pix = pi * x;
  return (sin(pix) / pix) * (sin(pix / a) / (pix / a));
}

// clamp helper
static int clamp_int(int v, int lo, int hi) {
  if (v < lo)
    return lo;
  if (v > hi)
    return hi;
  return v;
}

// lanczos-3 resize - photoshop quality
void ImageLoader_ResizeLanczos(ImageData *image, int newWidth, int newHeight) {
  if (!image || !image->pixels || newWidth <= 0 || newHeight <= 0)
    return;

  int a = 3; // lanczos-3 uses 3-tap kernel
  int srcW = image->width;
  int srcH = image->height;

  unsigned char *newPixels = (unsigned char *)malloc(newWidth * newHeight * 4);
  if (!newPixels)
    return;

  double xRatio = (double)srcW / newWidth;
  double yRatio = (double)srcH / newHeight;

// parallelize across rows for multi-core speedup
#pragma omp parallel for schedule(dynamic)
  for (int y = 0; y < newHeight; y++) {
    double srcY = (y + 0.5) * yRatio - 0.5;
    int y0 = (int)floor(srcY);

    for (int x = 0; x < newWidth; x++) {
      double srcX = (x + 0.5) * xRatio - 0.5;
      int x0 = (int)floor(srcX);

      double r = 0, g = 0, b = 0, alpha = 0;
      double weightSum = 0;

      // sample neighborhood
      for (int j = -a + 1; j <= a; j++) {
        int py = clamp_int(y0 + j, 0, srcH - 1);
        double wy = lanczos_kernel(srcY - (y0 + j), a);

        for (int i = -a + 1; i <= a; i++) {
          int px = clamp_int(x0 + i, 0, srcW - 1);
          double wx = lanczos_kernel(srcX - (x0 + i), a);
          double w = wx * wy;

          int idx = (py * srcW + px) * 4;
          r += image->pixels[idx + 0] * w;
          g += image->pixels[idx + 1] * w;
          b += image->pixels[idx + 2] * w;
          alpha += image->pixels[idx + 3] * w;
          weightSum += w;
        }
      }

      // normalize and clamp
      int dstIdx = (y * newWidth + x) * 4;
      if (weightSum > 0) {
        newPixels[dstIdx + 0] =
            (unsigned char)clamp_int((int)(r / weightSum + 0.5), 0, 255);
        newPixels[dstIdx + 1] =
            (unsigned char)clamp_int((int)(g / weightSum + 0.5), 0, 255);
        newPixels[dstIdx + 2] =
            (unsigned char)clamp_int((int)(b / weightSum + 0.5), 0, 255);
        newPixels[dstIdx + 3] =
            (unsigned char)clamp_int((int)(alpha / weightSum + 0.5), 0, 255);
      } else {
        newPixels[dstIdx + 0] = 0;
        newPixels[dstIdx + 1] = 0;
        newPixels[dstIdx + 2] = 0;
        newPixels[dstIdx + 3] = 255;
      }
    }
  }

  free(image->pixels);
  image->pixels = newPixels;
  image->width = newWidth;
  image->height = newHeight;
}

void ImageLoader_Sharpen(ImageData *image) {
  if (!image || !image->pixels)
    return;

  int w = image->width;
  int h = image->height;
  unsigned char *newPixels = (unsigned char *)malloc(w * h * 4);
  if (!newPixels)
    return;
  memcpy(newPixels, image->pixels, w * h * 4);

  // Sharpen kernel: 0 -1 0 / -1 5 -1 / 0 -1 0
  for (int y = 1; y < h - 1; y++) {
    for (int x = 1; x < w - 1; x++) {
      for (int c = 0; c < 3; c++) {
        int val = image->pixels[((y)*w + x) * 4 + c] * 5 -
                  image->pixels[((y - 1) * w + x) * 4 + c] -
                  image->pixels[((y + 1) * w + x) * 4 + c] -
                  image->pixels[((y)*w + x - 1) * 4 + c] -
                  image->pixels[((y)*w + x + 1) * 4 + c];
        if (val < 0)
          val = 0;
        if (val > 255)
          val = 255;
        newPixels[(y * w + x) * 4 + c] = (unsigned char)val;
      }
    }
  }

  free(image->pixels);
  image->pixels = newPixels;
}

void ImageLoader_Blur(ImageData *image) {
  if (!image || !image->pixels)
    return;

  int w = image->width;
  int h = image->height;
  unsigned char *newPixels = (unsigned char *)malloc(w * h * 4);
  if (!newPixels)
    return;
  memcpy(newPixels, image->pixels, w * h * 4);

  // 3x3 box blur
  for (int y = 1; y < h - 1; y++) {
    for (int x = 1; x < w - 1; x++) {
      for (int c = 0; c < 3; c++) {
        int sum = 0;
        for (int dy = -1; dy <= 1; dy++) {
          for (int dx = -1; dx <= 1; dx++) {
            sum += image->pixels[((y + dy) * w + (x + dx)) * 4 + c];
          }
        }
        newPixels[(y * w + x) * 4 + c] = (unsigned char)(sum / 9);
      }
    }
  }

  free(image->pixels);
  image->pixels = newPixels;
}

void ImageLoader_AutoLevels(ImageData *image) {
  if (!image || !image->pixels)
    return;

  // Find min/max for each channel
  int minR = 255, maxR = 0;
  int minG = 255, maxG = 0;
  int minB = 255, maxB = 0;

  int count = image->width * image->height * 4;
  for (int i = 0; i < count; i += 4) {
    if (image->pixels[i + 0] < minR)
      minR = image->pixels[i + 0];
    if (image->pixels[i + 0] > maxR)
      maxR = image->pixels[i + 0];
    if (image->pixels[i + 1] < minG)
      minG = image->pixels[i + 1];
    if (image->pixels[i + 1] > maxG)
      maxG = image->pixels[i + 1];
    if (image->pixels[i + 2] < minB)
      minB = image->pixels[i + 2];
    if (image->pixels[i + 2] > maxB)
      maxB = image->pixels[i + 2];
  }

  // Stretch histogram
  float scaleR = (maxR > minR) ? 255.0f / (maxR - minR) : 1.0f;
  float scaleG = (maxG > minG) ? 255.0f / (maxG - minG) : 1.0f;
  float scaleB = (maxB > minB) ? 255.0f / (maxB - minB) : 1.0f;

  for (int i = 0; i < count; i += 4) {
    image->pixels[i + 0] =
        (unsigned char)((image->pixels[i + 0] - minR) * scaleR);
    image->pixels[i + 1] =
        (unsigned char)((image->pixels[i + 1] - minG) * scaleG);
    image->pixels[i + 2] =
        (unsigned char)((image->pixels[i + 2] - minB) * scaleB);
  }
}

void ImageLoader_Sepia(ImageData *image) {
  if (!image || !image->pixels)
    return;

  int count = image->width * image->height * 4;
  for (int i = 0; i < count; i += 4) {
    int r = image->pixels[i + 0];
    int g = image->pixels[i + 1];
    int b = image->pixels[i + 2];

    int newR = (int)(r * 0.393f + g * 0.769f + b * 0.189f);
    int newG = (int)(r * 0.349f + g * 0.686f + b * 0.168f);
    int newB = (int)(r * 0.272f + g * 0.534f + b * 0.131f);

    image->pixels[i + 0] = (unsigned char)(newR > 255 ? 255 : newR);
    image->pixels[i + 1] = (unsigned char)(newG > 255 ? 255 : newG);
    image->pixels[i + 2] = (unsigned char)(newB > 255 ? 255 : newB);
  }
}

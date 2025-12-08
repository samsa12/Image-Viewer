/*
 * Renderer - Implementation
 * pix - renderer
 */

#include "renderer.h"
#include <stdlib.h>

void Renderer_Init(Renderer *renderer) {
  renderer->hBitmap = NULL;
  renderer->hMemDC = NULL;
  renderer->displayWidth = 0;
  renderer->displayHeight = 0;
  renderer->scale = 1.0f;
  renderer->offsetX = 0;
  renderer->offsetY = 0;
  renderer->fitToWindow = TRUE;

  // Initialize cache
  renderer->hScaledBitmap = NULL;
  renderer->hScaledDC = NULL;
  renderer->cachedScale = 0.0f;
  renderer->cachedWidth = 0;
  renderer->cachedHeight = 0;
}

void Renderer_Cleanup(Renderer *renderer) {
  if (renderer->hBitmap) {
    DeleteObject(renderer->hBitmap);
    renderer->hBitmap = NULL;
  }
  if (renderer->hMemDC) {
    DeleteDC(renderer->hMemDC);
    renderer->hMemDC = NULL;
  }
  // Clean up cached scaled bitmap
  if (renderer->hScaledBitmap) {
    DeleteObject(renderer->hScaledBitmap);
    renderer->hScaledBitmap = NULL;
  }
  if (renderer->hScaledDC) {
    DeleteDC(renderer->hScaledDC);
    renderer->hScaledDC = NULL;
  }
  renderer->cachedScale = 0.0f;
  renderer->cachedWidth = 0;
  renderer->cachedHeight = 0;
}

void Renderer_CreateBitmap(Renderer *renderer, HDC hdc,
                           const ImageData *image) {
  if (!image || !image->pixels)
    return;

  // Cleanup previous bitmap
  Renderer_Cleanup(renderer);

  // Create compatible DC
  renderer->hMemDC = CreateCompatibleDC(hdc);

  // Create DIB section for the image
  BITMAPINFO bmi = {0};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = image->width;
  bmi.bmiHeader.biHeight = -image->height; // Negative for top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void *bits = NULL;
  renderer->hBitmap =
      CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);

  if (renderer->hBitmap && bits) {
    // Copy pixels (convert RGBA to BGRA for Windows)
    unsigned char *src = image->pixels;
    unsigned char *dst = (unsigned char *)bits;
    int totalPixels = image->width * image->height;

    for (int i = 0; i < totalPixels; i++) {
      dst[i * 4 + 0] = src[i * 4 + 2]; // B
      dst[i * 4 + 1] = src[i * 4 + 1]; // G
      dst[i * 4 + 2] = src[i * 4 + 0]; // R
      dst[i * 4 + 3] = src[i * 4 + 3]; // A
    }

    SelectObject(renderer->hMemDC, renderer->hBitmap);
  }

  renderer->displayWidth = image->width;
  renderer->displayHeight = image->height;
}

void Renderer_FitToWindow(Renderer *renderer, RECT *clientRect,
                          const ImageData *image) {
  if (!image || image->width == 0 || image->height == 0)
    return;

  int windowWidth = clientRect->right - clientRect->left;
  int windowHeight = clientRect->bottom - clientRect->top;

  float scaleX = (float)windowWidth / (float)image->width;
  float scaleY = (float)windowHeight / (float)image->height;

  // Use the smaller scale to fit entirely
  renderer->scale = (scaleX < scaleY) ? scaleX : scaleY;

  // Don't scale up beyond 100%
  if (renderer->scale > 1.0f) {
    renderer->scale = 1.0f;
  }

  renderer->fitToWindow = TRUE;
  Renderer_CenterImage(renderer, clientRect, image);
}

void Renderer_SetScale(Renderer *renderer, float scale) {
  if (scale < 0.1f)
    scale = 0.1f;
  if (scale > 10.0f)
    scale = 10.0f;
  renderer->scale = scale;
  renderer->fitToWindow = FALSE;
}

void Renderer_CenterImage(Renderer *renderer, RECT *clientRect,
                          const ImageData *image) {
  if (!image)
    return;

  int windowWidth = clientRect->right - clientRect->left;
  int windowHeight = clientRect->bottom - clientRect->top;

  int scaledWidth = (int)(image->width * renderer->scale);
  int scaledHeight = (int)(image->height * renderer->scale);

  renderer->offsetX = (windowWidth - scaledWidth) / 2;
  renderer->offsetY = (windowHeight - scaledHeight) / 2;
}

void Renderer_Paint(Renderer *renderer, HDC hdc, RECT *clientRect,
                    const ImageData *image) {
  // Fill background with dark gray
  HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
  FillRect(hdc, clientRect, bgBrush);
  DeleteObject(bgBrush);

  if (!renderer->hMemDC || !image || !image->pixels) {
    // No image loaded - draw message
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(150, 150, 150));

    const char *msg = "Drag & drop an image or press O to open";
    RECT textRect = *clientRect;
    DrawTextA(hdc, msg, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return;
  }

  // Calculate scaled dimensions
  int scaledWidth = (int)(image->width * renderer->scale);
  int scaledHeight = (int)(image->height * renderer->scale);

  // Use high-quality stretching
  SetStretchBltMode(hdc, HALFTONE);
  SetBrushOrgEx(hdc, 0, 0, NULL);

  // Draw the image
  StretchBlt(hdc, renderer->offsetX, renderer->offsetY, scaledWidth,
             scaledHeight, renderer->hMemDC, 0, 0, image->width, image->height,
             SRCCOPY);
}

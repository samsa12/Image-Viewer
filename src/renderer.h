// renderer header
// handles drawing images to screen with zoom/pan

#ifndef RENDERER_H
#define RENDERER_H

#include "image_loader.h"
#include <windows.h>


// main renderer state
typedef struct {
  HBITMAP hBitmap;
  HDC hMemDC;
  int displayWidth;
  int displayHeight;
  float scale;
  int offsetX;
  int offsetY;
  BOOL fitToWindow;

  // cached scaled bitmap for smooth panning
  HBITMAP hScaledBitmap;
  HDC hScaledDC;
  float cachedScale;
  int cachedWidth;
  int cachedHeight;
} Renderer;

// functions
void Renderer_Init(Renderer *renderer);
void Renderer_Cleanup(Renderer *renderer);
void Renderer_CreateBitmap(Renderer *renderer, HDC hdc, const ImageData *image);
void Renderer_Paint(Renderer *renderer, HDC hdc, RECT *clientRect,
                    const ImageData *image);
void Renderer_FitToWindow(Renderer *renderer, RECT *clientRect,
                          const ImageData *image);
void Renderer_SetScale(Renderer *renderer, float scale);
void Renderer_CenterImage(Renderer *renderer, RECT *clientRect,
                          const ImageData *image);

#endif

/*
 * Image Loader - Implementation
 * Pure C Image Viewer
 */

#define STB_IMAGE_IMPLEMENTATION
#include "../lib/stb_image.h"
#include "image_loader.h"
#include <string.h>

static char g_lastError[256] = {0};

int ImageLoader_Load(const char* filepath, ImageData* image) {
    if (!filepath || !image) {
        strcpy(g_lastError, "Invalid parameters");
        return 0;
    }
    
    // Clear previous image data
    memset(image, 0, sizeof(ImageData));
    
    // Load image using stb_image (force RGBA)
    image->pixels = stbi_load(filepath, &image->width, &image->height, &image->channels, 4);
    
    if (!image->pixels) {
        snprintf(g_lastError, sizeof(g_lastError), "Failed to load: %s", stbi_failure_reason());
        return 0;
    }
    
    // Store filepath
    strncpy(image->filepath, filepath, MAX_PATH - 1);
    image->channels = 4; // We forced RGBA
    
    return 1;
}

void ImageLoader_Free(ImageData* image) {
    if (image && image->pixels) {
        stbi_image_free(image->pixels);
        image->pixels = NULL;
        image->width = 0;
        image->height = 0;
        image->channels = 0;
        image->filepath[0] = '\0';
    }
}

const char* ImageLoader_GetError(void) {
    return g_lastError;
}

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <png.h>

/* Test png_image API and colormap functions */
static void test_png_image_colormap(const uint8_t *data, size_t size) {
    png_image image;
    memset(&image, 0, sizeof(image));
    image.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_memory(&image, data, size)) {
        return;
    }

    /* Limit image size for performance */
    if (image.width > 1024 || image.height > 1024) {
        png_image_free(&image);
        return;
    }

    /* Test different colormap formats to trigger uncovered functions */
    png_uint_32 formats[] = {
        PNG_FORMAT_GRAY,
        PNG_FORMAT_GA,
        PNG_FORMAT_RGB,
        PNG_FORMAT_RGBA,
        PNG_FORMAT_RGB_COLORMAP,
        PNG_FORMAT_RGBA_COLORMAP
    };

    int i;
    for (i = 0; i < 6; i++) {
        image.format = formats[i];

        size_t buffer_size = PNG_IMAGE_SIZE(image);
        if (buffer_size > 0 && buffer_size < 5*1024*1024) { /* Limit to 5MB */
            void *img_buffer = malloc(buffer_size);
            if (img_buffer) {
                if (image.format & PNG_FORMAT_FLAG_COLORMAP) {
                    png_bytep colormap = (png_bytep)malloc(PNG_IMAGE_COLORMAP_SIZE(image));
                    if (colormap) {
                        /* This triggers png_image_finish_read and colormap generation */
                        png_image_finish_read(&image, NULL, img_buffer, 0, colormap);
                        free(colormap);
                    }
                } else {
                    png_image_finish_read(&image, NULL, img_buffer, 0, NULL);
                }
                free(img_buffer);
            }
        }

        /* Reset for next format */
        if (i < 5) {
            png_image_free(&image);
            memset(&image, 0, sizeof(image));
            image.version = PNG_IMAGE_VERSION;
            if (!png_image_begin_read_from_memory(&image, data, size)) {
                break;
            }
        }
    }

    png_image_free(&image);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return 0;

    /* Test 2: PNG image API with colormap processing */
    test_png_image_colormap(data, size);

    return 0;
}

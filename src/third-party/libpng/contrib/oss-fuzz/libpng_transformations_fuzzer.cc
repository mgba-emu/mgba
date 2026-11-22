#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <png.h>

/* Memory buffer structure for PNG reading */
struct png_mem_buffer {
    const uint8_t *data;
    size_t size;
    size_t pos;
};

/* Custom read function for memory buffer */
static void png_read_from_buffer(png_structp png_ptr, png_bytep data, png_size_t length) {
    struct png_mem_buffer *buf = (struct png_mem_buffer*)png_get_io_ptr(png_ptr);
    if (buf->pos + length > buf->size) {
        png_error(png_ptr, "Read error");
    }
    memcpy(data, buf->data + buf->pos, length);
    buf->pos += length;
}

/* Test PNG transformations to trigger uncovered code paths */
static void test_png_transformations(const uint8_t *data, size_t size) {
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return;
    }

    /* Declare heap pointers before setjmp so they can be freed on longjmp.
       Must be volatile per C standard §7.13.2.1: non-volatile locals modified
       between setjmp and longjmp have indeterminate values after longjmp. */
    volatile png_bytep row = NULL;
    volatile png_colorp palette = NULL;

    if (setjmp(png_jmpbuf(png_ptr))) {
        free((void*)row);
        free((void*)palette);
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return;
    }

    struct png_mem_buffer buffer = {data, size, 0};
    png_set_read_fn(png_ptr, &buffer, png_read_from_buffer);

    png_read_info(png_ptr, info_ptr);

    int width = png_get_image_width(png_ptr, info_ptr);
    int height = png_get_image_height(png_ptr, info_ptr);
    int color_type = png_get_color_type(png_ptr, info_ptr);
    int bit_depth = png_get_bit_depth(png_ptr, info_ptr);

    /* Sanity check dimensions */
    if (width <= 0 || height <= 0 || width > 2048 || height > 2048) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return;
    }

    /* Target 1: 16-bit depth processing (triggers png_do_chop) */
    if (bit_depth == 16) {
        png_set_scale_16(png_ptr);
    }

    /* Target 2: Color quantization (triggers png_do_quantize) */
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGB_ALPHA) {
        palette = (png_colorp)malloc(256 * sizeof(png_color));
        if (palette) {
            int i;
            for (i = 0; i < 256; i++) {
                palette[i].red = palette[i].green = palette[i].blue = (png_byte)i;
            }
            png_set_quantize(png_ptr, palette, 256, 256, NULL, 0);
            free((void*)palette);
            palette = NULL;
        }
    }

    /* Target 3: Alpha channel transformations */
    if (color_type & PNG_COLOR_MASK_ALPHA) {
        png_set_invert_alpha(png_ptr);  /* png_do_read_invert_alpha */
        png_set_swap_alpha(png_ptr);    /* png_do_read_swap_alpha */
    }

    /* Target 4: Bit shifting (triggers png_do_unshift) */
    if (bit_depth < 8) {
        png_set_packing(png_ptr);
    }

    /* Target 5: Alpha encoding (triggers png_do_encode_alpha) */
    png_set_alpha_mode(png_ptr, PNG_ALPHA_STANDARD, PNG_GAMMA_LINEAR);

    /* Update transformations */
    png_read_update_info(png_ptr, info_ptr);

    /* Read image data to execute transformations */
    size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);
    row = (png_bytep)malloc(rowbytes);
    if (row) {
        int y;
        for (y = 0; y < height && y < 100; y++) { /* Limit rows for performance */
            png_read_row(png_ptr, row, NULL);
        }
    }

    free((void*)row);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return 0;

    /* Test 1: Standard PNG reading with transformations */
    test_png_transformations(data, size);

    return 0;
}

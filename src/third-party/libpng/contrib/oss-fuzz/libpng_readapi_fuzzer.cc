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

/* Test png_read_png API */
static void test_png_read_png_api(const uint8_t *data, size_t size) {
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) return;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return;
    }

    struct png_mem_buffer buffer = {data, size, 0};
    png_set_read_fn(png_ptr, &buffer, png_read_from_buffer);

    /* Use png_read_png with transform flags */
    png_read_png(png_ptr, info_ptr,
                 PNG_TRANSFORM_SCALE_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND,
                 NULL);

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 8) return 0;

    /* Test 3: png_read_png API */
    test_png_read_png_api(data, size);

    return 0;
}

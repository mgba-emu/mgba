/* pnggetset.c
 *
 * Copyright (c) 2026 Cosmin Truta
 *
 * This code is released under the libpng license.
 * For conditions of distribution and use, see the disclaimer
 * and license in png.h
 *
 * Test getter and setter correctness.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HAVE_CONFIG_H) && !defined(PNG_NO_CONFIG_H)
#  include <config.h>
#endif

#ifdef PNG_FREESTANDING_TESTS
#  include <png.h>
#else
#  include "../../png.h"
#endif

/* Test: get the PLTE, pass it straight back to set, verify roundtrip. */
static int
test_plte_roundtrip(void)
{
   png_structp png_ptr;
   png_infop info_ptr;
   png_color palette[4];
   png_colorp got_palette = NULL;
   int num_palette = 0;
   int i;

   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
       NULL, NULL, NULL);
   if (png_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_write_struct failed\n");
      return 1;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_info_struct failed\n");
      png_destroy_write_struct(&png_ptr, NULL);
      return 1;
   }

   if (setjmp(png_jmpbuf(png_ptr)))
   {
      fprintf(stderr, "pnggetset: libpng error in test_plte_roundtrip\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   /* Set up a palette-color image header. */
   png_set_IHDR(png_ptr, info_ptr, 1, 1, 8, PNG_COLOR_TYPE_PALETTE,
       PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

   /* Populate with recognizable values. */
   for (i = 0; i < 4; i++)
   {
      palette[i].red   = (png_byte)(i * 10);
      palette[i].green = (png_byte)(i * 20);
      palette[i].blue  = (png_byte)(i * 30);
   }
   png_set_PLTE(png_ptr, info_ptr, palette, 4);

   /* Get the internal pointer and feed it straight back. */
   png_get_PLTE(png_ptr, info_ptr, &got_palette, &num_palette);
   if (got_palette == NULL || num_palette != 4)
   {
      fprintf(stderr, "pnggetset: png_get_PLTE returned unexpected values\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   /* This is the critical call: the pointer aliases info_ptr->palette. */
   png_set_PLTE(png_ptr, info_ptr, got_palette, num_palette);

   /* Verify the data survived the roundtrip. */
   got_palette = NULL;
   num_palette = 0;
   png_get_PLTE(png_ptr, info_ptr, &got_palette, &num_palette);
   if (got_palette == NULL || num_palette != 4)
   {
      fprintf(stderr, "pnggetset: PLTE lost after roundtrip\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }
   for (i = 0; i < 4; i++)
   {
      if ((got_palette[i].red != (png_byte)(i * 10))
          || (got_palette[i].green != (png_byte)(i * 20))
          || (got_palette[i].blue != (png_byte)(i * 30)))
      {
         fprintf(stderr,
             "pnggetset: PLTE entry %d corrupted after roundtrip\n", i);
         png_destroy_write_struct(&png_ptr, &info_ptr);
         return 1;
      }
   }

   png_destroy_write_struct(&png_ptr, &info_ptr);
   return 0;
}

#ifdef PNG_hIST_SUPPORTED
/* Test: get the hIST, pass it straight back to set, verify roundtrip. */
static int
test_hist_roundtrip(void)
{
   png_structp png_ptr;
   png_infop info_ptr;
   png_color palette[4];
   png_uint_16 hist[4];
   png_uint_16p got_hist = NULL;
   int i;

   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
       NULL, NULL, NULL);
   if (png_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_write_struct failed\n");
      return 1;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_info_struct failed\n");
      png_destroy_write_struct(&png_ptr, NULL);
      return 1;
   }

   if (setjmp(png_jmpbuf(png_ptr)))
   {
      fprintf(stderr, "pnggetset: libpng error in test_hist_roundtrip\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   /* Set up a palette-color image header. */
   memset(palette, 0, sizeof palette);
   png_set_IHDR(png_ptr, info_ptr, 1, 1, 8, PNG_COLOR_TYPE_PALETTE,
       PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
   png_set_PLTE(png_ptr, info_ptr, palette, 4);

   /* Populate with recognizable values. */
   for (i = 0; i < 4; i++)
      hist[i] = (png_uint_16)(i * 100 + 42);

   png_set_hIST(png_ptr, info_ptr, hist);

   /* Get the internal pointer and feed it straight back. */
   if (png_get_hIST(png_ptr, info_ptr, &got_hist) == 0 || got_hist == NULL)
   {
      fprintf(stderr, "pnggetset: png_get_hIST returned unexpected values\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   /* This is the critical call: the pointer aliases info_ptr->hist. */
   png_set_hIST(png_ptr, info_ptr, got_hist);

   /* Verify the data survived the roundtrip. */
   got_hist = NULL;
   if (png_get_hIST(png_ptr, info_ptr, &got_hist) == 0 || got_hist == NULL)
   {
      fprintf(stderr, "pnggetset: hIST lost after roundtrip\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }
   for (i = 0; i < 4; i++)
   {
      if (got_hist[i] != (png_uint_16)(i * 100 + 42))
      {
         fprintf(stderr,
             "pnggetset: hIST entry %d corrupted after roundtrip\n", i);
         png_destroy_write_struct(&png_ptr, &info_ptr);
         return 1;
      }
   }

   png_destroy_write_struct(&png_ptr, &info_ptr);
   return 0;
}
#endif /* PNG_hIST_SUPPORTED */

#ifdef PNG_tRNS_SUPPORTED
/* Test: get the tRNS, pass it straight back to set, verify roundtrip. */
static int
test_trns_roundtrip(void)
{
   png_structp png_ptr;
   png_infop info_ptr;
   png_color palette[4];
   png_byte trans_alpha[4];
   png_color_16 trans_color;
   png_bytep got_alpha = NULL;
   png_color_16p got_color = NULL;
   int num_trans = 0;
   int i;

   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
       NULL, NULL, NULL);
   if (png_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_write_struct failed\n");
      return 1;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_info_struct failed\n");
      png_destroy_write_struct(&png_ptr, NULL);
      return 1;
   }

   if (setjmp(png_jmpbuf(png_ptr)))
   {
      fprintf(stderr, "pnggetset: libpng error in test_trns_roundtrip\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   /* Set up a palette-color image. */
   memset(palette, 0, sizeof palette);
   png_set_IHDR(png_ptr, info_ptr, 1, 1, 8, PNG_COLOR_TYPE_PALETTE,
       PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
   png_set_PLTE(png_ptr, info_ptr, palette, 4);

   /* Populate tRNS with recognizable values. */
   for (i = 0; i < 4; i++)
      trans_alpha[i] = (png_byte)(0xff - i * 0x11);
   memset(&trans_color, 0, sizeof trans_color);

   png_set_tRNS(png_ptr, info_ptr, trans_alpha, 4, &trans_color);

   /* Get the internal pointer and feed it straight back. */
   png_get_tRNS(png_ptr, info_ptr, &got_alpha, &num_trans, &got_color);
   if (got_alpha == NULL || num_trans != 4)
   {
      fprintf(stderr, "pnggetset: png_get_tRNS returned unexpected values\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   /* This is the critical call: the pointer aliases info_ptr->trans_alpha. */
   png_set_tRNS(png_ptr, info_ptr, got_alpha, num_trans, got_color);

   /* Verify the data survived the roundtrip. */
   got_alpha = NULL;
   num_trans = 0;
   png_get_tRNS(png_ptr, info_ptr, &got_alpha, &num_trans, &got_color);
   if (got_alpha == NULL || num_trans != 4)
   {
      fprintf(stderr, "pnggetset: tRNS lost after roundtrip\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }
   for (i = 0; i < 4; i++)
   {
      if (got_alpha[i] != (png_byte)(0xff - i * 0x11))
      {
         fprintf(stderr,
             "pnggetset: tRNS entry %d corrupted after roundtrip\n", i);
         png_destroy_write_struct(&png_ptr, &info_ptr);
         return 1;
      }
   }

   png_destroy_write_struct(&png_ptr, &info_ptr);
   return 0;
}
#endif /* PNG_tRNS_SUPPORTED */

#ifdef PNG_TEXT_SUPPORTED
/* Test: get the text array, pass it straight back to set, verify data. */
#define TEXT_COUNT 6 /* enough to trigger reallocation on the second set */
static int
test_text_roundtrip(void)
{
   png_structp png_ptr;
   png_infop info_ptr;
   png_text text_entries[TEXT_COUNT];
   png_textp got_text = NULL;
   int got_num_text = 0;
   int i;

   /* Recognizable keys and values. */
   static const char *keys[TEXT_COUNT] = {
      "Title", "Author", "Desc", "Copyright", "Source", "Comment"
   };
   static const char *vals[TEXT_COUNT] = {
      "t0", "t1", "t2", "t3", "t4", "t5"
   };

   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
       NULL, NULL, NULL);
   if (png_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_write_struct failed\n");
      return 1;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_info_struct failed\n");
      png_destroy_write_struct(&png_ptr, NULL);
      return 1;
   }

   if (setjmp(png_jmpbuf(png_ptr)))
   {
      fprintf(stderr, "pnggetset: libpng error in test_text_roundtrip\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   /* Populate the text entries. */
   memset(text_entries, 0, sizeof text_entries);
   for (i = 0; i < TEXT_COUNT; i++)
   {
      text_entries[i].compression = PNG_TEXT_COMPRESSION_NONE;
      text_entries[i].key = (png_charp)keys[i];
      text_entries[i].text = (png_charp)vals[i];
   }
   png_set_text(png_ptr, info_ptr, text_entries, TEXT_COUNT);

   /* Get the internal pointer and feed it straight back (append). */
   png_get_text(png_ptr, info_ptr, &got_text, &got_num_text);
   if (got_text == NULL || got_num_text != TEXT_COUNT)
   {
      fprintf(stderr, "pnggetset: png_get_text returned unexpected values\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   /* This is the critical call: got_text aliases info_ptr->text. */
   png_set_text(png_ptr, info_ptr, got_text, got_num_text);

   /* Verify the original entries survived. */
   got_text = NULL;
   got_num_text = 0;
   png_get_text(png_ptr, info_ptr, &got_text, &got_num_text);
   if (got_text == NULL || got_num_text != TEXT_COUNT * 2)
   {
      fprintf(stderr, "pnggetset: text count %d, expected %d after roundtrip\n",
          got_num_text, TEXT_COUNT * 2);
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }
   for (i = 0; i < TEXT_COUNT; i++)
   {
      if (got_text[i].key == NULL ||
          strcmp(got_text[i].key, keys[i]) != 0 ||
          got_text[i].text == NULL ||
          strcmp(got_text[i].text, vals[i]) != 0)
      {
         fprintf(stderr,
             "pnggetset: text entry %d corrupted after roundtrip\n", i);
         png_destroy_write_struct(&png_ptr, &info_ptr);
         return 1;
      }
   }

   png_destroy_write_struct(&png_ptr, &info_ptr);
   return 0;
}
#undef TEXT_COUNT
#endif /* PNG_TEXT_SUPPORTED */

#ifdef PNG_sPLT_SUPPORTED
/* Test: get the sPLT array, pass it straight back to set, verify data. */
static int
test_splt_roundtrip(void)
{
   png_structp png_ptr;
   png_infop info_ptr;
   png_sPLT_t splt;
   png_sPLT_entry splt_entries[4];
   png_sPLT_tp got_spalettes = NULL;
   int got_num, i;

   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
       NULL, NULL, NULL);
   if (png_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_write_struct failed\n");
      return 1;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_info_struct failed\n");
      png_destroy_write_struct(&png_ptr, NULL);
      return 1;
   }

   if (setjmp(png_jmpbuf(png_ptr)))
   {
      fprintf(stderr, "pnggetset: libpng error in test_splt_roundtrip\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   /* Populate with recognizable values. */
   memset(splt_entries, 0, sizeof splt_entries);
   for (i = 0; i < 4; i++)
   {
      splt_entries[i].red = (png_uint_16)(i * 1000);
      splt_entries[i].green = (png_uint_16)(i * 2000);
      splt_entries[i].blue = (png_uint_16)(i * 3000);
      splt_entries[i].alpha = 0xffffU;
      splt_entries[i].frequency = (png_uint_16)(i + 1);
   }
   memset(&splt, 0, sizeof splt);
   splt.name = (png_charp)"test_sPLT";
   splt.depth = 16;
   splt.entries = splt_entries;
   splt.nentries = 4;

   png_set_sPLT(png_ptr, info_ptr, &splt, 1);

   /* Get the internal pointer and feed it straight back (append). */
   got_num = png_get_sPLT(png_ptr, info_ptr, &got_spalettes);
   if (got_spalettes == NULL || got_num != 1)
   {
      fprintf(stderr, "pnggetset: png_get_sPLT returned unexpected values\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   /* This is the critical call: got_spalettes aliases internal storage. */
   png_set_sPLT(png_ptr, info_ptr, got_spalettes, got_num);

   /* Verify the original entry survived. */
   got_spalettes = NULL;
   got_num = png_get_sPLT(png_ptr, info_ptr, &got_spalettes);
   if (got_spalettes == NULL || got_num != 2)
   {
      fprintf(stderr, "pnggetset: sPLT count %d, expected 2 after roundtrip\n",
          got_num);
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }
   if (strcmp(got_spalettes[0].name, "test_sPLT") != 0 ||
       got_spalettes[0].nentries != 4 ||
       got_spalettes[0].depth != 16)
   {
      fprintf(stderr,
          "pnggetset: sPLT entry 0 corrupted after roundtrip\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }
   for (i = 0; i < 4; i++)
   {
      if (got_spalettes[0].entries[i].red != (png_uint_16)(i * 1000) ||
          got_spalettes[0].entries[i].green != (png_uint_16)(i * 2000) ||
          got_spalettes[0].entries[i].blue != (png_uint_16)(i * 3000))
      {
         fprintf(stderr,
             "pnggetset: sPLT[0] entry %d corrupted after roundtrip\n", i);
         png_destroy_write_struct(&png_ptr, &info_ptr);
         return 1;
      }
   }

   png_destroy_write_struct(&png_ptr, &info_ptr);
   return 0;
}
#endif /* PNG_sPLT_SUPPORTED */

#ifdef PNG_STORE_UNKNOWN_CHUNKS_SUPPORTED
/* Test: get unknown chunks, pass them straight back to set, verify data. */
static int
test_unknown_roundtrip(void)
{
   png_structp png_ptr;
   png_infop info_ptr;
   png_unknown_chunk unk;
   png_unknown_chunkp got_unknowns = NULL;
   int got_num;
   static const png_byte test_data[] = {0xde, 0xad, 0xbe, 0xef};

   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
       NULL, NULL, NULL);
   if (png_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_write_struct failed\n");
      return 1;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_info_struct failed\n");
      png_destroy_write_struct(&png_ptr, NULL);
      return 1;
   }

   if (setjmp(png_jmpbuf(png_ptr)))
   {
      fprintf(stderr,
          "pnggetset: libpng error in test_unknown_roundtrip\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   /* Set up an unknown chunk with recognizable data. */
   memset(&unk, 0, sizeof unk);
   memcpy(unk.name, "teSt", 5);
   unk.data = (png_bytep)test_data;
   unk.size = sizeof test_data;
   unk.location = PNG_HAVE_IHDR;

   png_set_keep_unknown_chunks(png_ptr, PNG_HANDLE_CHUNK_ALWAYS, NULL, 0);
   png_set_unknown_chunks(png_ptr, info_ptr, &unk, 1);

   /* Get the internal pointer and feed it straight back (append). */
   got_num = png_get_unknown_chunks(png_ptr, info_ptr, &got_unknowns);
   if (got_unknowns == NULL || got_num != 1)
   {
      fprintf(stderr,
          "pnggetset: png_get_unknown_chunks returned unexpected values\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   /* This is the critical call: got_unknowns aliases internal storage. */
   png_set_unknown_chunks(png_ptr, info_ptr, got_unknowns, got_num);

   /* Verify the original entry survived. */
   got_unknowns = NULL;
   got_num = png_get_unknown_chunks(png_ptr, info_ptr, &got_unknowns);
   if (got_unknowns == NULL || got_num != 2)
   {
      fprintf(stderr,
          "pnggetset: unknown_chunks count %d, expected 2 after roundtrip\n",
          got_num);
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }
   if (memcmp(got_unknowns[0].name, "teSt", 4) != 0 ||
       got_unknowns[0].size != sizeof test_data ||
       memcmp(got_unknowns[0].data, test_data, sizeof test_data) != 0)
   {
      fprintf(stderr,
          "pnggetset: unknown chunk 0 corrupted after roundtrip\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   png_destroy_write_struct(&png_ptr, &info_ptr);
   return 0;
}
#endif /* PNG_STORE_UNKNOWN_CHUNKS_SUPPORTED */

/* Memory buffer for PNG I/O without temp files. */
#define MEM_BUF_SIZE 4096

typedef struct
{
   png_byte data[MEM_BUF_SIZE];
   size_t len;
   size_t pos;
} mem_buf;

static void PNGCBAPI
mem_write(png_structp png_ptr, png_bytep buf, png_size_t length)
{
   mem_buf *mb = (mem_buf *)png_get_io_ptr(png_ptr);

   if (mb->len + length > MEM_BUF_SIZE)
      png_error(png_ptr, "pnggetset: write overflow");

   memcpy(mb->data + mb->len, buf, length);
   mb->len += length;
}

static void PNGCBAPI
mem_flush(png_structp png_ptr)
{
   (void)png_ptr;
}

static void PNGCBAPI
mem_read(png_structp png_ptr, png_bytep buf, png_size_t length)
{
   mem_buf *mb = (mem_buf *)png_get_io_ptr(png_ptr);

   if (mb->pos + length > mb->len)
      png_error(png_ptr, "pnggetset: read overflow");

   memcpy(buf, mb->data + mb->pos, length);
   mb->pos += length;
}

/* Palette sync after gamma correction.
 *
 * When info_ptr->palette and png_ptr->palette are separate buffers,
 * in-place gamma correction of png_ptr->palette must be synced back
 * to info_ptr->palette so that png_get_PLTE returns the corrected
 * values.
 */
#define PLTE_SYNC_NPALETTE 4

static const png_color plte_sync_original[PLTE_SYNC_NPALETTE] =
{
   {  64,  96, 128 },
   { 128, 160, 192 },
   { 192, 224, 240 },
   {  32,  48,  64 }
};

static int
test_plte_palette_sync(void)
{
   mem_buf buf;
   png_structp png_ptr;
   png_infop info_ptr;
   png_colorp got_palette;
   int num_palette;
   double file_gamma;
   png_byte row[1];
   int i;
   int changed;

   /* Write a 1x1 palette PNG with gAMA = 1.0 (linear). */
   buf.len = 0;
   buf.pos = 0;

   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
       NULL, NULL, NULL);
   if (png_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_write_struct failed\n");
      return 1;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_info_struct failed\n");
      png_destroy_write_struct(&png_ptr, NULL);
      return 1;
   }

   if (setjmp(png_jmpbuf(png_ptr)))
   {
      fprintf(stderr, "pnggetset: libpng error in test_plte_palette_sync"
          " (write)\n");
      png_destroy_write_struct(&png_ptr, &info_ptr);
      return 1;
   }

   png_set_write_fn(png_ptr, &buf, mem_write, mem_flush);
   png_set_IHDR(png_ptr, info_ptr, 1, 1, 8, PNG_COLOR_TYPE_PALETTE,
       PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE,
       PNG_FILTER_TYPE_BASE);
   png_set_PLTE(png_ptr, info_ptr,
       (png_colorp)plte_sync_original, PLTE_SYNC_NPALETTE);
   png_set_gAMA(png_ptr, info_ptr, 1.0);
   png_write_info(png_ptr, info_ptr);

   row[0] = 0;
   png_write_row(png_ptr, row);
   png_write_end(png_ptr, info_ptr);
   png_destroy_write_struct(&png_ptr, &info_ptr);

   /* Read back with gamma correction as the sole transform. */
   buf.pos = 0;

   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING,
       NULL, NULL, NULL);
   if (png_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_read_struct failed\n");
      return 1;
   }

   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fprintf(stderr, "pnggetset: png_create_info_struct failed\n");
      png_destroy_read_struct(&png_ptr, NULL, NULL);
      return 1;
   }

   if (setjmp(png_jmpbuf(png_ptr)))
   {
      fprintf(stderr, "pnggetset: libpng error in test_plte_palette_sync"
          " (read)\n");
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return 1;
   }

   png_set_read_fn(png_ptr, &buf, mem_read);
   png_read_info(png_ptr, info_ptr);

   if (png_get_gAMA(png_ptr, info_ptr, &file_gamma) == 0)
   {
      fprintf(stderr, "pnggetset: gAMA chunk not found\n");
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return 1;
   }

   png_set_gamma(png_ptr, 2.2, file_gamma);
   png_read_update_info(png_ptr, info_ptr);

   if (png_get_PLTE(png_ptr, info_ptr, &got_palette, &num_palette) == 0)
   {
      fprintf(stderr, "pnggetset: png_get_PLTE failed after update\n");
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return 1;
   }

   if (num_palette != PLTE_SYNC_NPALETTE)
   {
      fprintf(stderr, "pnggetset: palette size %d, expected %d\n",
          num_palette, PLTE_SYNC_NPALETTE);
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      return 1;
   }

   /* Every entry must differ from the original after gamma correction
    * (file_gamma=1.0, screen_gamma=2.2).  If the sync was skipped,
    * info_ptr->palette still holds the stale pre-correction values.
    */
   changed = 0;

   for (i = 0; i < PLTE_SYNC_NPALETTE; i++)
   {
      if ((got_palette[i].red != plte_sync_original[i].red)
          || (got_palette[i].green != plte_sync_original[i].green)
          || (got_palette[i].blue != plte_sync_original[i].blue))
      {
         changed++;
      }
      else
      {
         fprintf(stderr,
             "pnggetset: palette entry %d NOT gamma-corrected: "
             "got {%u, %u, %u}, same as original\n",
             i,
             (unsigned)got_palette[i].red,
             (unsigned)got_palette[i].green,
             (unsigned)got_palette[i].blue);
      }
   }

   png_read_row(png_ptr, row, NULL);
   png_read_end(png_ptr, NULL);
   png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

   if (changed != PLTE_SYNC_NPALETTE)
   {
      fprintf(stderr,
          "pnggetset: only %d of %d palette entries were "
          "gamma-corrected (palette sync failed)\n",
          changed, PLTE_SYNC_NPALETTE);
      return 1;
   }

   return 0;
}

int
main(void)
{
   int result = 0;

   printf("Testing PLTE get-then-set roundtrip... ");
   fflush(stdout);
   if (test_plte_roundtrip() != 0)
   {
      printf("FAIL\n");
      result = 1;
   }
   else
      printf("PASS\n");

#ifdef PNG_hIST_SUPPORTED
   printf("Testing hIST get-then-set roundtrip... ");
   fflush(stdout);
   if (test_hist_roundtrip() != 0)
   {
      printf("FAIL\n");
      result = 1;
   }
   else
      printf("PASS\n");
#endif

#ifdef PNG_tRNS_SUPPORTED
   printf("Testing tRNS get-then-set roundtrip... ");
   fflush(stdout);
   if (test_trns_roundtrip() != 0)
   {
      printf("FAIL\n");
      result = 1;
   }
   else
      printf("PASS\n");
#endif

#ifdef PNG_TEXT_SUPPORTED
   printf("Testing tEXt get-then-set roundtrip... ");
   fflush(stdout);
   if (test_text_roundtrip() != 0)
   {
      printf("FAIL\n");
      result = 1;
   }
   else
      printf("PASS\n");
#endif

#ifdef PNG_sPLT_SUPPORTED
   printf("Testing sPLT get-then-set roundtrip... ");
   fflush(stdout);
   if (test_splt_roundtrip() != 0)
   {
      printf("FAIL\n");
      result = 1;
   }
   else
      printf("PASS\n");
#endif

#ifdef PNG_STORE_UNKNOWN_CHUNKS_SUPPORTED
   printf("Testing unknown chunks get-then-set roundtrip... ");
   fflush(stdout);
   if (test_unknown_roundtrip() != 0)
   {
      printf("FAIL\n");
      result = 1;
   }
   else
      printf("PASS\n");
#endif

   printf("Testing PLTE sync after gamma correction... ");
   fflush(stdout);
   if (test_plte_palette_sync() != 0)
   {
      printf("FAIL\n");
      result = 1;
   }
   else
      printf("PASS\n");

   return result;
}

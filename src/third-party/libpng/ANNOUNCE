libpng 1.6.59 - September 28, 2026
==================================

This is a public release of libpng, intended for use in production code.


Files available for download
----------------------------

Source files:

 * libpng-1.6.59.tar.xz (LZMA-compressed, recommended)
 * libpng-1.6.59.tar.gz (deflate-compressed)
 * lpng1659.7z (LZMA-compressed)
 * lpng1659.zip (deflate-compressed)

Other information:

 * README.md
 * LICENSE.md
 * AUTHORS.md
 * TRADEMARK.md


Changes from version 1.6.58 to version 1.6.59
---------------------------------------------

 * Fixed CVE-2026-46675 (medium severity):
   Use-after-free of zlib input in `png_read_end` after incomplete zTXt, iTXt
   or iCCP decompression.
   (Reported independently by Ze Sheng and
   <JasonHonKL@users.noreply.github.com>.)
 * Fixed a regression introduced in version 1.6.47 that caused libpng to reject
   hIST chunks in their correct position, after PLTE.
   (Contributed by Yuki Sekiguchi.)
 * Prevented a double free of `png_struct` members after an allocation failure.
   (Contributed by Anthony Hurtado.)
 * Applied fixes and updates to the CMake build.
 * Adopted the REUSE Specification for licensing the CI files.


Send comments/corrections/commendations to png-mng-implement at lists.sf.net.
Subscription is required; visit
<https://lists.sourceforge.net/lists/listinfo/png-mng-implement>
to subscribe.

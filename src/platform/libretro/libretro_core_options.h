#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <retro_inline.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 ********************************
 * Core Option Definitions
 ********************************
*/

/* RETRO_LANGUAGE_ENGLISH */

/* Default language:
 * - All other languages must include the same keys and values
 * - Will be used as a fallback in the event that frontend language
 *   is not available
 * - Will be used as a fallback for any missing entries in
 *   frontend language definition */

struct retro_core_option_definition option_defs_us[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */

   {
      "mgba_solar_sensor_level",
      "Solar sensor level",
      "Can be used by games that employed the use of a solar sensor on their cartridges. E.g: Boktai games.",
      {
         { "0", NULL },
         { "1", NULL },
         { "2",  NULL },
         { "3",  NULL },
         { "4",  NULL },
         { "5",  NULL },
         { "6",  NULL },
         { "7",  NULL },
         { "8",  NULL },
         { "9",  NULL },
         { "10",  NULL },
         { NULL, NULL},
      },
      "0"
   },
   {
      "mgba_allow_opposing_directions",
      "Allow opposing directional input",
      "Allows opposing directional inputs. Up with Down. Right with Left.",
      {
         { "OFF",         "Disabled" },
         { "ON",          "Enabled" },
         { NULL, NULL},
      },
      "OFF"
   },
   {
      "mgba_gb_model",
      "Game Boy model (requires restart)",
      "Runs loaded content with a specific Game Boy model. Autodetect will select the most appropriate model for the current game.",
      {
         { "Autodetect",     NULL },
         { "Game Boy",       NULL },
         { "Super Game Boy", NULL },
         { "Game Boy Color", NULL},
         { "Game Boy Advance", NULL},
         { NULL, NULL },
      },
      "Autodetect"
   },
   {
      "mgba_use_bios",
      "Use BIOS file if found (requires restart)",
      "Uses BIOS present in RetroArch's system directory. Look at the BIOS section for more information.",
      {
         { "ON",  "Enabled" },
         { "OFF", "Disabled" },
         { NULL, NULL},
      },
      "ON"
   },
   {
      "mgba_skip_bios",
      "Skip BIOS intro (requires restart)",
      "The 'Use BIOS file if found' core option must be set to On for proper operation. Skips the BIOS intro when a BIOS is present in RetroArch's system directory is used.",
      {
         { "OFF", "Disabled" },
         { "ON", "Enabled" },
         { NULL, NULL},
      },
      "OFF"
   },
   {
      "mgba_sgb_borders",
      "Use Super Game Boy borders (requires restart)",
      "Display Super Game Boy borders for Super Game Boy enhanced games.",
      {
         { "ON",   "Enabled" },
         { "OFF",  "Disabled" },
         { NULL, NULL},
      },
      "ON"
   },
   {
      "mgba_idle_optimization",
      "Idle loop removal",
      "Optimizes game performance by driving the GBA's CPU less hard. Use this on low-powered hardware if its struggling with game performance.",
      {
         { "Remove Known", NULL },
         { "Detect and Remove", NULL },
         { "Don't Remove", NULL },
         { NULL, NULL},
      },
      "Remove Known"
   },
   {
      "mgba_frameskip",
      "Frameskip",
      "Choose how much frames should be skipped to improve performance at the expense of visual smoothness.",
      {
         { "0", NULL },
         { "1", NULL },
         { "2", NULL },
         { "3", NULL },
         { "4", NULL },
         { "5", NULL },
         { "6", NULL },
         { "7", NULL },
         { "8", NULL },
         { "9", NULL },
         { "10", NULL },
         { NULL, NULL },
      },
      "0"
   },
#if defined(COLOR_16_BIT) && defined(COLOR_5_6_5)
   {
      "mgba_color_correction",
      "Color Correction",
      "Awaiting description",
      {
         { "OFF",  NULL },
         { "GBA",  "Game Boy Advance" },
         { "GBC",  "Game Boy Color" },
         { "Auto",  NULL },
         { NULL, NULL},
      },
      "OFF"
   },
#endif
   { NULL, NULL, NULL, {{0}}, NULL },
};

/* RETRO_LANGUAGE_JAPANESE */

/* RETRO_LANGUAGE_FRENCH */

/* RETRO_LANGUAGE_SPANISH */

/* RETRO_LANGUAGE_GERMAN */

/* RETRO_LANGUAGE_ITALIAN */

/* RETRO_LANGUAGE_DUTCH */

/* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */

/* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */

/* RETRO_LANGUAGE_RUSSIAN */

/* RETRO_LANGUAGE_KOREAN */

/* RETRO_LANGUAGE_CHINESE_TRADITIONAL */

/* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */

/* RETRO_LANGUAGE_ESPERANTO */

/* RETRO_LANGUAGE_POLISH */

/* RETRO_LANGUAGE_VIETNAMESE */

/* RETRO_LANGUAGE_ARABIC */

/* RETRO_LANGUAGE_GREEK */

/* RETRO_LANGUAGE_TURKISH */

struct retro_core_option_definition option_defs_tr[] = {

   /* These variable names and possible values constitute an ABI with ZMZ (ZSNES Libretro player).
    * Changing "Show layer 1" is fine, but don't change "layer_1"/etc or the possible values ("Yes|No").
    * Adding more variables and rearranging them is safe. */
      {
      "snes9x_region",
      "Konsol Bölgesi (Core Yenilenir)",
      "Sistemin hangi bölgeden olduğunu belirtir.. 'PAL' 50hz'dir, 'NTSC' ise 60hz. Yanlış bölge seçiliyse, oyunlar normalden daha hızlı veya daha yavaş çalışacaktır.",
      {
         { "auto", "Otomatik" },
         { "ntsc", "NTSC" },
         { "pal",  "PAL" },
         { NULL, NULL},
      },
      "auto"
   },
   {
      "snes9x_aspect",
      "Tercih Edilen En Boy Oranı",
      "Tercih edilen içerik en boy oranını seçin. Bu, yalnızca RetroArch’ın en boy oranı Video ayarlarında 'Core tarafından' olarak ayarlandığında uygulanacaktır.",
      {
         { "4:3",         NULL },
         { "uncorrected", "Düzeltilmemiş" },
         { "auto",        "Otomatik" },
         { "ntsc",        "NTSC" },
         { "pal",         "PAL" },
         { NULL, NULL},
      },
      "4:3"
   },
   {
      "snes9x_overscan",
      "Aşırı Taramayı Kırp",
      "Ekranın üst ve alt kısmındaki ~8 piksel sınırlarını, tipik olarak standart çözünürlüklü bir televizyondakini kaldırır. 'Otomatik' ise geçerli içeriğe bağlı olarak aşırı taramayı algılamaya ve kırpmaya çalışacaktır.",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { "auto",     "Otomatik" },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_gfx_hires",
      "Hi-Res Modunu Etkinleştir",
      "Oyunların hi-res moduna (512x448) geçmesine izin verir veya tüm içeriği 256x224'te (ezilmiş piksellerle) çıkmaya zorlar.",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_hires_blend",
      "Hi-Res Karışımı",
      "Oyun hi-res moduna geçtiğinde pikselleri karıştırır (512x448). Şeffaflık efektleri üretmek için hi-res modunu kullanan bazı oyunlar için gereklidir (Kirby's Dream Land, Jurassic Park ...).",
      {
         { "disabled", NULL },
         { "merge",    "Birlşetir" },
         { "blur",     "Bulanıklaştır" },
         { NULL, NULL},
      },
      "disabled"
   },
   {
      "snes9x_blargg",
      "Blargg NTSC Filtresi",
      "Çeşitli NTSC TV sinyallerini taklit etmek için bir video filtresi uygular.",
      {
         { "disabled",   NULL },
         { "monochrome", "Monochrome" },
         { "rf",         "RF" },
         { "composite",  "Composite" },
         { "s-video",    "S-Video" },
         { "rgb",        "RGB" },
         { NULL, NULL},
      },
      "disabled"
   },
   {
      "snes9x_audio_interpolation",
      "Ses Enterpolasyonu",
      "Belirtilen ses filtresini uygular. 'Gaussian', orijinal donanımın bas ağırlıklı sesini üretir. 'Cubic' ve 'Sinc' daha az doğrudur ve daha fazla aralığı korur.",
      {
         { "gaussian", "Gaussian" },
         { "cubic",    "Cubic" },
         { "sinc",     "Sinc" },
         { "none",     "Hiçbiri" },
         { "linear",   "Linear" },
         { NULL, NULL},
      },
      "gaussian"
   },
   {
      "snes9x_up_down_allowed",
      "Karşı Yönlere İzin Ver",
      "Bunu etkinleştirmek aynı anda hem sola hem de sağa (veya yukarı ve aşağı) yönlere basma / hızlı değiştirme / tutma imkanı sağlar. Bu harekete dayalı hatalara neden olabilir.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "snes9x_overclock_superfx",
      "SuperFX Hız Aşırtma",
      "SuperFX işlemcisi frekans çarpanıdır. Kare hızını artırabilir veya zamanlama hatalarına neden olabilir. % 100'ün altındaki değerler yavaş cihazlarda oyun performansını artırabilir.",
      {
         { "50%",  NULL },
         { "60%",  NULL },
         { "70%",  NULL },
         { "80%",  NULL },
         { "90%",  NULL },
         { "100%", NULL },
         { "150%", NULL },
         { "200%", NULL },
         { "250%", NULL },
         { "300%", NULL },
         { "350%", NULL },
         { "400%", NULL },
         { "450%", NULL },
         { "500%", NULL },
         { NULL, NULL},
      },
      "100%"
   },
   {
      "snes9x_overclock_cycles",
      "Yavaşlamayı Azalt (Hack, Güvensiz)",
      "SNES İşlemcisi için hız aşırtmadır. Oyunların çökmesine neden olabilir! Daha kısa yükleme süreleri için 'Hafif'i, yavaşlama gösteren oyunların çoğunda' Uyumlu 've yalnızca kesinlikle gerekliyse' Maks 'kullanın (Gradius 3, Süper R tipi ...).",
      {
         { "disabled",   NULL },
         { "light",      "Hafif" },
         { "compatible", "Uyumlu" },
         { "max",        "Maks" },
         { NULL, NULL},
      },
      "disabled"
   },
   {
      "snes9x_reduce_sprite_flicker",
      "Kırılmayı Azalt (Hack, Güvensiz)",
      "Ekranda aynı anda çizilebilen sprite sayısını arttırır.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled"
   },
   {
      "snes9x_randomize_memory",
      "Belleği Rastgele Kıl (Güvensiz)",
      "Başlatıldığında sistem RAM'ını rastgele ayarlar. 'Super Off Road' gibi bazı oyunlar, oyunu daha öngörülemeyen hale getirmek için öğe yerleştirme ve AI davranışı için rastgele sayı üreticisi olarak sistem RAM'ini kullanır.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled"
   },
   {
      "snes9x_block_invalid_vram_access",
      "Geçersiz VRAM Erişimini Engelle",
      "Bazı Homebrew/ROM'lar, doğru işlem için bu seçeneğin devre dışı bırakılmasını gerektirir.",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_echo_buffer_hack",
      "Eko Tampon Hack (Güvenli değil, yalnızca eski addmusic için etkinleştirin)",
      "Bazı Homebrew/ROM'lar, doğru işlem için bu seçeneğin devre dışı bırakılmasını gerektirir.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled"
   },
   {
      "snes9x_show_lightgun_settings",
      "Light Gun Ayarlarını Göster",
      "Super Scope / Justifier / M.A.C.S. için tüfek girişi yapılandırmasını etkinleştir. NOT: Bu ayarın etkili olabilmesi için Hızlı Menü’nün açılması gerekir.",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "disabled"
   },
   {
      "snes9x_lightgun_mode",
      "Light Gun Modu",
      "Fare kontrollü 'Light Gun' veya 'Dokunmatik Ekran' girişini kullanın.",
      {
         { "Lightgun",    "Light Gun" },
         { "Touchscreen", "Dokunmatik Ekran" },
         { NULL, NULL},
      },
      "Lightgun"
   },
   {
      "snes9x_superscope_reverse_buttons",
      "Super Scope Ters Tetik Düğmeleri",
      "Süper Scope için 'Ateş' ve 'İmleç' butonlarının pozisyonlarını değiştir.",
      {
         { "disabled", NULL },
         { "enabled",  NULL },
         { NULL, NULL},
      },
      "disabled"
   },
   {
      "snes9x_superscope_crosshair",
      "Super Scope İmkeç",
      "Ekrandaki imleç işaretini değiştirin.",
      {
         { "0",  NULL },
         { "1",  NULL },
         { "2",  NULL },
         { "3",  NULL },
         { "4",  NULL },
         { "5",  NULL },
         { "6",  NULL },
         { "7",  NULL },
         { "8",  NULL },
         { "9",  NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { "16", NULL },
         { NULL, NULL},
      },
      "2"
   },
   {
      "snes9x_superscope_color",
      "Super Scope Rengi",
      "Ekrandaki imleç işaretinin rengini değiştirin.",
      {
         { "White",            NULL },
         { "White (blend)",    NULL },
         { "Red",              NULL },
         { "Red (blend)",      NULL },
         { "Orange",           NULL },
         { "Orange (blend)",   NULL },
         { "Yellow",           NULL },
         { "Yellow (blend)",   NULL },
         { "Green",            NULL },
         { "Green (blend)",    NULL },
         { "Cyan",             NULL },
         { "Cyan (blend)",     NULL },
         { "Sky",              NULL },
         { "Sky (blend)",      NULL },
         { "Blue",             NULL },
         { "Blue (blend)",     NULL },
         { "Violet",           NULL },
         { "Violet (blend)",   NULL },
         { "Pink",             NULL },
         { "Pink (blend)",     NULL },
         { "Purple",           NULL },
         { "Purple (blend)",   NULL },
         { "Black",            NULL },
         { "Black (blend)",    NULL },
         { "25% Grey",         NULL },
         { "25% Grey (blend)", NULL },
         { "50% Grey",         NULL },
         { "50% Grey (blend)", NULL },
         { "75% Grey",         NULL },
         { "75% Grey (blend)", NULL },
         { NULL, NULL},
      },
      "White"
   },
   {
      "snes9x_justifier1_crosshair",
      "Justifier 1 İmleci",
      "Ekrandaki imleç işaretinin boyutunu değiştirin.",
      {
         { "0",  NULL },
         { "1",  NULL },
         { "2",  NULL },
         { "3",  NULL },
         { "4",  NULL },
         { "5",  NULL },
         { "6",  NULL },
         { "7",  NULL },
         { "8",  NULL },
         { "9",  NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { "16", NULL },
         { NULL, NULL},
      },
      "4"
   },
   {
      "snes9x_justifier1_color",
      "Justifier 1 Rengi",
      "Ekrandaki imleç işaretinin rengini değiştirin.",
      {
         { "Blue",             NULL },
         { "Blue (blend)",     NULL },
         { "Violet",           NULL },
         { "Violet (blend)",   NULL },
         { "Pink",             NULL },
         { "Pink (blend)",     NULL },
         { "Purple",           NULL },
         { "Purple (blend)",   NULL },
         { "Black",            NULL },
         { "Black (blend)",    NULL },
         { "25% Grey",         NULL },
         { "25% Grey (blend)", NULL },
         { "50% Grey",         NULL },
         { "50% Grey (blend)", NULL },
         { "75% Grey",         NULL },
         { "75% Grey (blend)", NULL },
         { "White",            NULL },
         { "White (blend)",    NULL },
         { "Red",              NULL },
         { "Red (blend)",      NULL },
         { "Orange",           NULL },
         { "Orange (blend)",   NULL },
         { "Yellow",           NULL },
         { "Yellow (blend)",   NULL },
         { "Green",            NULL },
         { "Green (blend)",    NULL },
         { "Cyan",             NULL },
         { "Cyan (blend)",     NULL },
         { "Sky",              NULL },
         { "Sky (blend)",      NULL },
         { NULL, NULL},
      },
      "Blue"
   },
   {
      "snes9x_justifier2_crosshair",
      "Justifier 2 İmleci",
      "Ekrandaki imleç işaretinin boyutunu değiştirin.",
      {
         { "0",  NULL },
         { "1",  NULL },
         { "2",  NULL },
         { "3",  NULL },
         { "4",  NULL },
         { "5",  NULL },
         { "6",  NULL },
         { "7",  NULL },
         { "8",  NULL },
         { "9",  NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { "16", NULL },
         { NULL, NULL},
      },
      "4"
   },
   {
      "snes9x_justifier2_color",
      "Justifier 2 REngi",
      "Ekrandaki imleç işaretinin rengini değiştirin.",
      {
         { "Pink",             NULL },
         { "Pink (blend)",     NULL },
         { "Purple",           NULL },
         { "Purple (blend)",   NULL },
         { "Black",            NULL },
         { "Black (blend)",    NULL },
         { "25% Grey",         NULL },
         { "25% Grey (blend)", NULL },
         { "50% Grey",         NULL },
         { "50% Grey (blend)", NULL },
         { "75% Grey",         NULL },
         { "75% Grey (blend)", NULL },
         { "White",            NULL },
         { "White (blend)",    NULL },
         { "Red",              NULL },
         { "Red (blend)",      NULL },
         { "Orange",           NULL },
         { "Orange (blend)",   NULL },
         { "Yellow",           NULL },
         { "Yellow (blend)",   NULL },
         { "Green",            NULL },
         { "Green (blend)",    NULL },
         { "Cyan",             NULL },
         { "Cyan (blend)",     NULL },
         { "Sky",              NULL },
         { "Sky (blend)",      NULL },
         { "Blue",             NULL },
         { "Blue (blend)",     NULL },
         { "Violet",           NULL },
         { "Violet (blend)",   NULL },
         { NULL, NULL},
      },
      "Pink"
   },
   {
      "snes9x_rifle_crosshair",
      "M.A.C.S. Tüfek ",
      "Ekrandaki imleç işaretinin rengini değiştirin..",
      {
         { "0",  NULL },
         { "1",  NULL },
         { "2",  NULL },
         { "3",  NULL },
         { "4",  NULL },
         { "5",  NULL },
         { "6",  NULL },
         { "7",  NULL },
         { "8",  NULL },
         { "9",  NULL },
         { "10", NULL },
         { "11", NULL },
         { "12", NULL },
         { "13", NULL },
         { "14", NULL },
         { "15", NULL },
         { "16", NULL },
         { NULL, NULL},
      },
      "2"
   },
   {
      "snes9x_rifle_color",
      "M.A.C.S. Tüfek Rengi",
      "Ekrandaki imleç işaretinin rengini değiştirin.",
      {
         { "White",            NULL },
         { "White (blend)",    NULL },
         { "Red",              NULL },
         { "Red (blend)",      NULL },
         { "Orange",           NULL },
         { "Orange (blend)",   NULL },
         { "Yellow",           NULL },
         { "Yellow (blend)",   NULL },
         { "Green",            NULL },
         { "Green (blend)",    NULL },
         { "Cyan",             NULL },
         { "Cyan (blend)",     NULL },
         { "Sky",              NULL },
         { "Sky (blend)",      NULL },
         { "Blue",             NULL },
         { "Blue (blend)",     NULL },
         { "Violet",           NULL },
         { "Violet (blend)",   NULL },
         { "Pink",             NULL },
         { "Pink (blend)",     NULL },
         { "Purple",           NULL },
         { "Purple (blend)",   NULL },
         { "Black",            NULL },
         { "Black (blend)",    NULL },
         { "25% Grey",         NULL },
         { "25% Grey (blend)", NULL },
         { "50% Grey",         NULL },
         { "50% Grey (blend)", NULL },
         { "75% Grey",         NULL },
         { "75% Grey (blend)", NULL },
         { NULL, NULL},
      },
      "White"
   },
   {
      "snes9x_show_advanced_av_settings",
      "Gelişmiş Ses/Video Ayarlarını Göster",
      "Düşük seviye video katmanı / GFX etkisi / ses kanalı parametrelerinin yapılandırılmasını etkinleştirir. NOT: Bu ayarın etkili olabilmesi için Hızlı Menü’nün açılması gerekir.",
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "disabled"
   },
   {
      "snes9x_layer_1",
      "1. Katmanı Göster",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_layer_2",
      "2. Katmanı Göster",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_layer_3",
      "3. Katmanı Göster",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_layer_4",
      "4. Katmanı Göster",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_layer_5",
      "Sprite Katmanını Göster",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_gfx_clip",
      "Grafik Klibi Pencerelerini Etkinleştir",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_gfx_transp",
      "Saydamlık Efektlerini Etkinleştir",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_sndchan_1",
      "Ses Kanalı 1'i etkinleştir",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_sndchan_2",
      "Ses Kanalı 2'yi etkinleştir",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_sndchan_3",
      "Ses Kanalı 3'ü etkinleştir",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_sndchan_4",
      "Ses Kanalı 4'ü etkinleştir",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_sndchan_5",
      "Ses Kanalı 5'i etkinleştir",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_sndchan_6",
      "Ses Kanalı 6'yı etkinleştir",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_sndchan_7",
      "Ses Kanalı 7'yi etkinleştir",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   {
      "snes9x_sndchan_8",
      "Ses Kanalı 8'i etkinleştir",
      NULL,
      {
         { "enabled",  NULL },
         { "disabled", NULL },
         { NULL, NULL},
      },
      "enabled"
   },
   { NULL, NULL, NULL, {{0}}, NULL },
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

struct retro_core_option_definition *option_defs_intl[RETRO_LANGUAGE_LAST] = {
   option_defs_us, /* RETRO_LANGUAGE_ENGLISH */
   NULL,           /* RETRO_LANGUAGE_JAPANESE */
   NULL,           /* RETRO_LANGUAGE_FRENCH */
   NULL,           /* RETRO_LANGUAGE_SPANISH */
   NULL,           /* RETRO_LANGUAGE_GERMAN */
   NULL,           /* RETRO_LANGUAGE_ITALIAN */
   NULL,           /* RETRO_LANGUAGE_DUTCH */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_BRAZIL */
   NULL,           /* RETRO_LANGUAGE_PORTUGUESE_PORTUGAL */
   NULL,           /* RETRO_LANGUAGE_RUSSIAN */
   NULL,           /* RETRO_LANGUAGE_KOREAN */
   NULL,           /* RETRO_LANGUAGE_CHINESE_TRADITIONAL */
   NULL,           /* RETRO_LANGUAGE_CHINESE_SIMPLIFIED */
   NULL,           /* RETRO_LANGUAGE_ESPERANTO */
   NULL,           /* RETRO_LANGUAGE_POLISH */
   NULL,           /* RETRO_LANGUAGE_VIETNAMESE */
   NULL,           /* RETRO_LANGUAGE_ARABIC */
   NULL,           /* RETRO_LANGUAGE_GREEK */
   option_defs_tr, /* RETRO_LANGUAGE_TURKISH */
};

/*
 ********************************
 * Functions
 ********************************
*/

/* Handles configuration/setting of core options.
 * Should only be called inside retro_set_environment().
 * > We place the function body in the header to avoid the
 *   necessity of adding more .c files (i.e. want this to
 *   be as painless as possible for core devs)
 */

static INLINE void libretro_set_core_options(retro_environment_t environ_cb)
{
   unsigned version = 0;

   if (!environ_cb)
      return;

   if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && (version == 1))
   {
      struct retro_core_options_intl core_options_intl;
      unsigned language = 0;

      core_options_intl.us    = option_defs_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = option_defs_intl[language];

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_intl);
   }
   else
   {
      size_t i;
      size_t option_index              = 0;
      size_t num_options               = 0;
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine number of options
       * > Note: We are going to skip a number of irrelevant
       *   core options when building the retro_variable array,
       *   but we'll allocate space for all of them. The difference
       *   in resource usage is negligible, and this allows us to
       *   keep the code 'cleaner' */
      while (true)
      {
         if (option_defs_us[num_options].key)
            num_options++;
         else
            break;
      }

      /* Allocate arrays */
      variables  = (struct retro_variable *)calloc(num_options + 1, sizeof(struct retro_variable));
      values_buf = (char **)calloc(num_options, sizeof(char *));

      if (!variables || !values_buf)
         goto error;

      /* Copy parameters from option_defs_us array */
      for (i = 0; i < num_options; i++)
      {
         const char *key                        = option_defs_us[i].key;
         const char *desc                       = option_defs_us[i].desc;
         const char *default_value              = option_defs_us[i].default_value;
         struct retro_core_option_value *values = option_defs_us[i].values;
         size_t buf_len                         = 3;
         size_t default_index                   = 0;

         values_buf[i] = NULL;

         /* Skip options that are irrelevant when using the
          * old style core options interface */
         if ((strcmp(key, "snes9x_show_lightgun_settings") == 0) ||
             (strcmp(key, "snes9x_show_advanced_av_settings") == 0))
            continue;

         if (desc)
         {
            size_t num_values = 0;

            /* Determine number of values */
            while (true)
            {
               if (values[num_values].value)
               {
                  /* Check if this is the default value */
                  if (default_value)
                     if (strcmp(values[num_values].value, default_value) == 0)
                        default_index = num_values;

                  buf_len += strlen(values[num_values].value);
                  num_values++;
               }
               else
                  break;
            }

            /* Build values string */
            if (num_values > 1)
            {
               size_t j;

               buf_len += num_values - 1;
               buf_len += strlen(desc);

               values_buf[i] = (char *)calloc(buf_len, sizeof(char));
               if (!values_buf[i])
                  goto error;

               strcpy(values_buf[i], desc);
               strcat(values_buf[i], "; ");

               /* Default value goes first */
               strcat(values_buf[i], values[default_index].value);

               /* Add remaining values */
               for (j = 0; j < num_values; j++)
               {
                  if (j != default_index)
                  {
                     strcat(values_buf[i], "|");
                     strcat(values_buf[i], values[j].value);
                  }
               }
            }
         }

         variables[option_index].key   = key;
         variables[option_index].value = values_buf[i];
         option_index++;
      }
      
      /* Set variables */
      environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, variables);

error:

      /* Clean up */
      if (values_buf)
      {
         for (i = 0; i < num_options; i++)
         {
            if (values_buf[i])
            {
               free(values_buf[i]);
               values_buf[i] = NULL;
            }
         }

         free(values_buf);
         values_buf = NULL;
      }

      if (variables)
      {
         free(variables);
         variables = NULL;
      }
   }
}

#ifdef __cplusplus
}
#endif

#endif

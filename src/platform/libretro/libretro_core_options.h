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
   {
      "mgba_solar_sensor_level",
      "Solar Sensor Level",
      "Sets ambient sunlight intensity. Can be used by games that included a solar sensor in their cartridges, e.g: the Boktai series.",
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
         { NULL, NULL },
      },
      "0"
   },
   {
      "mgba_allow_opposing_directions",
      "Allow Opposing Directional Input",
      "Enabling this will allow pressing / quickly alternating / holding both left and right (or up and down) directions at the same time. This may cause movement-based glitches.",
      {
         { "no",  "disabled" },
         { "yes", "enabled" },
         { NULL, NULL },
      },
      "no"
   },
   {
      "mgba_gb_model",
      "Game Boy Model (requires restart)",
      "Runs loaded content with a specific Game Boy model. 'Autodetect' will select the most appropriate model for the current game.",
      {
         { "Autodetect",       NULL },
         { "Game Boy",         NULL },
         { "Super Game Boy",   NULL },
         { "Game Boy Color",   NULL },
         { "Game Boy Advance", NULL },
         { NULL, NULL },
      },
      "Autodetect"
   },
   {
      "mgba_use_bios",
      "Use BIOS File if Found (requires restart)",
      "Use official BIOS/bootloader for emulated hardware, if present in RetroArch's system directory.",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "mgba_skip_bios",
      "Skip BIOS Intro (requires restart)",
      "When using an official BIOS/bootloader, skip the start-up logo animation. This setting is ignored when 'Use BIOS File if Found' is disabled.",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "mgba_sgb_borders",
      "Use Super Game Boy Borders (requires restart)",
      "Display Super Game Boy borders when running Super Game Boy enhanced games.",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "mgba_idle_optimization",
      "Idle Loop Removal",
      "Reduce system load by optimizing so-called 'idle-loops' - sections in the code where nothing happens, but the CPU runs at full speed (like a car revving in neutral). Improves performance, and should be enabled on low-end hardware.",
      {
         { "Remove Known",      NULL },
         { "Detect and Remove", NULL },
         { "Don't Remove",      NULL },
         { NULL, NULL },
      },
      "Remove Known"
   },
   {
      "mgba_frameskip",
      "Frameskip",
      "Skip frames to improve performance at the expense of visual smoothness. Value set here is the number of frames omitted after a frame is rendered - i.e. '0' = 60fps, '1' = 30fps, '2' = 15fps, etc.",
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
         { NULL, NULL },
      },
      "0"
   },
#if defined(COLOR_16_BIT) && defined(COLOR_5_6_5)
   {
      "mgba_color_correction",
      "Color Correction",
      "Adjusts output colors to match the display of real GBA/GBC hardware.",
      {
         { "OFF",  NULL },
         { "GBA",  "Game Boy Advance" },
         { "GBC",  "Game Boy Color" },
         { "Auto", NULL },
         { NULL, NULL },
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
   {
      "mgba_solar_sensor_level",
      "Güneş Sensörü Seviyesi",
      "Ortam güneş ışığının yoğunluğunu ayarlar. Boktai serisi, kartuşlarına güneş sensörü içeren oyunlar tarafından kullanılabilir.",
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
         { NULL, NULL },
      },
      "0"
   },
   {
      "mgba_allow_opposing_directions",
      "Karşı Yönlü Girdiye Çıkmaya İzin Ver",
      "Bunu etkinleştirmek aynı anda hem sola hem de sağa (veya yukarı ve aşağı) yönlere basma / hızlı değiştirme / tutma imkanı sağlar. Bu harekete dayalı hatalara neden olabilir.",
      {
         { "no",  "Devre dışı bırak" },
         { "yes", "Etkileştir" },
         { NULL, NULL },
      },
      "no"
   },
   {
      "mgba_gb_model",
      "Game Boy Modeli (yeniden başlatma gerektirir)",
      "Yüklenen içeriği belirli bir Game Boy modeliyle çalıştırır. 'Otomatik Tespit' mevcut oyun için en uygun modeli seçecektir.",
      {
         { "Autodetect",       "Otomatik Tespit" },
         { "Game Boy",         NULL },
         { "Super Game Boy",   NULL },
         { "Game Boy Color",   NULL },
         { "Game Boy Advance", NULL },
         { NULL, NULL },
      },
      "Autodetect"
   },
   {
      "mgba_use_bios",
      "Bulunursa BIOS Dosyasını kullanın (yeniden başlatma gerektirir)",
      "RetroArch'ın sistem dizininde varsa, öykünülmüş donanım için resmi BIOS/önyükleyici kullanır.",
      {
         { "ON",  "AÇIK" },
         { "OFF", "KAPALI" },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "mgba_skip_bios",
      "BIOS Girişini Atla (yeniden başlatma gerektirir)",
      "Resmi bir BIOS / önyükleyici kullanırken, başlangıç logosu animasyonunu atlayın. Bu ayar, 'Bulunursa BIOS Dosyasını Kullan' devre dışı bırakıldığında geçersiz sayılır.",
      {
         { "OFF", "HAYIR" },
         { "ON",  "AÇIK" },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "mgba_sgb_borders",
      "Super Game Boy Sınırlarını kullanın (yeniden başlatma gerekir)",
      "Super Game Boy gelişmiş oyunlarını çalıştırırken Super Game Boy sınırlarını görüntüleR.",
      {
         { "ON",  "AÇIK" },
         { "OFF", "KAPALI" },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "mgba_idle_optimization",
      "Boşta Döngü Kaldırma",
      "'Boşta döngüler' denilen sistemi optimize ederek sistem yükünü azaltın - hiçbir şeyin olmadığı koddaki bölümler için, CPU tam hızda çalıştırır (boşa dönen bir araba gibi). Performansı arttırır ve düşük kaliteli donanımlarda etkinleştirilmesi gerekir.",
      {
         { "Remove Known",      "Bilinenleri Kaldır" },
         { "Detect and Remove", "Algıla ve Kaldır" },
         { "Don't Remove",      "Kaldırma" },
         { NULL, NULL },
      },
      "Remove Known"
   },
   {
      "mgba_frameskip",
      "Kare atlama",
      "Görsel pürüzsüzlük pahasına performansı artırmak için çerçeveleri atlayın. Burada ayarlanan değer, bir kare oluşturulduktan sonra atlanan kare sayısıdır - yani '0' = 60fps, '1' = 30fps, '2' = 15fps, vb.",
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
         { NULL, NULL },
      },
      "0"
   },
#if defined(COLOR_16_BIT) && defined(COLOR_5_6_5)
   {
      "mgba_color_correction",
      "Renk Düzeltmesi",
      "Çıktı renklerini gerçek GBA / GBC donanımının görüntüsüyle eşleşecek şekilde ayarlar.",
      {
         { "OFF",  "KAPALI" },
         { "GBA",  "Game Boy Advance" },
         { "GBC",  "Game Boy Color" },
         { "Auto", "Otomatik" },
         { NULL, NULL },
      },
      "OFF"
   },
#endif
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
 * Should be called as early as possible - ideally inside
 * retro_set_environment(), and no later than retro_load_game()
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
      size_t num_options               = 0;
      struct retro_variable *variables = NULL;
      char **values_buf                = NULL;

      /* Determine number of options */
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

         variables[i].key   = key;
         variables[i].value = values_buf[i];
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

#ifndef LIBRETRO_CORE_OPTIONS_H__
#define LIBRETRO_CORE_OPTIONS_H__

#include <stdlib.h>
#include <string.h>

#include "libretro.h"
#include "retro_inline.h"

#ifndef HAVE_NO_LANGEXTRA
#include "libretro_core_options_intl.h"
#endif

/*
 ********************************
 * VERSION: 1.3
 ********************************
 *
 * - 1.3: Move translations to libretro_core_options_intl.h
 *        - libretro_core_options_intl.h includes BOM and utf-8
 *          fix for MSVC 2010-2013
 *        - Added HAVE_NO_LANGEXTRA flag to disable translations
 *          on platforms/compilers without BOM support
 * - 1.2: Use core options v1 interface when
 *        RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION is >= 1
 *        (previously required RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION == 1)
 * - 1.1: Support generation of core options v0 retro_core_option_value
 *        arrays containing options with a single value
 * - 1.0: First commit
*/

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
         { "sensor",  "Use device sensor if available" },
         { "0",       NULL },
         { "1",       NULL },
         { "2",       NULL },
         { "3",       NULL },
         { "4",       NULL },
         { "5",       NULL },
         { "6",       NULL },
         { "7",       NULL },
         { "8",       NULL },
         { "9",       NULL },
         { "10",      NULL },
         { NULL,      NULL },
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
      "Skip frames to avoid audio buffer under-run (crackling). Improves performance at the expense of visual smoothness. 'Auto' skips frames when advised by the frontend. 'Auto (Threshold)' utilises the 'Frameskip Threshold (%)' setting. 'Fixed Interval' utilises the 'Frameskip Interval' setting.",
      {
         { "disabled",       NULL },
         { "auto",           "Auto" },
         { "auto_threshold", "Auto (Threshold)" },
         { "fixed_interval", "Fixed Interval" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "mgba_frameskip_threshold",
      "Frameskip Threshold (%)",
      "When 'Frameskip' is set to 'Auto (Threshold)', specifies the audio buffer occupancy threshold (percentage) below which frames will be skipped. Higher values reduce the risk of crackling by causing frames to be dropped more frequently.",
      {
         { "15", NULL },
         { "18", NULL },
         { "21", NULL },
         { "24", NULL },
         { "27", NULL },
         { "30", NULL },
         { "33", NULL },
         { "36", NULL },
         { "39", NULL },
         { "42", NULL },
         { "45", NULL },
         { "48", NULL },
         { "51", NULL },
         { "54", NULL },
         { "57", NULL },
         { "60", NULL },
         { NULL, NULL },
      },
      "33"
   },
   {
      "mgba_frameskip_interval",
      "Frameskip Interval",
      "When 'Frameskip' is set to 'Fixed Interval', the value set here is the number of frames omitted after a frame is rendered - i.e. '0' = 60fps, '1' = 30fps, '2' = 15fps, etc.",
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
   {
      "mgba_interframe_blending",
      "Interframe Blending",
      "Simulates LCD ghosting effects. 'Simple' performs a 50:50 mix of the current and previous frames. 'Smart' attempts to detect screen flickering, and only performs a 50:50 mix on affected pixels. 'LCD Ghosting' mimics natural LCD response times by combining multiple buffered frames. 'Simple' or 'Smart' blending is required when playing games that aggressively exploit LCD ghosting for transparency effects (Wave Race, Chikyuu Kaihou Gun ZAS, F-Zero, the Boktai series...).",
      {
         { "OFF",               NULL },
         { "mix",               "Simple" },
         { "mix_smart",         "Smart" },
         { "lcd_ghosting",      "LCD Ghosting (Accurate)" },
         { "lcd_ghosting_fast", "LCD Ghosting (Fast)" },
         { NULL, NULL },
      },
      "OFF"
   },
#endif
   {
      "mgba_force_gbp",
      "Enable Game Boy Player Rumble (requires restart)",
      "Enabling this will allow compatible games with the Game Boy Player boot logo to make the controller rumble. Due to how Nintendo decided this feature should work, it may cause glitches such as flickering or lag in some of these games.",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   { NULL, NULL, NULL, {{0}}, NULL },
};

/*
 ********************************
 * Language Mapping
 ********************************
*/

#ifndef HAVE_NO_LANGEXTRA
struct retro_core_option_definition *option_defs_intl[RETRO_LANGUAGE_LAST] = {
   option_defs_us, /* RETRO_LANGUAGE_ENGLISH */
   NULL,           /* RETRO_LANGUAGE_JAPANESE */
   NULL,           /* RETRO_LANGUAGE_FRENCH */
   option_defs_es, /* RETRO_LANGUAGE_SPANISH */
   NULL,           /* RETRO_LANGUAGE_GERMAN */
   option_defs_it, /* RETRO_LANGUAGE_ITALIAN */
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
#endif

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

   if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && (version >= 1))
   {
#ifndef HAVE_NO_LANGEXTRA
      struct retro_core_options_intl core_options_intl;
      unsigned language = 0;

      core_options_intl.us    = option_defs_us;
      core_options_intl.local = NULL;

      if (environ_cb(RETRO_ENVIRONMENT_GET_LANGUAGE, &language) &&
          (language < RETRO_LANGUAGE_LAST) && (language != RETRO_LANGUAGE_ENGLISH))
         core_options_intl.local = option_defs_intl[language];

      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, &core_options_intl);
#else
      environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS, &option_defs_us);
#endif
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
            if (num_values > 0)
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

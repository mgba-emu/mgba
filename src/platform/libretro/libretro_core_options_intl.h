#ifndef LIBRETRO_CORE_OPTIONS_INTL_H__
#define LIBRETRO_CORE_OPTIONS_INTL_H__

#if defined(_MSC_VER) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#pragma warning(disable:4566)
#endif

#include "libretro.h"

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

/* RETRO_LANGUAGE_JAPANESE */

/* RETRO_LANGUAGE_FRENCH */

/* RETRO_LANGUAGE_SPANISH */

/* RETRO_LANGUAGE_GERMAN */

/* RETRO_LANGUAGE_ITALIAN */

struct retro_core_option_definition option_defs_it[] = {
   {
      "mgba_solar_sensor_level",
      "Livello Sensore Solare",
      "Imposta l'intensità solare dell'ambiente. Può essere usato dai giochi che includono un sensore solare nelle loro cartucce, es.: la serie Boktai.",
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
      "Permetti Input Direzionali Opposti",
      "Attivando questa funzionalità ti permette di premere / alternare velocemente / tenere premuti entrambe le direzioni destra e sinistra (oppure su e giù) allo stesso momento. Potrebbe causare dei glitch di movimento.",
      {
         { "no",  "Disabilitato" },
         { "yes", "Abilitato" },
         { NULL, NULL },
      },
      "no"
   },
   {
      "mgba_gb_model",
      "Modello Game Boy (richiede riavvio)",
      "Esegue il contenuto caricato con un modello specifico di Game Boy. 'Rivela Automaticamente' selezionerà il modello più appropriato per il gioco attuale.",
      {
         { "Autodetect",       "Rivela Automaticamente" },
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
      "Usa il File BIOS se Presente (richiede riavvio)",
      "Usa il BIOS/bootloader ufficiale per hardware emulato, se presente nella cartella di sistema di RetroArch.",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "mgba_skip_bios",
      "Salta Intro BIOS (richiede riavvio)",
      "Salta il filmato del logo di avvio se si usa un BIOS/bootloader ufficiale. Questa impostazione è ignorata se 'Usa il file BIOS se presente' è disabilitato.",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "mgba_sgb_borders",
      "Utilizza i Bordi Super Game Boy (richiede riavvio)",
      "Visualizza i bordi del Super Game Boy quando apri un gioco potenziato dal Super Game Boy.",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "mgba_idle_optimization",
      "Rimozione Idle Loop",
      "Riduce il carico del sistema ottimizzando gli 'idle-loops' - sezione del codice dove non accade nulla, ma la CPU lavora a velocità massima. Migliora le prestazioni, è consigliato abilitarlo su hardware di bassa fascia.",
      {
         { "Remove Known",      "Rimuovi Conosciuti" },
         { "Detect and Remove", "Rileva e Rimuovi" },
         { "Don't Remove",      "Non Rimuovere" },
         { NULL, NULL },
      },
      "Remove Known"
   },
#if defined(COLOR_16_BIT) && defined(COLOR_5_6_5)
   {
      "mgba_color_correction",
      "Correzione Colore",
      "Regola i colori per corrispondere lo schermo di GBA/GBC reali.",
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
         { "sensor",  "Sensörü" },
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

#ifdef __cplusplus
}
#endif

#endif

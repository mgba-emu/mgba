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

struct retro_core_option_definition option_defs_es[] = {
   {
      "mgba_solar_sensor_level",
      "Nivel del sensor solar",
      "Ajusta la intensidad de la luz solar ambiental. Para juegos que contenían un sensor solar en sus cartuchos, p. ej.: la saga Boktai.",
      {
         { "sensor",  "Utilizar dispositivo sensor si está disponible" },
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
      "Permitir entradas direccionales opuestas",
      "Permite pulsar, alternar rápidamente o mantener las direcciones hacia la izquierda y hacia la derecha (o hacia arriba y abajo) al mismo tiempo. Puede provocar defectos en el movimiento.",
      {
         { "no",  "No" },
         { "yes", "Sí" },
         { NULL, NULL },
      },
      "no"
   },
   {
      "mgba_gb_model",
      "Modelo de Game Boy (es necesario reiniciar)",
      "Carga el contenido cargado utilizando un modelo de Game Boy específico. La opción «Autodetectar» seleccionará el modelo más adecuado para el juego actual.",
      {
         { "Autodetect",       "Autodetectar" },
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
      "Utilizar BIOS en caso de encontrarla (es necesario reiniciar)",
      "Si se encuentran en el directorio de sistema de RetroArch, se utilizarán los archivos de la BIOS y el bootloader oficiales para emular el hardware.",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "mgba_skip_bios",
      "Omitir introducción de la BIOS (es necesario reiniciar)",
      "Al utilizar una BIOS y bootloader oficiales, omitirá la animación del logotipo al arrancar. Esta opción será ignorada si «Utilizar BIOS en caso de encontrarla» está desactivada.",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "mgba_sgb_borders",
      "Utilizar bordes de Super Game Boy (es necesario reiniciar)",
      "Muestra los bordes de Super Game Boy al ejecutar juegos compatibles con este sistema.",
      {
         { "ON",  NULL },
         { "OFF", NULL },
         { NULL, NULL },
      },
      "ON"
   },
   {
      "mgba_idle_optimization",
      "Eliminar bucle de inactividad",
      "Minimiza la carga del sistema optimizando los llamados bucles de inactividad: secciones de código en las que no ocurre nada, pero la CPU se ejecuta a máxima velocidad (como cuando un vehículo es revolucionado sin tener la marcha puesta). Mejora el rendimiento y debería activarse en hardware de bajas prestaciones.",
      {
         { "Remove Known",      "Eliminar bucles conocidos" },
         { "Detect and Remove", "Detectar y eliminar" },
         { "Don't Remove",      "No eliminar" },
         { NULL, NULL },
      },
      "Remove Known"
   },
   {
      "mgba_frameskip",
      "Omisión de fotogramas",
      "Omite fotogramas para no saturar el búfer de audio (chasquidos en el sonido). Mejora el rendimiento a costa de perder fluidez visual. El valor Automática omite fotogramas según lo aconseje el front-end. El valor Automática (umbral) utiliza el ajuste Umbral de omisión de fotogramas (%). El valor «Intervalos fijos» utiliza el ajuste «Intervalo de omisión de fotogramas».",
      {
         { "disabled",       "Desactivada" },
         { "auto",           "Automática" },
         { "auto_threshold", "Automática (umbral)" },
         { "fixed_interval", "Intervalos fijos" },
         { NULL, NULL },
      },
      "disabled"
   },
   {
      "mgba_frameskip_threshold",
      "Umbral de omisión de fotogramas (%)",
      "Cuando la omisión de fotogramas esté configurada como Automática (umbral), este ajuste especifica el umbral de ocupación del búfer de audio (en porcentaje) por el que se omitirán fotogramas si el valor es inferior. Un valor más elevado reduce el riesgo de chasquidos omitiendo fotogramas con una mayor frecuencia.",
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
      "Intervalo de omisión de fotogramas",
      "Cuando la omisión de fotogramas esté configurada como Intervalos fijos, el valor que se asigne aquí será el número de fotogramas omitidos una vez se haya renderizado un fotograma. Por ejemplo: «0» = 60 FPS, «1» = 30 FPS, «2» = 15 FPS, etcétera.",
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
      "Corrección de color",
      "Ajusta los colores de la salida de imagen para que esta coincida con la que mostraría un hardware real de GBA/GBC.",
      {
         { "OFF",  NULL },
         { "GBA",  "Game Boy Advance" },
         { "GBC",  "Game Boy Color" },
         { "Auto", "Automática" },
         { NULL, NULL },
      },
      "OFF"
   },
   {
      "mgba_interframe_blending",
      "Fusión interfotograma",
      "Simula el efecto fantasma de una pantalla LCD. «Sencilla» mezcla los fotogramas previos y posteriores en un 50%. «Inteligente» intenta detectar los parpadeos de pantalla y solo lleva a cabo la fusión del 50% en los fotogramas afectados. «Efecto fantasma de LCD» imita los tiempos de respuesta naturales de una pantalla LCD combinando varios fotogramas guardados en el búfer. La fusión sencilla o inteligente es necesaria en aquellos juegos que aprovechan de forma agresiva el efecto fantasma de la pantalla LCD para los efectos de transparencia (Wave Race, Chikyuu Kaihou Gun ZAS, F-Zero, la saga Boktai...).",
      {
         { "OFF",               NULL },
         { "mix",               "Sencilla" },
         { "mix_smart",         "Inteligente" },
         { "lcd_ghosting",      "Efecto fantasma de LCD (preciso)" },
         { "lcd_ghosting_fast", "Efecto fantasma de LCD (rápido)" },
         { NULL, NULL },
      },
      "OFF"
   },
#endif
   {
      "mgba_force_gbp",
      "Vibración de Game Boy Player (es necesario reiniciar)",
      "Permite que aquellos juegos compatibles con el logotipo de arranque de Game Boy Player hagan vibrar el mando. Debido a el método que utilizó Nintendo, puede provocar fallos gráficos, como parpadeos o retrasos de señal en algunos de estos juegos.",
      {
         { "OFF", NULL },
         { "ON",  NULL },
         { NULL, NULL },
      },
      "OFF"
   },
   { NULL, NULL, NULL, {{0}}, NULL },
};

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
   {
      "mgba_frameskip",
      "Salta Frame",
      "Salta dei frame per migliorare le prestazioni a costo della fluidità dell'immagine. Il valore impostato qui è il numero dei frame rimosso dopo che un frame sia stato renderizzato - ovvero '0' = 60fps, '1' = 30fps, '2' = 15fps, ecc.",
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

/* Copyright (c) 2013-2015 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba/internal/gb/overrides.h>

#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/mbc.h>

#include <mgba-util/configuration.h>
#include <mgba-util/crc32.h>

#define PAL_ENTRY(A, B, C, D) \
	0xFF000000 | M_RGB5_TO_RGB8(A), \
	0xFF000000 | M_RGB5_TO_RGB8(B), \
	0xFF000000 | M_RGB5_TO_RGB8(C), \
	0xFF000000 | M_RGB5_TO_RGB8(D)

#define PAL0  PAL_ENTRY(0x7FFF, 0x32BF, 0x00D0, 0x0000)
#define PAL1  PAL_ENTRY(0x639F, 0x4279, 0x15B0, 0x04CB)
#define PAL2  PAL_ENTRY(0x7FFF, 0x6E31, 0x454A, 0x0000)
#define PAL3  PAL_ENTRY(0x7FFF, 0x1BEF, 0x0200, 0x0000)
#define PAL4  PAL_ENTRY(0x7FFF, 0x421F, 0x1CF2, 0x0000)
#define PAL5  PAL_ENTRY(0x7FFF, 0x5294, 0x294A, 0x0000)
#define PAL6  PAL_ENTRY(0x7FFF, 0x03FF, 0x012F, 0x0000)
#define PAL7  PAL_ENTRY(0x7FFF, 0x03EF, 0x01D6, 0x0000)
#define PAL8  PAL_ENTRY(0x7FFF, 0x42B5, 0x3DC8, 0x0000)
#define PAL9  PAL_ENTRY(0x7E74, 0x03FF, 0x0180, 0x0000)
#define PAL10 PAL_ENTRY(0x67FF, 0x77AC, 0x1A13, 0x2D6B)
#define PAL11 PAL_ENTRY(0x7ED6, 0x4BFF, 0x2175, 0x0000)
#define PAL12 PAL_ENTRY(0x53FF, 0x4A5F, 0x7E52, 0x0000)
#define PAL13 PAL_ENTRY(0x4FFF, 0x7ED2, 0x3A4C, 0x1CE0)
#define PAL14 PAL_ENTRY(0x03ED, 0x7FFF, 0x255F, 0x0000)
#define PAL15 PAL_ENTRY(0x036A, 0x021F, 0x03FF, 0x7FFF)
#define PAL16 PAL_ENTRY(0x7FFF, 0x01DF, 0x0112, 0x0000)
#define PAL17 PAL_ENTRY(0x231F, 0x035F, 0x00F2, 0x0009)
#define PAL18 PAL_ENTRY(0x7FFF, 0x03EA, 0x011F, 0x0000)
#define PAL19 PAL_ENTRY(0x299F, 0x001A, 0x000C, 0x0000)
#define PAL20 PAL_ENTRY(0x7FFF, 0x027F, 0x001F, 0x0000)
#define PAL21 PAL_ENTRY(0x7FFF, 0x03E0, 0x0206, 0x0120)
#define PAL22 PAL_ENTRY(0x7FFF, 0x7EEB, 0x001F, 0x7C00)
#define PAL23 PAL_ENTRY(0x7FFF, 0x3FFF, 0x7E00, 0x001F)
#define PAL24 PAL_ENTRY(0x7FFF, 0x03FF, 0x001F, 0x0000)
#define PAL25 PAL_ENTRY(0x03FF, 0x001F, 0x000C, 0x0000)
#define PAL26 PAL_ENTRY(0x7FFF, 0x033F, 0x0193, 0x0000)
#define PAL27 PAL_ENTRY(0x0000, 0x4200, 0x037F, 0x7FFF)
#define PAL28 PAL_ENTRY(0x7FFF, 0x7E8C, 0x7C00, 0x0000)
#define PAL29 PAL_ENTRY(0x7FFF, 0x1BEF, 0x6180, 0x0000)
#define PAL30 PAL_ENTRY(0x7C00, 0x7FFF, 0x3FFF, 0x7E00)
#define PAL31 PAL_ENTRY(0x7FFF, 0x7FFF, 0x7E8C, 0x7C00)
#define PAL32 PAL_ENTRY(0x0000, 0x7FFF, 0x421F, 0x1CF2)

#define PAL1A PAL_ENTRY(0x67BF, 0x265B, 0x10B5, 0x2866)
#define PAL1B PAL_ENTRY(0x637B, 0x3AD9, 0x0956, 0x0000)
#define PAL1C PAL_ENTRY(0x7F1F, 0x2A7D, 0x30F3, 0x4CE7)
#define PAL1D PAL_ENTRY(0x57FF, 0x2618, 0x001F, 0x006A)
#define PAL1E PAL_ENTRY(0x5B7F, 0x3F0F, 0x222D, 0x10EB)
#define PAL1F PAL_ENTRY(0x7FBB, 0x2A3C, 0x0015, 0x0900)
#define PAL1G PAL_ENTRY(0x2800, 0x7680, 0x01EF, 0x2FFF)
#define PAL1H PAL_ENTRY(0x73BF, 0x46FF, 0x0110, 0x0066)
#define PAL2A PAL_ENTRY(0x533E, 0x2638, 0x01E5, 0x0000)
#define PAL2B PAL_ENTRY(0x7FFF, 0x2BBF, 0x00DF, 0x2C0A)
#define PAL2C PAL_ENTRY(0x7F1F, 0x463D, 0x74CF, 0x4CA5)
#define PAL2D PAL_ENTRY(0x53FF, 0x03E0, 0x00DF, 0x2800)
#define PAL2E PAL_ENTRY(0x433F, 0x72D2, 0x3045, 0x0822)
#define PAL2F PAL_ENTRY(0x7FFA, 0x2A5F, 0x0014, 0x0003)
#define PAL2G PAL_ENTRY(0x1EED, 0x215C, 0x42FC, 0x0060)
#define PAL2H PAL_ENTRY(0x7FFF, 0x5EF7, 0x39CE, 0x0000)
#define PAL3A PAL_ENTRY(0x4F5F, 0x630E, 0x159F, 0x3126)
#define PAL3B PAL_ENTRY(0x637B, 0x121C, 0x0140, 0x0840)
#define PAL3C PAL_ENTRY(0x66BC, 0x3FFF, 0x7EE0, 0x2C84)
#define PAL3D PAL_ENTRY(0x5FFE, 0x3EBC, 0x0321, 0x0000)
#define PAL3E PAL_ENTRY(0x63FF, 0x36DC, 0x11F6, 0x392A)
#define PAL3F PAL_ENTRY(0x65EF, 0x7DBF, 0x035F, 0x2108)
#define PAL3G PAL_ENTRY(0x2B6C, 0x7FFF, 0x1CD9, 0x0007)
#define PAL3H PAL_ENTRY(0x53FC, 0x1F2F, 0x0E29, 0x0061)
#define PAL4A PAL_ENTRY(0x36BE, 0x7EAF, 0x681A, 0x3C00)
#define PAL4B PAL_ENTRY(0x7BBE, 0x329D, 0x1DE8, 0x0423)
#define PAL4C PAL_ENTRY(0x739F, 0x6A9B, 0x7293, 0x0001)
#define PAL4D PAL_ENTRY(0x5FFF, 0x6732, 0x3DA9, 0x2481)
#define PAL4E PAL_ENTRY(0x577F, 0x3EBC, 0x456F, 0x1880)
#define PAL4F PAL_ENTRY(0x6B57, 0x6E1B, 0x5010, 0x0007)
#define PAL4G PAL_ENTRY(0x0F96, 0x2C97, 0x0045, 0x3200)
#define PAL4H PAL_ENTRY(0x67FF, 0x2F17, 0x2230, 0x1548)

#define PALETTE(X, Y, Z) { PAL ## X, PAL ## Y, PAL ## Z }
#define UNIFORM_PAL(A, B, C, D) { PAL_ENTRY(A, B, C, D), PAL_ENTRY(A, B, C, D), PAL_ENTRY(A, B, C, D) }
#define SGB_PAL(A) { PAL ## A, PAL ## A, PAL ## A }

static const struct GBCartridgeOverride _gbcOverrides[] = {
	// Adventures of Lolo (Europe)
	{ 0xFBE65286, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 3) },

	// Alleyway (World)
	{ 0xCBAA161B, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(9, 9, 9) },

	// Arcade Classic No. 1 - Asteroids & Missile Command (USA, Europe)
	{ 0x309FDB70, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(3, 4, 4) },

	// Arcade Classic No. 3 - Galaga & Galaxian (USA)
	{ 0xE13EF629, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(27, 27, 27) },

	// Arcade Classic No. 4 - Defender & Joust (USA, Europe)
	{ 0x5C8B229D, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 3) },

	// Balloon Kid (USA, Europe)
	{ 0xEC3438FA, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(20, 20, 20) },

	// Baseball (World)
	{ 0xE02904BD, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(15, 31, 4) },

	// Battle Arena Toshinden (USA)
	{ 0xA2C3DF62, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 3) },

	// Battletoads in Ragnarok's World (Europe)
	{ 0x51B259CF, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 3, 3) },

	// Chessmaster, The (DMG-EM) (Europe)
	{ 0x96A68366, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 28) },

	// David Crane's The Rescue of Princess Blobette Starring A Boy and His Blob (Europe)
	{ 0x6413F5E2, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 3, 28) },

	// Donkey Kong (Japan, USA)
	{ 0xA777EE2F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(20, 4, 4) },

	// Donkey Kong (World) (Rev A)
	{ 0xC8F8ACDA, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(20, 4, 4) },

	// Donkey Kong Land (Japan)
	{ 0x2CA7EEF3, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(2, 17, 22) },

	// Donkey Kong Land (USA, Europe)
	{ 0x0D3E401D, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(13, 17, 4) },

	// Donkey Kong Land 2 (USA, Europe)
	{ 0x07ED9445, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(2, 17, 22) },

	// Donkey Kong Land III (USA, Europe)
	{ 0xCA01A31C, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(2, 17, 22) },

	// Donkey Kong Land III (USA, Europe) (Rev A)
	{ 0x6805BA1E, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(2, 17, 22) },

	// Dr. Mario (World)
	{ 0xA3C2C1E9, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(28, 28, 4) },

	// Dr. Mario (World) (Rev A)
	{ 0x69975661, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(28, 28, 4) },

	// Dr. Mario (World) (Beta)
	{ 0x22E55535, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(9, 19, 30) },

	// Dynablaster (Europe)
	{ 0xD9D0211F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 28) },

	// F-1 Race (World)
	{ 0x8434CB2C, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 0, 0) },

	// F-1 Race (World) (Rev A)
	{ 0xBA63383B, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 0, 0) },

	// Game & Watch Gallery (Europe)
	{ 0x4A43B8B9, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(7, 4, 4) },

	// Game & Watch Gallery (USA)
	{ 0xBD0736D4, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(7, 4, 4) },

	// Game & Watch Gallery (USA) (Rev A)
	{ 0xA969B4F0, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(7, 4, 4) },

	// Game Boy Camera Gold (USA)
	{ 0x83947EC8, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 3, 4) },

	// Game Boy Gallery (Japan)
	{ 0xDC3C3642, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(7, 4, 4) },

	// Game Boy Gallery - 5 Games in One (Europe)
	{ 0xD83E3F82, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 0, 0) },

	// Game Boy Gallery 2 (Australia)
	{ 0x6C477A30, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(7, 4, 4) },

	// Game Boy Gallery 2 (Japan)
	{ 0xC5AAAFDA, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(7, 4, 4) },

	// Game Boy Wars (Japan)
	{ 0x03E3ED72, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(8, 16, 22) },

	// Golf (World)
	{ 0x885C242D, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(3, 4, 4) },

	// Hoshi no Kirby (Japan)
	{ 0x4AA02A13, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(9, 19, 30) },

	// Hoshi no Kirby (Japan) (Rev A)
	{ 0x88D03280, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(9, 19, 30) },

	// Hoshi no Kirby 2 (Japan)
	{ 0x58B7A321, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(9, 19, 30) },

	// James Bond 007 (USA, Europe)
	{ 0x7DDEB68E, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(29, 4, 29) },

	// Kaeru no Tame ni Kane wa Naru (Japan)
	{ 0x7F805941, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(2, 4, 2) },

	// Kid Icarus - Of Myths and Monsters (USA, Europe)
	{ 0x5D93DB0F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(2, 4, 4) },

	// Killer Instinct (USA, Europe)
	{ 0x117043A9, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(2, 4, 0) },

	// King of Fighters '95, The (USA)
	{ 0x0F81CC70, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 3) },

	// King of the Zoo (Europe)
	{ 0xB492FB51, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 28) },

	// Kirby no Block Ball (Japan)
	{ 0x4203B79F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(9, 19, 30) },

	// Kirby no Kirakira Kids (Japan)
	{ 0x74C3A937, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 0, 0) },

	// Kirby no Pinball (Japan)
	{ 0x89239AED, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(9, 19, 19) },

	// Kirby's Block Ball (USA, Europe)
	{ 0xCE8B1B18, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(9, 19, 30) },

	// Kirby's Dream Land (USA, Europe)
	{ 0x302017CC, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(9, 19, 30) },

	// Kirby's Dream Land 2 (USA, Europe)
	{ 0xF6C9E5A8, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(9, 19, 30) },

	// Kirby's Pinball Land (USA, Europe)
	{ 0x9C4AA9D8, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(9, 19, 19) },

	// Kirby's Star Stacker (USA, Europe)
	{ 0xC1B481CA, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 0, 0) },

	// Legend of Zelda, The - Link's Awakening (Canada)
	{ 0x9F54D47B, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 21, 28) },

	// Legend of Zelda, The - Link's Awakening (France)
	{ 0x441D7FAD, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 21, 28) },

	// Legend of Zelda, The - Link's Awakening (Germany)
	{ 0x838D65D6, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 21, 28) },

	// Legend of Zelda, The - Link's Awakening (USA, Europe) (Rev A)
	{ 0x24CAAB4D, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 21, 28) },

	// Legend of Zelda, The - Link's Awakening (USA, Europe) (Rev B)
	{ 0xBCBB6BDB, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 21, 28) },

	// Legend of Zelda, The - Link's Awakening (USA, Europe)
	{ 0x9A193109, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 21, 28) },

	// Magnetic Soccer (Europe)
	{ 0x6735A1F5, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(3, 4, 28) },

	// Mario & Yoshi (Europe)
	{ 0xEC14B007, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(18, 4, 4) },

	// Mario no Picross (Japan)
	{ 0x602C2371, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 0, 0) },

	// Mario's Picross (USA, Europe)
	{ 0x725BBFF6, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 0, 0) },

	// Mega Man - Dr. Wily's Revenge (Europe)
	{ 0xB2FE1EDB, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 3) },

	// Mega Man II (Europe)
	{ 0xC5EE1580, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 3) },

	// Mega Man III (Europe)
	{ 0x88249B90, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 3) },

	// Metroid II - Return of Samus (World)
	{ 0xBDCCC648, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(28, 25, 3) },

	// Moguranya (Japan)
	{ 0x41C1D13C, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(8, 16, 16) },

	// Mole Mania (USA, Europe)
	{ 0x32E8EEA3, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(8, 16, 16) },

	// Mystic Quest (Europe)
	{ 0x8DC57012, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(3, 4, 28) },

	// Mystic Quest (France)
	{ 0x09728780, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(3, 4, 28) },

	// Mystic Quest (Germany)
	{ 0x6F8568A8, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(3, 4, 28) },

	// Nigel Mansell's World Championship Racing (Europe)
	{ 0xAC2D636D, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 0, 0) },

	// Nintendo World Cup (USA, Europe)
	{ 0xB43E44C1, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(3, 4, 4) },

	// Othello (Europe)
	{ 0x45F34317, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(3, 4, 28) },

	// Pac-In-Time (Europe)
	{ 0x8C608574, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(29, 4, 28) },

	// Picross 2 (Japan)
	{ 0xBA91DDD8, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 0, 0) },

	// Pinocchio (Europe)
	{ 0x849C74C0, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(2, 2, 17) },

	// Play Action Football (USA)
	{ 0x2B703514, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(3, 4, 4) },

	// Pocket Bomberman (Europe)
	{ 0x9C5E0D5E, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(2, 17, 17) },

	// Pocket Camera (Japan) (Rev A)
	{ 0x211A85AC, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(26, 26, 26) },

	// Pocket Monsters - Aka (Japan)
	{ 0x29D07340, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 3, 4) },

	// Pocket Monsters - Aka (Japan) (Rev A)
	{ 0x6BB566EC, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 3, 4) },

	// Pocket Monsters - Ao (Japan)
	{ 0x65EF364B, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(28, 4, 28) },

	// Pocket Monsters - Midori (Japan)
	{ 0x923D46DD, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(29, 4, 29) },

	// Pocket Monsters - Midori (Japan) (Rev A)
	{ 0x6C926BFF, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(29, 4, 29) },

	// Pocket Monsters - Pikachu (Japan)
	{ 0xF52AD7C1, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(24, 24, 24) },

	// Pocket Monsters - Pikachu (Japan) (Rev A)
	{ 0x0B54FAEB, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(24, 24, 24) },

	// Pocket Monsters - Pikachu (Japan) (Rev B)
	{ 0x9A161366, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(24, 24, 24) },

	// Pocket Monsters - Pikachu (Japan) (Rev C)
	{ 0x8E1C14E4, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(24, 24, 24) },

	// Pokemon - Blaue Edition (Germany)
	{ 0x6C3587F2, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(28, 4, 28) },

	// Pokemon - Blue Version (USA, Europe)
	{ 0x28323CE0, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(28, 4, 28) },

	// Pokemon - Edicion Azul (Spain)
	{ 0x93FCE15B, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(28, 4, 28) },

	// Pokemon - Edicion Roja (Spain)
	{ 0xFD20BB1C, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 3, 4) },

	// Pokemon - Red Version (USA, Europe)
	{ 0xCC25454F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 3, 4) },

	// Pokemon - Rote Edition (Germany)
	{ 0xE5DD23CE, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 3, 4) },

	// Pokemon - Version Bleue (France)
	{ 0x98BFEC5A, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(28, 4, 28) },

	// Pokemon - Version Rouge (France)
	{ 0x1D6D8022, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 3, 4) },

	// Pokemon - Versione Blu (Italy)
	{ 0x7864DECC, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(28, 4, 28) },

	// Pokemon - Versione Rossa (Italy)
	{ 0xFE2A3F93, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 3, 4) },

	// QIX (World)
	{ 0x5EECB346, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(24, 24, 22) },

	// Radar Mission (Japan)
	{ 0xD03B1A15, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(8, 16, 8) },

	// Radar Mission (USA, Europe)
	{ 0xCEDD9FEB, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(8, 16, 8) },

	// Soccer (Europe)
	{ 0xB0274CDA, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(14, 31, 0) },

	// SolarStriker (World)
	{ 0x981620E7, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(27, 27, 27) },

	// Space Invaders (Europe)
	{ 0x3B032784, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(27, 27, 27) },

	// Space Invaders (USA)
	{ 0x63A767E2, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(27, 27, 27) },

	// Star Wars (USA, Europe) (Rev A)
	{ 0x44CE17EE, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 3, 28) },

	// Street Fighter II (USA)
	{ 0xC512D0B1, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 3) },

	// Street Fighter II (USA, Europe) (Rev A)
	{ 0x79E16545, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 3) },

	// Super Donkey Kong GB (Japan)
	{ 0x940D4974, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(13, 17, 4) },

	// Super Mario Land (World)
	{ 0x6C0ACA9F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(11, 32, 32) },

	// Super Mario Land (World) (Rev A)
	{ 0xCA117ACC, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(11, 32, 32) },

	// Super Mario Land 2 - 6 Golden Coins (USA, Europe) (Rev A)
	{ 0x423E09E6, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(10, 16, 28) },

	// Super Mario Land 2 - 6 Golden Coins (USA, Europe) (Rev B)
	{ 0x445A0358, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(10, 16, 28) },

	// Super Mario Land 2 - 6 Golden Coins (USA, Europe)
	{ 0xDE2960A1, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(10, 16, 28) },

	// Super Mario Land 2 - 6-tsu no Kinka (Japan)
	{ 0xD47CED78, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(10, 16, 28) },

	// Super Mario Land 2 - 6-tsu no Kinka (Japan) (Rev A)
	{ 0xA4B4F9F9, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(10, 16, 28) },

	// Super Mario Land 2 - 6-tsu no Kinka (Japan) (Rev B)
	{ 0x5842F25D, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(10, 16, 28) },

	// Super R.C. Pro-Am (USA, Europe)
	{ 0x8C39B1C8, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 28, 3) },

	// Tennis (World)
	{ 0xD2BEBF08, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(14, 31, 0) },

	// Tetris (World)
	{ 0xE906C6A6, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(24, 24, 24) },

	// Tetris (World) (Rev A)
	{ 0x4674B43F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(24, 24, 24) },

	// Tetris 2 (USA)
	{ 0x687505F1, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(24, 24, 22) },

	// Tetris 2 (USA, Europe)
	{ 0x6761459F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(24, 24, 22) },

	// Tetris Attack (USA)
	{ 0x00E9474B, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(18, 18, 22) },

	// Tetris Blast (USA, Europe)
	{ 0xDDDEEEDE, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(20, 20, 20) },

	// Tetris Attack (USA, Europe) (Rev A)
	{ 0x6628C535, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(18, 18, 22) },

	// Tetris Flash (Japan)
	{ 0xED669A78, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(24, 24, 22) },

	// Top Rank Tennis (USA)
	{ 0xA6497CC0, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(14, 31, 0) },

	// Top Ranking Tennis (Europe)
	{ 0x62C12E05, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(14, 31, 0) },

	// Toy Story (Europe)
	{ 0x67066E28, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(3, 4, 4) },

	// Vegas Stakes (USA, Europe)
	{ 0x80CB217F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(3, 4, 28) },

	// Wario Land - Super Mario Land 3 (World)
	{ 0xF1EA10E9, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(8, 16, 22) },

	// Wario Land II (USA, Europe)
	{ 0xD56A50A1, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(8, 0, 28) },

	// Wave Race (USA, Europe)
	{ 0x52A6E4CC, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(28, 4, 23) },

	// X (Japan)
	{ 0xFED4C47F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(5, 5, 5) },

	// Yakuman (Japan)
	{ 0x40604F17, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 0, 0) },

	// Yakuman (Japan) (Rev A)
	{ 0x2959ACFC, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(0, 0, 0) },

	// Yoshi (USA)
	{ 0xAB1605B9, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(18, 4, 4) },

	// Yoshi no Cookie (Japan)
	{ 0x841753DA, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(20, 20, 22) },

	// Yoshi no Panepon (Japan)
	{ 0xAA1AD903, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(18, 18, 22) },

	// Yoshi no Tamago (Japan)
	{ 0xD4098A6B, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(18, 4, 4) },

	// Yoshi's Cookie (USA, Europe)
	{ 0x940EDD87, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(20, 20, 22) },

	// Zelda no Densetsu - Yume o Miru Shima (Japan)
	{ 0x259C9A82, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 21, 28) },

	// Zelda no Densetsu - Yume o Miru Shima (Japan) (Rev A)
	{ 0x61F269CD, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, PALETTE(4, 21, 28) },

	{ 0, 0, 0, { 0 } }
};

static const struct GBCartridgeOverride _sgbOverrides[] = {
	// Alleyway (World)
	{ 0xCBAA161B, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3F) },

	// Baseball (World)
	{ 0xE02904BD, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(2G) },

	// Dr. Mario (World)
	{ 0xA3C2C1E9, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3B) },

	// Dr. Mario (World) (Rev A)
	{ 0x69975661, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3B) },

	// F-1 Race (World)
	{ 0x8434CB2C, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(4F) },

	// F-1 Race (World) (Rev A)
	{ 0xBA63383B, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(4F) },

	// Game Boy Wars (Japan)
	{ 0x03E3ED72, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3E) },

	// Golf (World)
	{ 0x885C242D, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3H) },

	// Hoshi no Kirby (Japan)
	{ 0x4AA02A13, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(2C) },

	// Hoshi no Kirby (Japan) (Rev A)
	{ 0x88D03280, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(2C) },

	// Kaeru no Tame ni Kane wa Naru (Japan)
	{ 0x7F805941, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(2A) },

	// Kid Icarus - Of Myths and Monsters (USA, Europe)
	{ 0x5D93DB0F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(2F) },

	// Kirby no Pinball (Japan)
	{ 0x89239AED, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1C) },

	// Kirby's Dream Land (USA, Europe)
	{ 0x302017CC, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(2C) },

	// Kirby's Pinball Land (USA, Europe)
	{ 0x9C4AA9D8, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1C) },

	// Legend of Zelda, The - Link's Awakening (Canada)
	{ 0x9F54D47B, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1E) },

	// Legend of Zelda, The - Link's Awakening (France)
	{ 0x441D7FAD, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1E) },

	// Legend of Zelda, The - Link's Awakening (Germany)
	{ 0x838D65D6, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1E) },

	// Legend of Zelda, The - Link's Awakening (USA, Europe) (Rev A)
	{ 0x24CAAB4D, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1E) },

	// Legend of Zelda, The - Link's Awakening (USA, Europe) (Rev B)
	{ 0xBCBB6BDB, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1E) },

	// Legend of Zelda, The - Link's Awakening (USA, Europe)
	{ 0x9A193109, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1E) },

	// Mario & Yoshi (Europe)
	{ 0xEC14B007, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(2D) },

	// Metroid II - Return of Samus (World)
	{ 0xBDCCC648, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(4G) },

	// QIX (World)
	{ 0x5EECB346, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(4A) },

	// SolarStriker (World)
	{ 0x981620E7, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1G) },

	// Super Mario Land (World)
	{ 0x6C0ACA9F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1F) },

	// Super Mario Land (World) (Rev A)
	{ 0xCA117ACC, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1F) },

	// Super Mario Land 2 - 6 Golden Coins (USA, Europe) (Rev A)
	{ 0x423E09E6, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3D) },

	// Super Mario Land 2 - 6 Golden Coins (USA, Europe) (Rev B)
	{ 0x445A0358, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3D) },

	// Super Mario Land 2 - 6 Golden Coins (USA, Europe)
	{ 0xDE2960A1, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3D) },

	// Super Mario Land 2 - 6-tsu no Kinka (Japan)
	{ 0xD47CED78, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3D) },

	// Super Mario Land 2 - 6-tsu no Kinka (Japan) (Rev A)
	{ 0xA4B4F9F9, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3D) },

	// Super Mario Land 2 - 6-tsu no Kinka (Japan) (Rev B)
	{ 0x5842F25D, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3D) },

	// Tennis (World)
	{ 0xD2BEBF08, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3G) },

	// Tetris (World)
	{ 0xE906C6A6, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3A) },

	// Tetris (World) (Rev A)
	{ 0x4674B43F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3A) },

	// Wario Land - Super Mario Land 3 (World)
	{ 0xF1EA10E9, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1B) },

	// X (Japan)
	{ 0xFED4C47F, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(4D) },

	// Yakuman (Japan)
	{ 0x40604F17, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3C) },

	// Yakuman (Japan) (Rev A)
	{ 0x2959ACFC, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(3C) },

	// Yoshi (USA)
	{ 0xAB1605B9, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(2D) },

	// Yoshi no Cookie (Japan)
	{ 0x841753DA, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1D) },

	// Yoshi no Tamago (Japan)
	{ 0xD4098A6B, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(2D) },

	// Yoshi's Cookie (USA, Europe)
	{ 0x940EDD87, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1D) },

	// Zelda no Densetsu - Yume o Miru Shima (Japan)
	{ 0x259C9A82, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1E) },

	// Zelda no Densetsu - Yume o Miru Shima (Japan) (Rev A)
	{ 0x61F269CD, GB_MODEL_AUTODETECT, GB_MBC_AUTODETECT, SGB_PAL(1E) },

	{ 0, 0, 0, { 0 } }
};

static const struct GBCartridgeOverride _overrides[] = {
	// Pokemon Spaceworld 1997 demo
	{ 0x232A067D, GB_MODEL_AUTODETECT, GB_MBC3_RTC, { 0 } }, // Gold (debug)
	{ 0x630ED957, GB_MODEL_AUTODETECT, GB_MBC3_RTC, { 0 } }, // Gold (non-debug)
	{ 0x5AFF0038, GB_MODEL_AUTODETECT, GB_MBC3_RTC, { 0 } }, // Silver (debug)
	{ 0xA61856BD, GB_MODEL_AUTODETECT, GB_MBC3_RTC, { 0 } }, // Silver (non-debug)
	// Unlicensed bootlegs
	{ 0x30F8F86C, GB_MODEL_AUTODETECT, GB_UNL_PKJD, { 0 } }, // Pokemon Jade Version (Telefang Speed bootleg)
	{ 0xE1147E75, GB_MODEL_AUTODETECT, GB_UNL_NT_OLD_1, { 0 } }, // Rockman 8
	{ 0xEFF88FAA, GB_MODEL_AUTODETECT, GB_UNL_NT_OLD_1, { 0 } }, // True Color 25 in 1 (NT-9920)
	{ 0x811925D9, GB_MODEL_AUTODETECT, GB_UNL_NT_OLD_2, { 0 } }, // 23 in 1 (CR2011)
	{ 0x62A8016A, GB_MODEL_AUTODETECT, GB_UNL_NT_OLD_2, { 0 } }, // 29 in 1 (CR2020)
	{ 0x5758D6D9, GB_MODEL_AUTODETECT, GB_UNL_NT_OLD_2, { 0 } }, // Caise Gedou 24 in 1 Diannao Huamian Xuan Game (CY2060)
	{ 0x62A8016A, GB_MODEL_AUTODETECT, GB_UNL_NT_OLD_2, { 0 } }, // Caise Gedou 29 in 1 Diannao Huamian Xuan Game (CY2061)
	{ 0x80265A64, GB_MODEL_AUTODETECT, GB_UNL_NT_OLD_2, { 0 } }, // Rockman X4 (Megaman X4)
	{ 0x805459DE, GB_MODEL_AUTODETECT, GB_UNL_NT_OLD_2, { 0 } }, // Sonic Adventure 8
	{ 0x0B1B808A, GB_MODEL_AUTODETECT, GB_UNL_NT_OLD_2, { 0 } }, // Super Donkey Kong 5
	{ 0x0B1B808A, GB_MODEL_AUTODETECT, GB_UNL_NT_OLD_2, { 0 } }, // Super Donkey Kong 5 (Alt)
	{ 0x4650EB9A, GB_MODEL_AUTODETECT, GB_UNL_NT_OLD_2, { 0 } }, // Super Mario Special 3
	{ 0xB289D95A, GB_MODEL_AUTODETECT, GB_UNL_NT_NEW, { 0 } }, // Capcom vs SNK - Millennium Fight 2001
	{ 0x688D6713, GB_MODEL_AUTODETECT, GB_UNL_NT_NEW, { 0 } }, // Digimon 02 4
	{ 0x8931A272, GB_MODEL_AUTODETECT, GB_UNL_NT_NEW, { 0 } }, // Digimon 2
	{ 0x79083C6B, GB_MODEL_AUTODETECT, GB_UNL_NT_NEW, { 0 } }, // Digimon Pocket
	{ 0x0C5047EE, GB_MODEL_AUTODETECT, GB_UNL_NT_NEW, { 0 } }, // Harry Potter 3
	{ 0x8AC634B7, GB_MODEL_AUTODETECT, GB_UNL_NT_NEW, { 0 } }, // Pokemon Diamond (Special Pikachu Edition)
	{ 0x8628A287, GB_MODEL_AUTODETECT, GB_UNL_NT_NEW, { 0 } }, // Pokemon Jade (Special Pikachu Edition)
	{ 0xBC75D7B8, GB_MODEL_AUTODETECT, GB_UNL_NT_NEW, { 0 } }, // Pokemon - Mewtwo Strikes Back
	{ 0xFF0B60CC, GB_MODEL_AUTODETECT, GB_UNL_NT_NEW, { 0 } }, // Shuma Baolong 02 4
	{ 0x14A992A6, GB_MODEL_AUTODETECT, GB_UNL_NT_NEW, { 0 } }, // /Street Fighter Zero 4
	{ 0x3EF5AFB2, GB_MODEL_AUTODETECT, GB_UNL_LI_CHENG, { 0 } }, // Pokemon Jade Version (Telefang Speed bootleg)

	{ 0, 0, 0, { 0 } }
};

static const struct GBColorPreset _colorPresets[] = {
	{ "Grayscale", UNIFORM_PAL(0x7FFF, 0x56B5, 0x294A, 0x0000), },
	{ "DMG Green", UNIFORM_PAL(0x2691, 0x19A9, 0x1105, 0x04A3), },
	{ "GB Pocket", UNIFORM_PAL(0x52D4, 0x4270, 0x2989, 0x10A3), },
	{ "GB Light", UNIFORM_PAL(0x7FCF, 0x738B, 0x56C3, 0x39E0), },
	{ "GBC Brown ↑", PALETTE(0, 0, 0), },
	{ "GBC Red ↑A", PALETTE(4, 3, 28), },
	{ "GBC Dark Brown ↑B", PALETTE(1, 0, 0), },
	{ "GBC Pale Yellow ↓", PALETTE(12, 12, 12), },
	{ "GBC Orange ↓A", PALETTE(24, 24, 24), },
	{ "GBC Yellow ↓B", PALETTE(6, 28, 3), },
	{ "GBC Blue ←", PALETTE(28, 4, 3), },
	{ "GBC Dark Blue ←A", PALETTE(2, 4, 0), },
	{ "GBC Gray ←B", PALETTE(5, 5, 5), },
	{ "GBC Green →", PALETTE(18, 18, 18), },
	{ "GBC Dark Green →A", PALETTE(29, 4, 4), },
	{ "GBC Reverse →B", PALETTE(27, 27, 27), },
	{ "SGB 1-A", SGB_PAL(1A), },
	{ "SGB 1-B", SGB_PAL(1B), },
	{ "SGB 1-C", SGB_PAL(1C), },
	{ "SGB 1-D", SGB_PAL(1D), },
	{ "SGB 1-E", SGB_PAL(1E), },
	{ "SGB 1-F", SGB_PAL(1F), },
	{ "SGB 1-G", SGB_PAL(1G), },
	{ "SGB 1-H", SGB_PAL(1H), },
	{ "SGB 2-A", SGB_PAL(2A), },
	{ "SGB 2-B", SGB_PAL(2B), },
	{ "SGB 2-C", SGB_PAL(2C), },
	{ "SGB 2-D", SGB_PAL(2D), },
	{ "SGB 2-E", SGB_PAL(2E), },
	{ "SGB 2-F", SGB_PAL(2F), },
	{ "SGB 2-G", SGB_PAL(2G), },
	{ "SGB 2-H", SGB_PAL(2H), },
	{ "SGB 3-A", SGB_PAL(3A), },
	{ "SGB 3-B", SGB_PAL(3B), },
	{ "SGB 3-C", SGB_PAL(3C), },
	{ "SGB 3-D", SGB_PAL(3D), },
	{ "SGB 3-E", SGB_PAL(3E), },
	{ "SGB 3-F", SGB_PAL(3F), },
	{ "SGB 3-G", SGB_PAL(3G), },
	{ "SGB 3-H", SGB_PAL(3H), },
	{ "SGB 4-A", SGB_PAL(4A), },
	{ "SGB 4-B", SGB_PAL(4B), },
	{ "SGB 4-C", SGB_PAL(4C), },
	{ "SGB 4-D", SGB_PAL(4D), },
	{ "SGB 4-E", SGB_PAL(4E), },
	{ "SGB 4-F", SGB_PAL(4F), },
	{ "SGB 4-G", SGB_PAL(4G), },
	{ "SGB 4-H", SGB_PAL(4H), },
};

bool GBOverrideColorFind(struct GBCartridgeOverride* override, enum GBColorLookup order) {
	int i;
	if (order & GB_COLORS_SGB) {
		for (i = 0; _gbcOverrides[i].headerCrc32; ++i) {
			if (override->headerCrc32 == _sgbOverrides[i].headerCrc32) {
				memcpy(override->gbColors, _sgbOverrides[i].gbColors, sizeof(override->gbColors));
				return true;
			}
		}
	}
	if (order & GB_COLORS_CGB) {
		for (i = 0; _gbcOverrides[i].headerCrc32; ++i) {
			if (override->headerCrc32 == _gbcOverrides[i].headerCrc32) {
				memcpy(override->gbColors, _gbcOverrides[i].gbColors, sizeof(override->gbColors));
				return true;
			}
		}
	}
	return false;
}

bool GBOverrideFind(const struct Configuration* config, struct GBCartridgeOverride* override) {
	override->model = GB_MODEL_AUTODETECT;
	override->mbc = GB_MBC_AUTODETECT;
	memset(override->gbColors, 0, sizeof(override->gbColors));
	bool found = false;

	int i;
	for (i = 0; _overrides[i].headerCrc32; ++i) {
		if (override->headerCrc32 == _overrides[i].headerCrc32) {
			*override = _overrides[i];
			found = true;
			break;
		}
	}

	if (config) {
		char sectionName[24] = "";
		snprintf(sectionName, sizeof(sectionName), "gb.override.%08X", override->headerCrc32);
		const char* model = ConfigurationGetValue(config, sectionName, "model");
		const char* mbc = ConfigurationGetValue(config, sectionName, "mbc");
		const char* pal[12] = {
			ConfigurationGetValue(config, sectionName, "pal[0]"),
			ConfigurationGetValue(config, sectionName, "pal[1]"),
			ConfigurationGetValue(config, sectionName, "pal[2]"),
			ConfigurationGetValue(config, sectionName, "pal[3]"),
			ConfigurationGetValue(config, sectionName, "pal[4]"),
			ConfigurationGetValue(config, sectionName, "pal[5]"),
			ConfigurationGetValue(config, sectionName, "pal[6]"),
			ConfigurationGetValue(config, sectionName, "pal[7]"),
			ConfigurationGetValue(config, sectionName, "pal[8]"),
			ConfigurationGetValue(config, sectionName, "pal[9]"),
			ConfigurationGetValue(config, sectionName, "pal[10]"),
			ConfigurationGetValue(config, sectionName, "pal[11]")
		};

		if (model) {
			override->model = GBNameToModel(model);
			found = override->model != GB_MODEL_AUTODETECT;
		}

		if (mbc) {
			char* end;
			long type = strtoul(mbc, &end, 0);
			if (end && !*end) {
				override->mbc = type;
				found = true;
			}
		}

		for (i = 0; i < 12; ++i) {
			if (!pal[i]) {
				continue;
			}
			char* end;
			unsigned long value = strtoul(pal[i], &end, 10);
			if (end == &pal[i][1] && *end == 'x') {
				value = strtoul(pal[i], &end, 16);
			}
			if (*end) {
				continue;
			}
			value |= 0xFF000000;
			override->gbColors[i] = value;
			if (i < 8) {
				override->gbColors[i + 4] = value;
			}
			if (i < 4) {
				override->gbColors[i + 8] = value;
			}
		}
	}
	return found;
}

void GBOverrideSave(struct Configuration* config, const struct GBCartridgeOverride* override) {
	char sectionName[24] = "";
	snprintf(sectionName, sizeof(sectionName), "gb.override.%08X", override->headerCrc32);
	const char* model = GBModelToName(override->model);
	ConfigurationSetValue(config, sectionName, "model", model);

	if (override->gbColors[0] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[0]", override->gbColors[0] & ~0xFF000000);
	}
	if (override->gbColors[1] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[1]", override->gbColors[1] & ~0xFF000000);
	}
	if (override->gbColors[2] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[2]", override->gbColors[2] & ~0xFF000000);
	}
	if (override->gbColors[3] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[3]", override->gbColors[3] & ~0xFF000000);
	}
	if (override->gbColors[4] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[4]", override->gbColors[4] & ~0xFF000000);
	}
	if (override->gbColors[5] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[5]", override->gbColors[5] & ~0xFF000000);
	}
	if (override->gbColors[6] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[6]", override->gbColors[6] & ~0xFF000000);
	}
	if (override->gbColors[7] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[7]", override->gbColors[7] & ~0xFF000000);
	}
	if (override->gbColors[8] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[8]", override->gbColors[8] & ~0xFF000000);
	}
	if (override->gbColors[9] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[9]", override->gbColors[9] & ~0xFF000000);
	}
	if (override->gbColors[10] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[10]", override->gbColors[10] & ~0xFF000000);
	}
	if (override->gbColors[11] & 0xFF000000) {
		ConfigurationSetIntValue(config, sectionName, "pal[11]", override->gbColors[11] & ~0xFF000000);
	}

	if (override->mbc != GB_MBC_AUTODETECT) {
		ConfigurationSetIntValue(config, sectionName, "mbc", override->mbc);
	} else {
		ConfigurationClearValue(config, sectionName, "mbc");
	}
}

size_t GBColorPresetList(const struct GBColorPreset** presets) {
	*presets = _colorPresets;
	return sizeof(_colorPresets) / sizeof(*_colorPresets);
}

void GBOverrideApply(struct GB* gb, const struct GBCartridgeOverride* override) {
	if (override->model != GB_MODEL_AUTODETECT) {
		gb->model = override->model;
		gb->video.renderer->deinit(gb->video.renderer);
		gb->video.renderer->init(gb->video.renderer, gb->model, gb->video.sgbBorders);
	}

	if (override->mbc != GB_MBC_AUTODETECT) {
		gb->memory.mbcType = override->mbc;
		GBMBCInit(gb);
	}

	int i;
	for (i = 0; i < 12; ++i) {
		if (!(override->gbColors[i] & 0xFF000000)) {
			continue;
		}
		GBVideoSetPalette(&gb->video, i, override->gbColors[i]);
		if (i < 8) {
			GBVideoSetPalette(&gb->video, i + 4, override->gbColors[i]);
		}
		if (i < 4) {
			GBVideoSetPalette(&gb->video, i + 8, override->gbColors[i]);
		}
	}
}

void GBOverrideApplyDefaults(struct GB* gb) {
	struct GBCartridgeOverride override;
	override.headerCrc32 = doCrc32(&gb->memory.rom[0x100], sizeof(struct GBCartridge));
	if (GBOverrideFind(0, &override)) {
		GBOverrideApply(gb, &override);
	}
}

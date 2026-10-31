/*
 * Copyright (C) 2026 Ishan
 * Android Port component of mGBA.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3.
 *
 * This program is distributed without any warranty. See the GNU General Public License for more details.
 */

#pragma once

#include <cstdint>
#include <string>

struct NoIntroMetadata {
	std::string name;
	std::string romName;
	uint64_t size = 0;
	uint32_t crc32 = 0;
	bool verified = false;
};

bool noIntroInit(const char* databasePath, const char* datPath);
void noIntroShutdown();

bool noIntroLookupCRC32(uint32_t crc32, NoIntroMetadata& out);
[[maybe_unused]] bool noIntroLookupMD5(const uint8_t md5[16], NoIntroMetadata& out);
[[maybe_unused]] bool noIntroLookupSHA1(const uint8_t sha1[20], NoIntroMetadata& out);
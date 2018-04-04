/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef ELF_READ_H
#define ELF_READ_H

#include <mgba-util/common.h>

CXX_GUARD_START

#ifdef USE_ELF

#include <libelf.h>

#include <mgba-util/vector.h>

struct ELF;
struct VFile;

DECLARE_VECTOR(ELFProgramHeaders, Elf32_Phdr);
DECLARE_VECTOR(ELFSectionHeaders, Elf32_Shdr);

struct ELF* ELFOpen(struct VFile*);
void ELFClose(struct ELF*);

void* ELFBytes(struct ELF*, size_t* size);

uint16_t ELFMachine(struct ELF*);
uint32_t ELFEntry(struct ELF*);

void ELFGetProgramHeaders(struct ELF*, struct ELFProgramHeaders*);

size_t ELFFindSection(struct ELF*, const char* name);
void ELFGetSectionHeaders(struct ELF*, struct ELFSectionHeaders*);
Elf32_Shdr* ELFGetSectionHeader(struct ELF*, size_t index);

const char* ELFGetString(struct ELF*, size_t section, size_t string);

#endif

CXX_GUARD_END

#endif

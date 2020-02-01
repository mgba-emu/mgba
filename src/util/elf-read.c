/* Copyright (c) 2013-2017 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <mgba-util/elf-read.h>

#ifdef USE_ELF

#include <mgba-util/vfs.h>

DEFINE_VECTOR(ELFProgramHeaders, Elf32_Phdr);
DEFINE_VECTOR(ELFSectionHeaders, Elf32_Shdr);

static bool _elfInit = false;

struct ELF {
	Elf* e;
	struct VFile* vf;
	size_t size;
	char* memory;
};

struct ELF* ELFOpen(struct VFile* vf) {
	if (!_elfInit) {
		_elfInit = elf_version(EV_CURRENT) != EV_NONE;
		if (!_elfInit) {
			return NULL;
		}
	}
	if (!vf) {
		return NULL;
	}
	size_t size = vf->size(vf);
	char* memory = vf->map(vf, size, MAP_READ);
	if (!memory) {
		return NULL;
	}

	Elf* e = elf_memory(memory, size);
	if (!e || elf_kind(e) != ELF_K_ELF) {
		elf_end(e);
		vf->unmap(vf, memory, size);
		return false;
	}
	struct ELF* elf = malloc(sizeof(*elf));
	elf->e = e;
	elf->vf = vf;
	elf->size = size;
	elf->memory = memory;
	return elf;
}

void ELFClose(struct ELF* elf) {
	elf_end(elf->e);
	elf->vf->unmap(elf->vf, elf->memory, elf->size);
	free(elf);
}

void* ELFBytes(struct ELF* elf, size_t* size) {
	if (size) {
		*size = elf->size;
	}
	return elf->memory;
}

uint16_t ELFMachine(struct ELF* elf) {
	Elf32_Ehdr* hdr = elf32_getehdr(elf->e);
	if (!hdr) {
		return 0;
	}
	return hdr->e_machine;
}

uint32_t ELFEntry(struct ELF* elf) {
	Elf32_Ehdr* hdr = elf32_getehdr(elf->e);
	if (!hdr) {
		return 0;
	}
	return hdr->e_entry;
}

void ELFGetProgramHeaders(struct ELF* elf, struct ELFProgramHeaders* ph) {
	ELFProgramHeadersClear(ph);
	Elf32_Ehdr* hdr = elf32_getehdr(elf->e);
	Elf32_Phdr* phdr = elf32_getphdr(elf->e);
	if (!hdr || !phdr) {
		return;
	}
	ELFProgramHeadersResize(ph, hdr->e_phnum);
	memcpy(ELFProgramHeadersGetPointer(ph, 0), phdr, sizeof(*phdr) * hdr->e_phnum);
}

void ELFGetSectionHeaders(struct ELF* elf, struct ELFSectionHeaders* sh) {
	ELFSectionHeadersClear(sh);
	Elf_Scn* section = elf_getscn(elf->e, 0);
	do {
		*ELFSectionHeadersAppend(sh) = *elf32_getshdr(section);
	} while ((section = elf_nextscn(elf->e, section)));
}

Elf32_Shdr* ELFGetSectionHeader(struct ELF* elf, size_t index) {
	Elf_Scn* section = elf_getscn(elf->e, index);
	return elf32_getshdr(section);
}

size_t ELFFindSection(struct ELF* elf, const char* name) {
	Elf32_Ehdr* hdr = elf32_getehdr(elf->e);
	size_t shstrtab = hdr->e_shstrndx;
	if (strcmp(name, ".shstrtab") == 0) {
		return shstrtab;
	}
	Elf_Scn* section = NULL;
	while ((section = elf_nextscn(elf->e, section))) {
		Elf32_Shdr* shdr = elf32_getshdr(section);
		const char* sname = elf_strptr(elf->e, shstrtab, shdr->sh_name);
		if (strcmp(sname, name) == 0) {
			return elf_ndxscn(section);
		}
	}
	return 0;
}

const char* ELFGetString(struct ELF* elf, size_t section, size_t string) {
	return elf_strptr(elf->e, section, string);
}

#endif

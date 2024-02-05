PREFIX := $(DEVKITARM)/bin/arm-none-eabi-
AS := $(PREFIX)as
OBJCOPY := $(PREFIX)objcopy

all: hle-bios.c

hle-bios.o: hle-bios.s
	$(AS) -o $@ $<

hle-bios.bin: hle-bios.o
	$(OBJCOPY) -O binary $< $@

hle-bios.c: hle-bios.bin
	echo '#include "hle-bios.h"' > $@
	echo >> $@
	echo '#include <mgba/internal/gba/memory.h>' >> $@
	echo >> $@
	xxd -i $< | sed -e 's/unsigned char hle_bios_bin\[\]/const uint8_t hleBios[GBA_SIZE_BIOS]/' -e 's/^ \+/\t/' | grep -v hle_bios_bin_len >> $@

CCFLAGS = \
	-O3 -fomit-frame-pointer -fexpensive-optimizations -flto \
	-I../../../../include -I../../.. -std=gnu11 \
	-Wall -Wno-stringop-overflow -Wno-lto-type-mismatch \
	-Wno-ignored-optimization-argument -Wno-unknown-warning-option \
	-DMINIMAL_CORE=2 -DDISABLE_THREADING -DM_CORE_GBA \
	-DCOLOR_16_BIT -DMGBA_STANDALONE -DENABLE_DEBUGGERS \
	$(PLAT_FLAGS)

DEST_64 = ../../../../../../Assets/dll
DESTCOPY_64 = ../../../../../../output/dll

SRCS = \
	$(ROOT_DIR)/core/bitmap-cache.c \
	$(ROOT_DIR)/core/cache-set.c \
	$(ROOT_DIR)/core/cheats.c \
	$(ROOT_DIR)/core/config.c \
	$(ROOT_DIR)/core/core.c \
	$(ROOT_DIR)/core/directories.c \
	$(ROOT_DIR)/core/input.c \
	$(ROOT_DIR)/core/interface.c \
	$(ROOT_DIR)/core/library.c \
	$(ROOT_DIR)/core/lockstep.c \
	$(ROOT_DIR)/core/log.c \
	$(ROOT_DIR)/core/map-cache.c \
	$(ROOT_DIR)/core/mem-search.c \
	$(ROOT_DIR)/core/rewind.c \
	$(ROOT_DIR)/core/serialize.c \
	$(ROOT_DIR)/core/sync.c \
	$(ROOT_DIR)/core/thread.c \
	$(ROOT_DIR)/core/tile-cache.c \
	$(ROOT_DIR)/core/timing.c \
	$(ROOT_DIR)/arm/arm.c \
	$(ROOT_DIR)/arm/decoder-arm.c \
	$(ROOT_DIR)/arm/decoder-thumb.c \
	$(ROOT_DIR)/arm/decoder.c \
	$(ROOT_DIR)/arm/isa-arm.c \
	$(ROOT_DIR)/arm/isa-thumb.c \
	$(ROOT_DIR)/arm/debugger/cli-debugger.c \
	$(ROOT_DIR)/arm/debugger/memory-debugger.c \
	$(ROOT_DIR)/arm/debugger/debugger.c \
	$(ROOT_DIR)/gb/audio.c \
	$(ROOT_DIR)/gba/audio.c \
	$(ROOT_DIR)/gba/bios.c \
	$(ROOT_DIR)/gba/cheats.c \
	$(ROOT_DIR)/gba/core.c \
	$(ROOT_DIR)/gba/dma.c \
	$(ROOT_DIR)/gba/gba.c \
	$(ROOT_DIR)/gba/hle-bios.c \
	$(ROOT_DIR)/gba/input.c \
	$(ROOT_DIR)/gba/io.c \
	$(ROOT_DIR)/gba/memory.c \
	$(ROOT_DIR)/gba/overrides.c \
	$(ROOT_DIR)/gba/savedata.c \
	$(ROOT_DIR)/gba/serialize.c \
	$(ROOT_DIR)/gba/sharkport.c \
	$(ROOT_DIR)/gba/sio.c \
	$(ROOT_DIR)/gba/timer.c \
	$(ROOT_DIR)/gba/video.c \
	$(ROOT_DIR)/gba/cart/ereader.c \
	$(ROOT_DIR)/gba/cart/gpio.c \
	$(ROOT_DIR)/gba/cart/matrix.c \
	$(ROOT_DIR)/gba/cart/unlicensed.c \
	$(ROOT_DIR)/gba/cart/vfame.c \
	$(ROOT_DIR)/gba/cheats/codebreaker.c \
	$(ROOT_DIR)/gba/cheats/gameshark.c \
	$(ROOT_DIR)/gba/cheats/parv3.c \
	$(ROOT_DIR)/gba/debugger/cli.c \
	$(ROOT_DIR)/gba/renderers/cache-set.c \
	$(ROOT_DIR)/gba/renderers/common.c \
	$(ROOT_DIR)/gba/renderers/software-bg.c \
	$(ROOT_DIR)/gba/renderers/software-mode0.c \
	$(ROOT_DIR)/gba/renderers/software-obj.c \
	$(ROOT_DIR)/gba/renderers/video-software.c \
	$(ROOT_DIR)/gba/sio/gbp.c \
	$(ROOT_DIR)/gba/sio/lockstep.c \
	$(ROOT_DIR)/debugger/debugger.c \
	$(ROOT_DIR)/debugger/stack-trace.c \
	$(ROOT_DIR)/debugger/cli-debugger.c \
	$(ROOT_DIR)/debugger/symbols.c \
	$(ROOT_DIR)/debugger/parser.c \
	$(ROOT_DIR)/third-party/inih/ini.c \
	$(ROOT_DIR)/util/audio-buffer.c \
	$(ROOT_DIR)/util/audio-resampler.c \
	$(ROOT_DIR)/util/circle-buffer.c \
	$(ROOT_DIR)/util/configuration.c \
	$(ROOT_DIR)/util/crc32.c \
	$(ROOT_DIR)/util/elf-read.c \
	$(ROOT_DIR)/util/formatting.c \
	$(ROOT_DIR)/util/gbk-table.c \
	$(ROOT_DIR)/util/gui.c \
	$(ROOT_DIR)/util/hash.c \
	$(ROOT_DIR)/util/image/png-io.c \
	$(ROOT_DIR)/util/interpolator.c \
	$(ROOT_DIR)/util/md5.c \
	$(ROOT_DIR)/util/patch-fast.c \
	$(ROOT_DIR)/util/patch-ips.c \
	$(ROOT_DIR)/util/patch-ups.c \
	$(ROOT_DIR)/util/patch.c \
	$(ROOT_DIR)/util/ring-fifo.c \
	$(ROOT_DIR)/util/string.c \
	$(ROOT_DIR)/util/table.c \
	$(ROOT_DIR)/util/text-codec.c \
	$(ROOT_DIR)/util/vector.c \
	$(ROOT_DIR)/util/vfs.c \
	$(ROOT_DIR)/util/vfs/vfs-mem.c \
	$(ROOT_DIR)/platform/bizhawk/bizinterface.c \
	$(ROOT_DIR)/platform/bizhawk/localtime_r.c \
	$(PLAT_SRCS)

_OBJS:=$(SRCS:.c=.o)
OBJS:=$(patsubst $(ROOT_DIR)%,$(OBJ_DIR)%,$(_OBJS))

all: $(TARGET)

$(OBJ_DIR)/%.o: $(ROOT_DIR)/%.c
	@mkdir -p $(@D)
	@$(CC) -c -o $@ $< $(CCFLAGS)

$(TARGET): $(OBJS)
	@$(CC) -o $@ $(OBJS) $(LDFLAGS)

clean:
	@$(RM) -rf $(OBJ_DIR)
	@$(RM) -f $(TARGET)

install: $(TARGET)
	$(CP) $(TARGET) $(DEST_$(ARCH))
ifneq ("$(wildcard $(DESTCOPY_$(ARCH)))", "")
	$(CP) $(TARGET) $(DESTCOPY_$(ARCH))
endif

print-%:
	@echo $* = $($*)

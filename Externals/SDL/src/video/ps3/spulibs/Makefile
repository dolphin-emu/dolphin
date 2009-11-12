# This Makefile is for building the CELL BE SPU libs
# libfb_writer_spu.so, libyuv2rgb_spu.so, libbilin_scaler_spu.so

# Toolchain
PPU_LD=/usr/bin/ld
SPU_SRCDIR=$(srcdir)/src/video/ps3/spulibs
SPU_LIBDIR=$(srcdir)/src/video/ps3/spulibs/libs
SPU_CFLAGS=-g -W -Wall -Winline -Wno-main -I. -I /usr/spu/include -I /opt/cell/sdk/usr/spu/include -finline-limit=10000 -Winline -ftree-vectorize -funroll-loops -fmodulo-sched -ffast-math -fPIC -O2

DEPS = $(SPU_SRCDIR)/spu_common.h
LIBS= fb_writer yuv2rgb bilin_scaler

OBJLIBS = $(foreach lib,$(LIBS),lib$(lib)_spu.a)
SHALIBS = $(foreach lib,$(LIBS),lib$(lib)_spu.so)


ps3libs: $(foreach lib,$(OBJLIBS),$(SPU_LIBDIR)/$(lib)) $(foreach lib,$(SHALIBS),$(SPU_LIBDIR)/$(lib))


$(SPU_LIBDIR)/lib%_spu.a: $(SPU_LIBDIR)/%-embed.o
	$(AR) -qcs $@ $<

$(SPU_LIBDIR)/lib%_spu.so: $(SPU_LIBDIR)/%-embed.o
	$(PPU_LD) -o $@ -shared -soname=$(notdir $@) $<

$(SPU_LIBDIR)/%-embed.o: $(SPU_LIBDIR)/%.o
	$(EMBEDSPU) -m32 $(subst -embed.o,,$(notdir $@))_spu $< $@

$(SPU_LIBDIR)/%.o: $(SPU_SRCDIR)/%.c $(DEPS)
	$(SPU_GCC) $(SPU_CFLAGS) -o $@ $< -lm


ps3libs-install: $(foreach obj,$(OBJLIBS),$(SPU_LIBDIR)/$(obj)) $(foreach obj,$(SHALIBS),$(SPU_LIBDIR)/$(obj))
	for file in $(OBJLIBS); do \
		$(INSTALL) -c -m 0655 $(SPU_LIBDIR)/$$file $(DESTDIR)$(libdir)/$$file; \
	done
	for file in $(SHALIBS); do \
		$(INSTALL) -c -m 0755 $(SPU_LIBDIR)/$$file $(DESTDIR)$(libdir)/$$file; \
	done

ps3libs-uninstall:
	for file in $(OBJLIBS) $(SHALIBS); do \
		rm -f $(DESTDIR)$(libdir)/$$file; \
	done

ps3libs-clean:
	rm -f $(SPU_LIBDIR)/*

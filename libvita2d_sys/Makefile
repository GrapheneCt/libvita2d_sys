TARGET_LIB = libvita2d_sys.a
OBJS       = source/vita2d.o source/vita2d_texture.o source/vita2d_draw.o source/utils.o \
             source/vita2d_image_png.o source/vita2d_image_jpeg.o source/vita2d_image_bmp.o \
             source/vita2d_image_gim.o source/vita2d_pgf.o source/vita2d_pvf.o \
             source/bin_packing_2d.o source/texture_atlas.o source/int_htab.o \
			 source/vita2d_image_gxt.o source/vita2d_sysgraphics.o source/heap.o
INCLUDES   = include

PREFIX  ?= ${DOLCESDK}/arm-dolce-eabi
CC      = arm-dolce-eabi-gcc
AR      = arm-dolce-eabi-ar
CFLAGS  = -Wl,-q -Wall -O3 -I$(INCLUDES) -ffat-lto-objects -flto
ASFLAGS = $(CFLAGS)

all: $(TARGET_LIB)

debug: CFLAGS += -DDEBUG_BUILD
debug: all

$(TARGET_LIB): $(OBJS)
	$(AR) -rc $@ $^

clean:
	rm -rf $(TARGET_LIB) $(OBJS)

install: $(TARGET_LIB)
	@mkdir -p $(DESTDIR)$(PREFIX)/lib/
	cp $(TARGET_LIB) $(DESTDIR)$(PREFIX)/lib/
	@mkdir -p $(DESTDIR)$(PREFIX)/include/
	cp include/vita2d_sys.h $(DESTDIR)$(PREFIX)/include/

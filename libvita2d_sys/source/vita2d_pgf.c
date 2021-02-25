#include <font/libpgf.h>
#include <kernel.h>
#include <libsysmodule.h>
#include <libdbg.h>
#include <math.h>
#include "vita2d_sys.h"

#include "texture_atlas.h"
#include "bin_packing_2d.h"
#include "utils.h"
#include "shared.h"
#include "heap.h"

#define ATLAS_DEFAULT_W 512
#define ATLAS_DEFAULT_H 512

extern void* vita2d_heap_internal;
extern int pgf_module_was_loaded;

typedef struct vita2d_pgf_font_handle {
	SceFont_t_fontId font_handle;
	int (*in_font_group)(unsigned int c);
	struct vita2d_pgf_font_handle *next;
} vita2d_pgf_font_handle;

typedef struct vita2d_pgf {
	SceFont_t_fontId lib_handle;
	vita2d_pgf_font_handle *font_handle_list;
	texture_atlas *atlas;
	SceKernelLwMutexWork mutex;
	float vsize;
} vita2d_pgf;

static void *pgf_alloc_func(void *userdata, unsigned int size)
{
	heap_alloc_opt_param param;
	param.size = sizeof(heap_alloc_opt_param);
	param.alignment = sizeof(int);
	return heap_alloc_heap_memory_with_option(vita2d_heap_internal, (size + sizeof(int) - 1) / sizeof(int) * sizeof(int), &param);
}


static void pgf_free_func(void *userdata, void *p)
{
	heap_free_heap_memory(vita2d_heap_internal, p);
}

static void vita2d_load_pgf_post(vita2d_pgf *font) {
	SceFont_t_fontInfo fontinfo;

	sceFontGetFontInfo(font->font_handle_list->font_handle, &fontinfo);
	font->vsize = (fontinfo.fontStyleInfo.vSize / fontinfo.fontStyleInfo.vResolution)
		* SCREEN_DPI;

	font->atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H,
		SCE_GXM_TEXTURE_FORMAT_U8_R111);

	sceKernelCreateLwMutex(&font->mutex, "vita2d_pgf_mutex", 2, 0, NULL);
}

static vita2d_pgf *vita2d_load_pgf_pre(int numFonts)
{
	pgf_module_was_loaded = sceSysmoduleIsLoaded(SCE_SYSMODULE_PGF);

	if (pgf_module_was_loaded != SCE_SYSMODULE_LOADED)
		sceSysmoduleLoadModule(SCE_SYSMODULE_PGF);

	unsigned int error;
	vita2d_pgf *font = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*font));
	if (!font) {
		SCE_DBG_LOG_ERROR("[PGF] sceClibMspaceMalloc() returned NULL");
		return NULL;
	}
	sceClibMemset(font, 0, sizeof(vita2d_pgf));

	SceFont_t_initRec params = {
		font,
		numFonts,
		NULL,
		pgf_alloc_func,
		pgf_free_func,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	};

	font->lib_handle = sceFontNewLib(&params, &error);
	if (error != 0) {
		SCE_DBG_LOG_ERROR("[PGF] sceFontNewLib(): 0x%X", error);
		heap_free_heap_memory(vita2d_heap_internal, font);
		return NULL;
	}
	return font;
}

vita2d_pgf *vita2d_load_system_pgf(int numFonts, const vita2d_system_pgf_config *configs)
{
	if (numFonts < 1) {
		SCE_DBG_LOG_ERROR("[PGF] Invalid argument: numFonts");
		return NULL;
	}

	unsigned int error;
	int i;

	vita2d_pgf *font = vita2d_load_pgf_pre(numFonts);

	if (!font) {
		SCE_DBG_LOG_ERROR("[PGF] vita2d_load_pgf_pre() returned NULL");
		return NULL;
	}

	SceFont_t_fontStyleInfo style;
	sceClibMemset(&style, 0x00, sizeof(style));
	style.hSize = 10;
	style.vSize = 10;

	vita2d_pgf_font_handle *tmp = NULL;

	for (i = 0; i < numFonts; i++) {
		style.languageCode = configs[i].code;
		int index = sceFontFindOptimumFont(font->lib_handle, &style, &error);
		if (error != 0)
			goto cleanup;

		SceFont_t_fontId handle = sceFontOpen(font->lib_handle, index, 0, &error);
		if (error != 0)
			goto cleanup;

		if (font->font_handle_list == NULL) {
			tmp = font->font_handle_list = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(vita2d_pgf_font_handle));
		} else {
			tmp = tmp->next = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(vita2d_pgf_font_handle));
		}
		if (!tmp) {
			sceFontClose(handle);
			goto cleanup;
		}

		sceClibMemset(tmp, 0, sizeof(vita2d_pgf_font_handle));
		tmp->font_handle = handle;
		tmp->in_font_group = configs[i].in_font_group;
	}

	vita2d_load_pgf_post(font);

	return font;

cleanup:
	tmp = font->font_handle_list;
	while (tmp) {
		sceFontClose(tmp->font_handle);
		vita2d_pgf_font_handle *next = tmp->next;
		heap_free_heap_memory(vita2d_heap_internal, tmp);
		tmp = next;
	}
	sceFontDoneLib(font->lib_handle);
	heap_free_heap_memory(vita2d_heap_internal, font);
	return NULL;
}

vita2d_pgf *vita2d_load_default_pgf()
{
	vita2d_system_pgf_config configs[] = {
		{SCE_FONT_DEFAULT_LANGUAGE_CODE, NULL},
	};

	return vita2d_load_system_pgf(1, configs);
}

vita2d_pgf *vita2d_load_custom_pgf(const char *path)
{
	unsigned int error;
	vita2d_pgf *font = vita2d_load_pgf_pre(1);

	if (!font) {
		SCE_DBG_LOG_ERROR("[PGF] vita2d_load_pgf_pre() returned NULL");
		return NULL;
	}

	vita2d_pgf_font_handle *handle = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(vita2d_pgf_font_handle));
	if (!handle) {
		SCE_DBG_LOG_ERROR("[PGF] heap_alloc_heap_memory() returned NULL");
		heap_free_heap_memory(vita2d_heap_internal, font);
		return NULL;
	}

	SceFont_t_fontId font_handle = sceFontOpenUserFile(font->lib_handle, (char *)path, 1, &error);
	if (error != 0) {
		SCE_DBG_LOG_ERROR("[PGF] sceFontOpenUserFile(): 0x%X", error);
		sceFontDoneLib(font->lib_handle);
		heap_free_heap_memory(vita2d_heap_internal, handle);
		heap_free_heap_memory(vita2d_heap_internal, font);
		return NULL;
	}
	sceClibMemset(handle, 0, sizeof(vita2d_pgf_font_handle));
	handle->font_handle = font_handle;
	font->font_handle_list = handle;

	vita2d_load_pgf_post(font);

	return font;
}

vita2d_pgf *vita2d_load_custom_pgf_buffer(void* buf, SceSize bufSize)
{
	unsigned int error;
	vita2d_pgf *font = vita2d_load_pgf_pre(1);

	if (!font) {
		SCE_DBG_LOG_ERROR("[PGF] vita2d_load_pgf_pre() returned NULL");
		return NULL;
	}

	vita2d_pgf_font_handle *handle = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(vita2d_pgf_font_handle));
	if (!handle) {
		SCE_DBG_LOG_ERROR("[PGF] heap_alloc_heap_memory() returned NULL");
		heap_free_heap_memory(vita2d_heap_internal, font);
		return NULL;
	}

	SceFont_t_fontId font_handle = sceFontOpenUserMemory(font->lib_handle, buf, bufSize, &error);
	if (error != 0) {
		SCE_DBG_LOG_ERROR("[PGF] sceFontOpenUserFile(): 0x%X", error);
		sceFontDoneLib(font->lib_handle);
		heap_free_heap_memory(vita2d_heap_internal, handle);
		heap_free_heap_memory(vita2d_heap_internal, font);
		return NULL;
	}
	sceClibMemset(handle, 0, sizeof(vita2d_pgf_font_handle));
	handle->font_handle = font_handle;
	font->font_handle_list = handle;

	vita2d_load_pgf_post(font);

	return font;
}

void vita2d_free_pgf(vita2d_pgf *font)
{
	if (font) {
		sceKernelDeleteLwMutex(&font->mutex);

		vita2d_pgf_font_handle *tmp = font->font_handle_list;
		while (tmp) {
			sceFontClose(tmp->font_handle);
			vita2d_pgf_font_handle *next = tmp->next;
			heap_free_heap_memory(vita2d_heap_internal, tmp);
			tmp = next;
		}
		sceFontDoneLib(font->lib_handle);
		texture_atlas_free(font->atlas);
		heap_free_heap_memory(vita2d_heap_internal, font);
	}
}

static int atlas_add_glyph(vita2d_pgf *font, unsigned int character)
{
	SceFont_t_fontId font_handle = font->font_handle_list->font_handle;
	SceFont_t_charInfo char_info;
	bp2d_position position;
	void *texture_data;
	vita2d_texture *tex = font->atlas->texture;

	vita2d_pgf_font_handle *tmp = font->font_handle_list;
	while (tmp) {
		if (tmp->in_font_group == NULL || tmp->in_font_group(character)) {
			font_handle = tmp->font_handle;
			break;
		}
		tmp = tmp->next;
	}

	if (sceFontGetCharInfo(font_handle, character, &char_info) < 0)
		return 0;

	bp2d_size size = {
		char_info.bitmapWidth,
		char_info.bitmapHeight
	};

	texture_atlas_entry_data data = {
		char_info.bitmapLeft,
		char_info.bitmapTop,
		char_info.glyphMetrics.horizontalAdvance64,
		char_info.glyphMetrics.verticalAdvance64,
		0
	};

	if (!texture_atlas_insert(font->atlas, character, &size, &data,
				  &position))
			return 0;

	texture_data = vita2d_texture_get_datap(tex);

	SceFont_t_userImageBufferRec glyph_image;
	glyph_image.pixelFormat = SCE_FONT_USERIMAGE_DIRECT8;
	glyph_image.xPos64 = position.x << 6;
	glyph_image.yPos64 = position.y << 6;
	glyph_image.rect.width = vita2d_texture_get_width(tex);
	glyph_image.rect.height = vita2d_texture_get_height(tex);
	glyph_image.bytesPerLine = vita2d_texture_get_stride(tex);
	glyph_image.reserved = 0;
	glyph_image.buffer = (SceFont_t_u8 *)texture_data;

	return sceFontGetCharGlyphImage(font_handle, character, &glyph_image) == 0;
}

int generic_pgf_draw_text(vita2d_pgf *font, int draw, int *height,
			  int x, int y, float linespace, unsigned int color, float scale,
			  const char *text)
{
	sceKernelLockLwMutex(&font->mutex, 1, NULL);

	int i;
	unsigned int character;
	bp2d_rectangle rect;
	texture_atlas_entry_data data;
	vita2d_texture *tex = font->atlas->texture;
	int start_x = x;
	int max_x = 0;
	int pen_x = x;
	int pen_y = y;

	for (i = 0; text[i];) {
		i += utf8_to_ucs2(&text[i], &character);

		if (character == '\n') {
			if (pen_x > max_x)
				max_x = pen_x;
			pen_x = start_x;
			pen_y += font->vsize * scale + linespace;
			continue;
		}

		if (!texture_atlas_get(font->atlas, character, &rect, &data)) {
			if (!atlas_add_glyph(font, character)) {
				continue;
			}

			if (!texture_atlas_get(font->atlas, character,
					       &rect, &data))
					continue;
		}

		if (draw) {
			vita2d_draw_texture_tint_part_scale(tex,
				pen_x + data.bitmap_left * scale,
				pen_y - data.bitmap_top * scale,
				rect.x, rect.y, rect.w, rect.h,
				scale,
				scale,
				color);
		}

		pen_x += (data.advance_x >> 6) * scale;
	}

	if (pen_x > max_x)
		max_x = pen_x;

	if (height)
		*height = pen_y + font->vsize * scale - y;

	sceKernelUnlockLwMutex(&font->mutex, 1);

	return max_x - x;
}

int vita2d_pgf_draw_text(vita2d_pgf *font, int x, int y,
			 unsigned int color, float scale,
			 const char *text)
{
	return generic_pgf_draw_text(font, 1, NULL, x, y, 0.0f, color, scale, text);
}

int vita2d_pgf_draw_textf(vita2d_pgf *font, int x, int y,
			  unsigned int color, float scale,
			  const char *text, ...)
{
	char buf[1024];
	va_list argptr;
	va_start(argptr, text);
	sceClibVsnprintf(buf, sizeof(buf), text, argptr);
	va_end(argptr);
	return vita2d_pgf_draw_text(font, x, y, color, scale, buf);
}

int vita2d_pgf_draw_text_ls(vita2d_pgf *font, int x, int y, float linespace,
			 unsigned int color, float scale,
			 const char *text)
{
	return generic_pgf_draw_text(font, 1, NULL, x, y, linespace, color, scale, text);
}

int vita2d_pgf_draw_textf_ls(vita2d_pgf *font, int x, int y, float linespace,
			  unsigned int color, float scale,
			  const char *text, ...)
{
	char buf[1024];
	va_list argptr;
	va_start(argptr, text);
	sceClibVsnprintf(buf, sizeof(buf), text, argptr);
	va_end(argptr);
	return vita2d_pgf_draw_text_ls(font, x, y, linespace, color, scale, buf);
}

void vita2d_pgf_text_dimensions(vita2d_pgf *font, float scale,
				const char *text, int *width, int *height)
{
	int w;
	w = generic_pgf_draw_text(font, 0, height, 0.0f, 0, 0, 0, scale, text);

	if (width)
		*width = w;
}

int vita2d_pgf_text_width(vita2d_pgf *font, float scale, const char *text)
{
	int width;
	vita2d_pgf_text_dimensions(font, scale, text, &width, NULL);
	return width;
}

int vita2d_pgf_text_height(vita2d_pgf *font, float scale, const char *text)
{
	int height;
	vita2d_pgf_text_dimensions(font, scale, text, NULL, &height);
	return height;
}

#include <psp2/pvf.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>
#include <math.h>
#include "vita2d_sys.h"
#include "texture_atlas.h"
#include "bin_packing_2d.h"
#include "utils.h"
#include "shared.h"

#define ATLAS_DEFAULT_W 512
#define ATLAS_DEFAULT_H 512

#define PVF_GLYPH_MARGIN 2

extern void* mspace_internal;

extern void* sceClibMspaceMemalign(void* space, unsigned int alignment, unsigned int size);
extern void* sceClibMspaceRealloc(void* space, void* adress, unsigned int size);

typedef struct vita2d_pvf_font_handle {
	ScePvfFontId font_handle;
	int (*in_font_group)(unsigned int c);
	struct vita2d_pvf_font_handle *next;
} vita2d_pvf_font_handle;

typedef struct vita2d_pvf {
	ScePvfLibId lib_handle;
	vita2d_pvf_font_handle *font_handle_list;
	texture_atlas *atlas;
	SceKernelLwMutexWork mutex;
	float vsize;
} vita2d_pvf;

static void *pvf_alloc_func(void *userdata, unsigned int size)
{
	return sceClibMspaceMemalign(mspace_internal, sizeof(int), (size + sizeof(int) - 1) / sizeof(int) * sizeof(int));
}

static void *pvf_realloc_func(void *userdata, void *old_ptr, unsigned int size)
{
	return sceClibMspaceRealloc(mspace_internal, old_ptr, (size + sizeof(int) - 1) / sizeof(int) * sizeof(int));
}

static void pvf_free_func(void *userdata, void *p)
{
	sceClibMspaceFree(mspace_internal, p);
}

static void vita2d_load_pvf_post(vita2d_pvf *font)
{
	ScePvfIrect irectinfo;

	scePvfGetCharImageRect(font->font_handle_list->font_handle, 0x0057, &irectinfo);
	font->vsize = irectinfo.height;

	font->atlas = texture_atlas_create(ATLAS_DEFAULT_W, ATLAS_DEFAULT_H,
		SCE_GXM_TEXTURE_FORMAT_U8_R111);

	sceKernelCreateLwMutex(&font->mutex, "vita2d_pvf_mutex", 2, 0, NULL);
}

static vita2d_pvf *vita2d_load_pvf_pre(int numFonts)
{
	ScePvfError error;

	vita2d_pvf *font = sceClibMspaceMalloc(mspace_internal, sizeof(*font));
	if (!font)
		return NULL;

	sceClibMemset(font, 0, sizeof(vita2d_pvf));

	ScePvfInitRec params = {
		NULL,
		numFonts,
		NULL,
		NULL,
		pvf_alloc_func,
		pvf_realloc_func,
		pvf_free_func
	};

	font->lib_handle = scePvfNewLib(&params, &error);
	if (error != 0) {
		sceClibMspaceFree(mspace_internal, font);
		return NULL;
	}

	scePvfSetEM(font->lib_handle, 72.0f / (10.125f * 128.0f));
	scePvfSetResolution(font->lib_handle, 128.0f, 128.0f);

	return font;
}

vita2d_pvf *vita2d_load_system_pvf(int numFonts, const vita2d_system_pvf_config *configs, float hSize, float vSize)
{
	if (numFonts < 1) {
		return NULL;
	}

	ScePvfError error;
	int i;

	vita2d_pvf *font = vita2d_load_pvf_pre(numFonts);

	if (!font)
		return NULL;

	vita2d_pvf_font_handle *tmp = NULL;

	for (i = 0; i < numFonts; i++) {
		ScePvfFontStyleInfo style;

		sceClibMemset(&style, 0, sizeof(style));
		style.languageCode = configs[i].language;
		style.familyCode = configs[i].family;
		style.style = configs[i].style;

		ScePvfFontIndex index = scePvfFindOptimumFont(font->lib_handle, &style, &error);
		if (error != 0)
			goto cleanup;

		ScePvfFontId handle = scePvfOpen(font->lib_handle, index, 0, &error);
		if (error != 0)
			goto cleanup;

		scePvfSetCharSize(handle, hSize, vSize);

		if (font->font_handle_list == NULL) {
			tmp = font->font_handle_list = sceClibMspaceMalloc(mspace_internal, sizeof(vita2d_pvf_font_handle));
		} else {
			tmp = tmp->next = sceClibMspaceMalloc(mspace_internal, sizeof(vita2d_pvf_font_handle));
		}
		if (!tmp) {
			scePvfClose(handle);
			goto cleanup;
		}

		sceClibMemset(tmp, 0, sizeof(vita2d_pvf_font_handle));
		tmp->font_handle = handle;
		tmp->in_font_group = configs[i].in_font_group;
	}

	vita2d_load_pvf_post(font);

	return font;

cleanup:
	tmp = font->font_handle_list;
	while (tmp) {
		scePvfClose(tmp->font_handle);
		vita2d_pvf_font_handle *next = tmp->next;
		sceClibMspaceFree(mspace_internal, tmp);
		tmp = next;
	}
	scePvfDoneLib(font->lib_handle);
	sceClibMspaceFree(mspace_internal, font);
	return NULL;
}

vita2d_pvf *vita2d_load_default_pvf()
{
	vita2d_system_pvf_config configs[] = {
		{SCE_PVF_DEFAULT_LANGUAGE_CODE, SCE_PVF_DEFAULT_FAMILY_CODE, SCE_PVF_DEFAULT_STYLE_CODE, NULL},
	};

	return vita2d_load_system_pvf(1, configs, 10.125f, 10.125f);
}

vita2d_pvf *vita2d_load_custom_pvf(const char *path, float hSize, float vSize)
{
	ScePvfError error;
	vita2d_pvf *font = vita2d_load_pvf_pre(1);

	if (!font)
		return NULL;

	vita2d_pvf_font_handle *handle = sceClibMspaceMalloc(mspace_internal, sizeof(vita2d_pvf_font_handle));
	if (!handle) {
		sceClibMspaceFree(mspace_internal, font);
		return NULL;
	}

	ScePvfFontId font_handle = scePvfOpenUserFile(font->lib_handle, (char *)path, 1, &error);
	if (error != 0) {
		scePvfDoneLib(font->lib_handle);
		sceClibMspaceFree(mspace_internal, handle);
		sceClibMspaceFree(mspace_internal, font);
		return NULL;
	}

	scePvfSetCharSize(font_handle, hSize, vSize);

	sceClibMemset(handle, 0, sizeof(vita2d_pvf_font_handle));
	handle->font_handle = font_handle;
	font->font_handle_list = handle;

	vita2d_load_pvf_post(font);

	return font;
}

void vita2d_free_pvf(vita2d_pvf *font)
{
	if (font) {
		sceKernelDeleteLwMutex(&font->mutex);

		vita2d_pvf_font_handle *tmp = font->font_handle_list;
		while (tmp) {
			scePvfClose(tmp->font_handle);
			vita2d_pvf_font_handle *next = tmp->next;
			sceClibMspaceFree(mspace_internal, tmp);
			tmp = next;
		}
		scePvfDoneLib(font->lib_handle);
		texture_atlas_free(font->atlas);
		sceClibMspaceFree(mspace_internal, font);
	}
}

ScePvfFontId get_font_for_character(vita2d_pvf *font, unsigned int character)
{
	ScePvfFontId font_handle = font->font_handle_list->font_handle;
	vita2d_pvf_font_handle *tmp = font->font_handle_list;

	while (tmp) {
		if (tmp->in_font_group == NULL || tmp->in_font_group(character)) {
			font_handle = tmp->font_handle;
			break;
		}
		tmp = tmp->next;
	}

	return font_handle;
}

static int atlas_add_glyph(vita2d_pvf *font, ScePvfFontId font_handle, unsigned int character)
{
	ScePvfCharInfo char_info;
	ScePvfIrect char_image_rect;
	bp2d_position position;
	void *texture_data;
	vita2d_texture *tex = font->atlas->texture;

	if (scePvfGetCharInfo(font_handle, character, &char_info) < 0)
		return 0;

	if (scePvfGetCharImageRect(font_handle, character, &char_image_rect) < 0)
		return 0;

	bp2d_size size = {
		char_image_rect.width + 2 * PVF_GLYPH_MARGIN,
		char_image_rect.height + 2 * PVF_GLYPH_MARGIN
	};

	texture_atlas_entry_data data = {
		char_info.glyphMetrics.horizontalBearingX64 >> 6,
		char_info.glyphMetrics.horizontalBearingY64 >> 6,
		char_info.glyphMetrics.horizontalAdvance64,
		char_info.glyphMetrics.verticalAdvance64,
		0
	};

	if (!texture_atlas_insert(font->atlas, character, &size, &data,
				  &position))
			return 0;

	texture_data = vita2d_texture_get_datap(tex);

	ScePvfUserImageBufferRec glyph_image;
	glyph_image.pixelFormat = SCE_PVF_USERIMAGE_DIRECT8;
	glyph_image.xPos64 = ((position.x + PVF_GLYPH_MARGIN) << 6) - char_info.glyphMetrics.horizontalBearingX64;
	glyph_image.yPos64 = ((position.y + PVF_GLYPH_MARGIN) << 6) + char_info.glyphMetrics.horizontalBearingY64;
	glyph_image.rect.width = vita2d_texture_get_width(tex);
	glyph_image.rect.height = vita2d_texture_get_height(tex);
	glyph_image.bytesPerLine = vita2d_texture_get_stride(tex);
	glyph_image.reserved = 0;
	glyph_image.buffer = (ScePvfU8 *)texture_data;

	return scePvfGetCharGlyphImage(font_handle, character, &glyph_image) == 0;
}

int generic_pvf_draw_text(vita2d_pvf *font, int draw, int *height,
			  int x, int y, float linespace, unsigned int color, float scale,
			  const char *text)
{
	sceKernelLockLwMutex(&font->mutex, 1, NULL);

	int i;
	unsigned int character;
	ScePvfFontId fontid;
	bp2d_rectangle rect;
	texture_atlas_entry_data data;
	ScePvfKerningInfo kerning_info;
	unsigned int old_character = 0;
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
			pen_y += font->vsize * scale + (font->vsize / 2);
			continue;
		}

		fontid = get_font_for_character(font, character);

		if (!texture_atlas_get(font->atlas, character, &rect, &data)) {
			if (!atlas_add_glyph(font, fontid, character))
				continue;

			if (!texture_atlas_get(font->atlas, character,
					       &rect, &data))
					continue;
		}

		if (old_character) {
			if (scePvfGetKerningInfo(fontid, old_character, character, &kerning_info) >= 0) {
				pen_x += kerning_info.fKerningInfo.xOffset;
				pen_y += kerning_info.fKerningInfo.yOffset;
			}
		}

		if (draw) {
			vita2d_draw_texture_tint_part_scale(tex,
				pen_x + data.bitmap_left * scale,
				pen_y - data.bitmap_top * scale,
				rect.x + PVF_GLYPH_MARGIN / 2.0f, rect.y + PVF_GLYPH_MARGIN / 2.0f,
				rect.w - PVF_GLYPH_MARGIN / 2.0f, rect.h - PVF_GLYPH_MARGIN / 2.0f,
				scale,
				scale,
				color);
		}

		pen_x += (data.advance_x >> 6) * scale;
		old_character = character;
	}

	if (pen_x > max_x)
		max_x = pen_x;

	if (height)
		*height = pen_y + font->vsize * scale - y;

	sceKernelUnlockLwMutex(&font->mutex, 1);

	return max_x - x;
}

int vita2d_pvf_draw_text(vita2d_pvf *font, int x, int y,
			 unsigned int color, float scale,
			 const char *text)
{
	return generic_pvf_draw_text(font, 1, NULL, x, y, 0.0f, color, scale, text);
}

int vita2d_pvf_draw_textf(vita2d_pvf *font, int x, int y,
			  unsigned int color, float scale,
			  const char *text, ...)
{
	char buf[1024];
	va_list argptr;
	va_start(argptr, text);
	sceClibVsnprintf(buf, sizeof(buf), text, argptr);
	va_end(argptr);
	return vita2d_pvf_draw_text(font, x, y, color, scale, buf);
}

int vita2d_pvf_draw_text_ls(vita2d_pvf *font, int x, int y, float linespace,
			 unsigned int color, float scale,
			 const char *text)
{
	return generic_pvf_draw_text(font, 1, NULL, x, y, linespace, color, scale, text);
}

int vita2d_pvf_draw_textf_ls(vita2d_pvf *font, int x, int y, float linespace,
			  unsigned int color, float scale,
			  const char *text, ...)
{
	char buf[1024];
	va_list argptr;
	va_start(argptr, text);
	sceClibVsnprintf(buf, sizeof(buf), text, argptr);
	va_end(argptr);
	return vita2d_pvf_draw_text_ls(font, x, y, linespace, color, scale, buf);
}

void vita2d_pvf_text_dimensions(vita2d_pvf *font, float scale,
				const char *text, int *width, int *height)
{
	int w;
	w = generic_pvf_draw_text(font, 0, height, 0.0f, 0, 0, 0, scale, text);

	if (width)
		*width = w;
}

int vita2d_pvf_text_width(vita2d_pvf *font, float scale, const char *text)
{
	int width;
	vita2d_pvf_text_dimensions(font, scale, text, &width, NULL);
	return width;
}

int vita2d_pvf_text_height(vita2d_pvf *font, float scale, const char *text)
{
	int height;
	vita2d_pvf_text_dimensions(font, scale, text, NULL, &height);
	return height;
}

int vita2d_pvf_irect_maxheight(vita2d_pvf *font)
{
	ScePvfIrect irectinfo;
	scePvfGetCharImageRect(font->font_handle_list->font_handle, 0x0057, &irectinfo);

	return irectinfo.height;
}

int vita2d_pvf_irect_maxwidth(vita2d_pvf *font)
{
	ScePvfIrect irectinfo;
	scePvfGetCharImageRect(font->font_handle_list->font_handle, 0x0057, &irectinfo);

	return irectinfo.width;
}

void vita2d_pvf_embolden_rate(vita2d_pvf *font, float em)
{
	scePvfSetEmboldenRate(font->font_handle_list->font_handle, em);
}

void vita2d_pvf_skew_rate(vita2d_pvf *font, float ax, float ay)
{
	scePvfSetSkewValue(font->font_handle_list->font_handle, ax, ay);
}

void vita2d_pvf_char_size(vita2d_pvf *font, float hs, float vs)
{
	scePvfSetCharSize(font->font_handle_list->font_handle, hs, vs);
}
#include "vita2d_sys.h"
#include <psp2/kernel/threadmgr.h> 

typedef struct vita2d_sys_button {
	float xPos;
	float yPos;
	vita2d_texture *base;
	vita2d_texture *highlight;
	vita2d_texture *mask;
	vita2d_texture *bticon;
	vita2d_pvf *font;
	const char *text;
	int highlight_status;
	int status;
	int type;
	int baseTintR;
	int baseTintG;
	int baseTintB;
} vita2d_sys_button;

typedef struct vita2d_sys_marker {
	float xPos;
	float yPos;
	float xScale;
	float yScale;
	vita2d_texture *tex;
	int status;
	int type;
} vita2d_sys_marker;

typedef struct vita2d_sys_header {
	vita2d_texture *tex;
	vita2d_pvf *font;
	const char *text;
	float textPos;
	int status;
} vita2d_sys_header;

extern void* mspace_internal;

extern void* sceClibMspaceMalloc(void* space, unsigned int size);
extern void sceClibMspaceFree(void* space, void* adress);
extern void* sceClibMspaceMemalign(void* space, unsigned int alignment, unsigned int size);
extern void* sceClibMspaceRealloc(void* space, void* adress, unsigned int size);

/* main highlight lock */

static int highlight_lock_internal = 0;

/* touch highlight (white marker, tied to button) */

static int *btnptr_temp;
static int highlight_status_internal = 0;
static int highlight_internal = 0;
static int highlight_lock_btn_internal = 0;

/* physical buttons highlight (blue marker, independent) */

static vita2d_sys_marker sys_marker_normal;
static int marker_iterator_internal = 0;
static int marker_timer_internal = 0;
static int highlight_lock_marker_internal = 0;

/* normal button */

vita2d_texture *base_btn_normal;
vita2d_texture *highlight_btn_normal;

/* left corner button */

vita2d_texture *base_btn_l_corner;
vita2d_texture *highlight_btn_l_corner;
vita2d_texture *mask_btn_l_corner;

/* settings header */

vita2d_sys_header sys_header;

/* loads textures for buttons */

void vita2d_sys_load_tex_button_normal()
{
	base_btn_normal = vita2d_load_BMP_file("app0:resource/v2ds/button/normal_base.bmp");
	highlight_btn_normal = vita2d_load_BMP_file("app0:resource/v2ds/button/normal_highlight.bmp");
}

void vita2d_sys_load_tex_button_l_corner()
{
	base_btn_l_corner = vita2d_load_BMP_file("app0:resource/v2ds/button/l_corner_button_base.bmp");
	mask_btn_l_corner = vita2d_load_BMP_file("app0:resource/v2ds/button/l_corner_button_mask.bmp");
	highlight_btn_l_corner = vita2d_load_BMP_file("app0:resource/v2ds/button/l_corner_button_highlight.bmp");
}

/* creates one instance of normal button */

vita2d_sys_button *vita2d_sys_create_button_normal(float x, float y, const char *text, const char *icon)
{
	vita2d_sys_button *sys_button_normal = sceClibMspaceMalloc(mspace_internal, sizeof(*sys_button_normal));
	sys_button_normal->xPos = x;
	sys_button_normal->yPos = y;

	vita2d_texture *bticon;
	if (icon != NULL) {
		bticon = vita2d_load_BMP_file(icon);
		sys_button_normal->bticon = bticon;
	}
	else
		sys_button_normal->bticon = NULL;

	sys_button_normal->base = base_btn_normal;
	sys_button_normal->highlight = highlight_btn_normal;
	sys_button_normal->mask = NULL;
	sys_button_normal->highlight_status = 0;

	vita2d_system_pvf_config configs[] = {
		{SCE_PVF_LANGUAGE_LATIN, SCE_PVF_FAMILY_SANSERIF, SCE_PVF_STYLE_BOLD, NULL},
	};

	vita2d_pvf *font = vita2d_load_system_pvf(1, configs, 15.0f, 16.0f);

	sys_button_normal->font = font;
	sys_button_normal->text = text;

	sys_button_normal->status = 1;
	sys_button_normal->type = 0;

	sys_button_normal->baseTintR = 0;
	sys_button_normal->baseTintG = 0;
	sys_button_normal->baseTintB = 0;

	return sys_button_normal;
}

/* create physical buttons marker */

void vita2d_sys_create_marker(float x, float y, int type)
{
	sys_marker_normal.xPos = x;
	sys_marker_normal.yPos = y;
	sys_marker_normal.xScale = 0;
	sys_marker_normal.yScale = 0;
	sys_marker_normal.status = 0;
	switch (type) {
	case 0:
		sys_marker_normal.type = 0;
		sys_marker_normal.tex = highlight_btn_normal;
		break;
	case 1:
		sys_marker_normal.type = 1;
		sys_marker_normal.tex = highlight_btn_l_corner;
		break;
	}
}

/* change visibility state of the marker */

void vita2d_sys_visibility_marker(int val)
{
	if (val && highlight_lock_btn_internal == 0) {
		highlight_internal = 250;
		sys_marker_normal.status = 1;
		highlight_status_internal = 1;
		highlight_lock_marker_internal = 1;
	}
	else if (!val && highlight_lock_btn_internal == 0) {
		sys_marker_normal.xScale = 0;
		sys_marker_normal.yScale = 0;
		highlight_internal = 0;
		sys_marker_normal.status = 0;
		highlight_status_internal = 0;
		marker_timer_internal = 0;
		highlight_lock_marker_internal = 0;
		marker_iterator_internal = 0;
	}
}

/* set marker coordinates */

void vita2d_sys_coordinates_marker(float x, float y)
{
	sys_marker_normal.xPos = x;
	sys_marker_normal.yPos = y;
}

/* get marker x coordinate */

float vita2d_sys_getx_marker()
{
	return sys_marker_normal.xPos;
}

/* get marker y coordinate */

float vita2d_sys_gety_marker()
{
	return sys_marker_normal.yPos;
}

/* refresh marker timeout timer */

void vita2d_sys_refresh_marker()
{
	marker_timer_internal = 0;
}

/* change marker type */

void vita2d_sys_change_type_marker(int type)
{
	switch (type) {
	case 0:
		sys_marker_normal.xScale = 1.3f;
		sys_marker_normal.type = 0;
		sys_marker_normal.tex = highlight_btn_normal;
		break;
	case 1:
		sys_marker_normal.type = 1;
		sys_marker_normal.xScale = 1;
		sys_marker_normal.tex = highlight_btn_l_corner;
		break;
	}
}

/* move marker */

void vita2d_sys_move_marker(float delta)
{
	int initialVal, currentY;
	currentY = vita2d_sys_gety_marker();
	initialVal = currentY;
	if (delta > 0) {
		for (int y = currentY; y != initialVal + delta + 1; y++) {
			vita2d_sys_coordinates_marker(VITA2D_SYS_NORMAL_BTN_X_MARGIN, y);
			sceKernelDelayThread(500);
		}
	}
	else if (delta < 0) {
		for (int y = currentY; y != initialVal + delta - 1; y--) {
			vita2d_sys_coordinates_marker(VITA2D_SYS_NORMAL_BTN_X_MARGIN, y);
			sceKernelDelayThread(500);
		}
	}
}

/* universal marker drawing function */

void vita2d_sys_draw_marker()
{
	if (sys_marker_normal.status && marker_timer_internal != 8) {
		vita2d_draw_texture_tint_scale(sys_marker_normal.tex, sys_marker_normal.xPos, sys_marker_normal.yPos, sys_marker_normal.xScale, sys_marker_normal.yScale, RGBA8(3, 165, 255, highlight_internal));

		if (highlight_internal != 255 && marker_iterator_internal == 0) {
			highlight_internal = highlight_internal + 5;
			if (highlight_internal == 255) {
				marker_iterator_internal = 1;
				marker_timer_internal++;
			}
		}
		else if (highlight_internal != 25 && marker_iterator_internal == 1) {
			highlight_internal = highlight_internal - 5;
			if (highlight_internal == 25)
				marker_iterator_internal = 0;
		}

		if (highlight_status_internal) {
			float maxScale;
			if (sys_marker_normal.type == 0)
				maxScale = 1.3f;
			else
				maxScale = 1.0f;
			if (sys_marker_normal.xScale < maxScale) {
				sys_marker_normal.xScale = sys_marker_normal.xScale + 0.1;
				if (sys_marker_normal.yScale < 1.0f)
					sys_marker_normal.yScale = sys_marker_normal.yScale + 0.2;
				else {}
			}
			else {
				highlight_status_internal = 0;
			}
		}
	}
	else if (sys_marker_normal.status && marker_timer_internal == 8)
		vita2d_sys_visibility_marker(0);
}

/* universal button drawing function */

void vita2d_sys_draw_button(vita2d_sys_button *button)
{
	if (button->status) {
		switch (button->type) {
		case 0:
			vita2d_draw_texture_scale(button->base, button->xPos, button->yPos, 1.3f, 1);
			break;
		case 1:
			vita2d_draw_texture_tint(button->base, button->xPos, button->yPos, RGBA8(button->baseTintR, button->baseTintG, button->baseTintB, 255));
			vita2d_draw_texture(button->mask, button->xPos, button->yPos);
			break;
		}

		if (button->highlight_status == 1) {
			switch (button->type) {
			case 0:
				vita2d_draw_texture_tint_scale(button->highlight, button->xPos, button->yPos, 1.3f, 1, RGBA8(255, 255, 255, highlight_internal));
				break;
			case 1:
				vita2d_draw_texture_tint(button->highlight, button->xPos, button->yPos, RGBA8(255, 255, 255, highlight_internal));
				break;
			}

			switch (highlight_status_internal) {
			case 1:
				if (highlight_internal != 255)
					highlight_internal = highlight_internal + 51;
				else {
					highlight_lock_internal = 1;
					highlight_status_internal = 0;
				}
				break;
			case 2:
				if (highlight_internal != 0)
					highlight_internal = highlight_internal - 17;
				else {
					*btnptr_temp = 0;
					highlight_status_internal = 0;
					highlight_lock_internal = 0;
				}
				break;
			}
		}

		if (button->bticon != NULL) {
			switch (button->type) {
			case 0:
				vita2d_draw_texture(button->bticon, button->xPos + 112, button->yPos + 21);
				break;
			case 1:
				vita2d_draw_texture(button->bticon, button->xPos, button->yPos + 25);
				break;
			}
		}

		if (button->type == 0)
			vita2d_pvf_draw_text(button->font, button->xPos + 172, button->yPos + 50, RGBA8(255, 255, 255, 255), 1, button->text);
	}
}

/* make button visible/invisible (0 to hide, 1 to unhide) */

void vita2d_sys_visibility_button(vita2d_sys_button *button, int val)
{
	button->status = val;
}

/* change button coordinates */

void vita2d_sys_coordinates_button(vita2d_sys_button *button, float x, float y)
{
	button->xPos = x;
	button->yPos = y;
}

/* change button text */

void vita2d_sys_text_button(vita2d_sys_button *button, const char *text)
{
	button->text = text;
}

/* change button icon */

void vita2d_sys_icon_button(vita2d_sys_button *button, const char *icon)
{
	vita2d_texture *bticon;
	bticon = vita2d_load_BMP_file(icon);
	button->bticon = bticon;
}

/* get button x coordinate */

float vita2d_sys_getx_button(vita2d_sys_button *button)
{
	return button->xPos;
}

/* get button y coordinate */

float vita2d_sys_gety_button(vita2d_sys_button *button)
{
	return button->yPos;
}

/* make button highlighted/dehighlighted (for touch operation) */

void vita2d_sys_highlight_button(vita2d_sys_button *button, int val)
{
	if (val && highlight_lock_internal == 0 && highlight_lock_marker_internal == 0) {
		button->highlight_status = 1;
		highlight_status_internal = 1;
		highlight_lock_internal = 2;
		highlight_lock_btn_internal = 1;
	}
	else if (!val && highlight_lock_internal == 1 && button->highlight_status && highlight_lock_marker_internal == 0) {
		highlight_status_internal = 2;
		btnptr_temp = &button->highlight_status;
		highlight_lock_btn_internal = 0;
	}
}

/* create common settings header */

void vita2d_sys_create_settings_header(const char *text) 
{
	vita2d_set_clip_rectangle(0, 96, 960, 544);

	vita2d_texture *settings_separator = vita2d_load_BMP_file("app0:resource/v2ds/misc/separator.bmp");

	vita2d_system_pvf_config configs[] = {
		{SCE_PVF_LANGUAGE_LATIN, SCE_PVF_FAMILY_SANSERIF, SCE_PVF_STYLE_BOLD, NULL},
	};

	vita2d_pvf *font = vita2d_load_system_pvf(1, configs, 18.0f, 19.0f);

	int width = vita2d_pvf_text_width(font, 1, text);

	sys_header.tex = settings_separator;
	sys_header.textPos = 480 - width / 2;
	sys_header.font = font;
	sys_header.text = text;
	sys_header.status = 1;
}

/* draw common settings header */

void vita2d_sys_draw_settings_header()
{
	vita2d_draw_texture(sys_header.tex, 0, 94);
	vita2d_pvf_draw_text(sys_header.font, sys_header.textPos, 74, RGBA8(255, 255, 255, 255), 1, sys_header.text);
}

/* change settings header text */

void vita2d_sys_change_text_header(const char *text)
{
	int width = vita2d_pvf_text_width(sys_header.font, 1, text);

	sys_header.textPos = 480 - width / 2;
	sys_header.text = text;
}

/* hide settings header */

void vita2d_sys_hide_header()
{
	sys_header.status = 0;

	vita2d_set_clip_rectangle(0, 0, 0, 0);
}

/* show settings header */

void vita2d_sys_show_header(const char *text)
{
	sys_header.status = 1;

	vita2d_set_clip_rectangle(0, 96, 960, 544);
}

/* create back button */

vita2d_sys_button *vita2d_sys_create_button_l_corner(const char *icon, int colorR, int colorG, int colorB, int type)
{
	vita2d_sys_button *sys_button_l_corner = sceClibMspaceMalloc(mspace_internal, sizeof(*sys_button_l_corner));
	sys_button_l_corner->xPos = 0;
	sys_button_l_corner->yPos = 454;

	vita2d_texture *bticon;
	if (icon != NULL) {
		bticon = vita2d_load_BMP_file(icon);
	}
	else {
		switch (type) {
		case 0:
			bticon = vita2d_load_BMP_file("app0:resource/v2ds/button/l_corner_button_back_arrow.bmp");
			break;
		}
	}
	sys_button_l_corner->bticon = bticon;

	sys_button_l_corner->base = base_btn_l_corner;
	sys_button_l_corner->highlight = highlight_btn_l_corner;
	sys_button_l_corner->mask = mask_btn_l_corner;
	sys_button_l_corner->highlight_status = 0;

	sys_button_l_corner->font = NULL;
	sys_button_l_corner->text = NULL;

	sys_button_l_corner->status = 1;
	sys_button_l_corner->type = 1;

	sys_button_l_corner->baseTintR = colorR;
	sys_button_l_corner->baseTintG = colorG;
	sys_button_l_corner->baseTintB = colorB;

	return sys_button_l_corner;
}
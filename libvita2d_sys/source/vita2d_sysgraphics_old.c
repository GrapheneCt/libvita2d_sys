#include <psp2/kernel/clib.h>

#include "vita2d_sys.h"

extern void* mspace_internal;

static unsigned int uid_modifier = 0, highlight_iterator = 0;
static SceBool highlight_up = SCE_TRUE;

void *sceClibMspaceRealloc(void* mspace, void *ptr, SceSize new_size);

vita2d_sys_widget* vita2d_sys_create_widget_generic(vita2d_sys_widget* widget, vita2d_texture* texture, vita2d_pvf* font, float initX,
	float initY, float fontDeltaX, float fontDeltaY, unsigned int fontColor, const char* text)
{
	/*vita2d_sys_widget* widget = sceClibMspaceMalloc(mspace_internal, sizeof(vita2d_sys_widget));
	if (!widget)
		return NULL;*/

	//sceClibMemset(widget, 0, sizeof(vita2d_sys_widget));

	widget->widget_UID = 100 + uid_modifier;
	widget->x = initX;
	widget->y = initY;
	widget->font_x = initX + fontDeltaX;
	widget->font_y = initY + fontDeltaY;
	widget->tex = texture;
	widget->font = font;
	widget->visible = SCE_TRUE;
	widget->highlight = SCE_FALSE;
	widget->sub_widget = NULL;

	if (fontColor)
		widget->color_text = fontColor;
	else
		widget->color_text = RGBA8(255, 255, 255, 255);

	if (text != NULL) {
		int textlen = sceClibStrnlen(text, WIDGET_MAX_TEXT_LEN);
		void* strmem = sceClibMspaceMalloc(mspace_internal, textlen + 1);
		sceClibMemset(strmem, 0, textlen + 1);
		sceClibMemcpy(strmem, text, textlen);

		widget->p_text = (char *)strmem;
	}
	else
		widget->p_text = NULL;

	uid_modifier++;

	return widget;
}

vita2d_sys_widget* vita2d_sys_create_widget_slider(vita2d_texture* baseTex, vita2d_texture* pointerTex, int activeAxis, 
	float deadZone, float initX, float initY, float pointerDeltaX, float pointerDeltaY, float initPointerVal)
{
	vita2d_sys_widget* base_widget = vita2d_sys_create_widget_generic(NULL, baseTex, NULL, initX, initY, 0, 0, 0, NULL);

	vita2d_sys_widget* widget = sceClibMspaceMalloc(mspace_internal, sizeof(vita2d_sys_widget));
	if (!widget)
		return NULL;

	sceClibMemset(widget, 0, sizeof(vita2d_sys_widget));

	if (initPointerVal > 100)
		initPointerVal = 100;

	if (activeAxis == WIDGET_ACTIVE_AXIS_X) {
		widget->widget_UID = -100 - uid_modifier;
		float base_width = vita2d_texture_get_width(baseTex) - 2 * deadZone;
		widget->font_x = ((base_width * initPointerVal) / 100) + deadZone;
		widget->font_y = -deadZone;
	}
	else if (activeAxis == WIDGET_ACTIVE_AXIS_Y) {
		widget->widget_UID = -100 - uid_modifier;
		float base_height = vita2d_texture_get_height(baseTex) - 2 * deadZone;
		widget->font_y = ((base_height * initPointerVal) / 100) + deadZone;
		widget->font_x = -deadZone;
	}

	widget->x = initX + pointerDeltaX;
	widget->y = initY + pointerDeltaY;
	widget->tex = pointerTex;
	widget->visible = SCE_TRUE;
	widget->highlight = SCE_FALSE;
	widget->sub_widget = (void *)base_widget;
	widget->p_text = NULL;

	uid_modifier++;

	return widget;
}

void* sceClibMspaceCalloc(void* mspace, size_t num, size_t size);

vita2d_sys_widget* vita2d_sys_create_widget_button(vita2d_texture* buttonTex, vita2d_texture* iconTex, vita2d_pvf* font, float initX, float initY, 
	float iconDeltaX, float iconDeltaY, float fontDeltaX, float fontDeltaY, unsigned int fontColor, const char* text)
{
	vita2d_sys_widget* widget = sceClibMspaceCalloc(mspace_internal, 2, sizeof(vita2d_sys_widget));

	vita2d_sys_create_widget_generic(&widget[0], buttonTex, font, initX, initY, fontDeltaX, fontDeltaY, fontColor, text);

	if (iconTex != NULL) {
		widget[1].widget_UID = 100 + uid_modifier;
		widget[1].x = initX + iconDeltaX;
		widget[1].y = initY + iconDeltaY;
		widget[1].tex = iconTex;
		widget[1].visible = SCE_TRUE;
		widget[1].highlight = SCE_FALSE;
		widget[1].sub_widget = (void *)&widget[0];
		widget[1].p_text = NULL;

		uid_modifier++;

		return &widget[1];
	}
	else {
		uid_modifier++;

		return &widget[0];
	}

}

void vita2d_sys_widget_set_slider_position(vita2d_sys_widget* widget, float pointerVal)
{
	if (pointerVal > 100)
		pointerVal = 100;

	float deadZone;

	if (widget->font_x > 0) {
		deadZone = -widget->font_y;
		float base_width = vita2d_texture_get_width(widget->tex) - 2 * deadZone;
		widget->font_x = ((base_width * pointerVal) / 100) + deadZone;
	}
	else if (widget->font_y > 0) {
		deadZone = -widget->font_x;
		float base_height = vita2d_texture_get_height(widget->tex) - 2 * deadZone;
		widget->font_y = ((base_height * pointerVal) / 100) + deadZone;
	}
}

void vita2d_sys_widget_set_xy(vita2d_sys_widget* widget, float x, float y)
{
	widget->x = x;
	widget->y = y;
}

float vita2d_sys_widget_get_x(vita2d_sys_widget* widget)
{
	return widget->x;
}

float vita2d_sys_widget_get_y(vita2d_sys_widget* widget)
{
	return widget->y;
}

void vita2d_sys_widget_set_visibility(vita2d_sys_widget* widget, SceBool visibility)
{
	widget->visible = visibility;
}

void vita2d_sys_widget_set_highlight(vita2d_sys_widget* widget, SceBool highlight)
{
	widget->highlight = highlight;
}

void vita2d_sys_widget_set_text(vita2d_sys_widget* widget, const char* text)
{
	if (text != NULL && widget->p_text != NULL) {
		int textlen = sceClibStrnlen(text, WIDGET_MAX_TEXT_LEN);
		void* strmem = sceClibMspaceRealloc(mspace_internal, widget->p_text, textlen + 1);
		sceClibMemset(strmem, 0, textlen + 1);
		sceClibMemcpy(strmem, text, textlen);
		widget->p_text = (char *)strmem;
	}
	else if (text != NULL && widget->p_text == NULL) {
		int textlen = sceClibStrnlen(text, WIDGET_MAX_TEXT_LEN);
		void* strmem = sceClibMspaceMalloc(mspace_internal, textlen + 1);
		sceClibMemset(strmem, 0, textlen + 1);
		sceClibMemcpy(strmem, text, textlen);

		widget->p_text = (char *)strmem;
	}
	else if (text == NULL) {
		if (widget->p_text != NULL)
			sceClibMspaceFree(mspace_internal, widget->p_text);
		widget->p_text = NULL;
	}
}

void vita2d_sys_widget_set_highlight_max(void)
{
	highlight_iterator = 250;
}

void vita2d_sys_draw_widget(vita2d_sys_widget* widget)
{
	if (widget->sub_widget != NULL)
		vita2d_sys_draw_widget((vita2d_sys_widget *)widget->sub_widget);

	if (widget->widget_UID > 0) {
		if (widget->tex != NULL) {
			vita2d_draw_texture(widget->tex, widget->x, widget->y);

			if (widget->highlight) {
				vita2d_draw_texture_tint(widget->tex, widget->x, widget->y, RGBA8(76, 135, 230, highlight_iterator));
				switch (highlight_up) {
				case SCE_TRUE:
					highlight_iterator += 5;
					if (highlight_iterator == 255)
						highlight_up = SCE_FALSE;
					break;
				case SCE_FALSE:
					highlight_iterator -= 5;
					if (highlight_iterator == 0)
						highlight_up = SCE_TRUE;
					break;
				}
			}
		}

		if (widget->p_text != NULL)
			vita2d_pvf_draw_text(widget->font, widget->font_x, widget->font_y, widget->color_text, 1, widget->p_text);
	}
	else {
		if (widget->font_x > 0) {
			if (widget->tex != NULL)
				vita2d_draw_texture(widget->tex, widget->x + widget->font_x, widget->y);
		}
		else if (widget->font_y > 0) {
			if (widget->tex != NULL)
				vita2d_draw_texture(widget->tex, widget->x + widget->font_y, widget->y);
		}
	}
}

void vita2d_sys_delete_widget(vita2d_sys_widget* widget)
{
	if (widget->sub_widget != NULL)
		vita2d_sys_delete_widget((vita2d_sys_widget *)widget->sub_widget);

	if (widget->p_text != NULL)
		sceClibMspaceFree(mspace_internal, widget->p_text);

	sceClibMspaceFree(mspace_internal, widget);
}
#include <psp2/kernel/clib.h>
#include "vita2d_sys.h"

#include "heap.h"

extern void* vita2d_heap_internal;

static unsigned int uid_modifier = 0, highlight_iterator = 0;
static SceBool highlight_up = SCE_TRUE;

vita2d_sys_widget* vita2d_sys_create_widget_button(vita2d_texture* texture, vita2d_pvf* font, float initX, float initY, float fontDeltaX, float fontDeltaY, const char* text)
{
	vita2d_sys_widget* widget = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*widget));
	if (!widget)
		return NULL;

	widget->widget_UID = 100 + uid_modifier;
	widget->x = initX;
	widget->y = initY;
	widget->font_dx = fontDeltaX;
	widget->font_dy = fontDeltaY;
	widget->tex = texture;
	widget->font = font;
	widget->visible = SCE_TRUE;
	widget->highlight = SCE_FALSE;
	sceClibStrncpy(widget->text, text, WIDGET_BUTTON_MAX_TEXT_LEN);

	uid_modifier++;

	return widget;
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
	sceClibMemset(&widget->text, 0, WIDGET_BUTTON_MAX_TEXT_LEN);
	sceClibStrncpy(widget->text, text, WIDGET_BUTTON_MAX_TEXT_LEN);
}

void vita2d_sys_widget_set_highlight_max(void)
{
	highlight_iterator = 250;
}

void vita2d_sys_draw_widget(vita2d_sys_widget* widget)
{
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

	vita2d_pvf_draw_text(widget->font, widget->x + widget->font_dx, widget->y + widget->font_dy, RGBA8(255, 255, 255, 255), 1, widget->text);
}

void vita2d_sys_delete_widget(vita2d_sys_widget* widget)
{
	heap_free_heap_memory(vita2d_heap_internal, widget);
}
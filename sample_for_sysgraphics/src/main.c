#include <stdio.h>
#include <vita2d_sys.h>
#include <psp2/appmgr.h>
#include <psp2/io/fcntl.h>
#include <psp2/sysmodule.h>
#include <psp2/ctrl.h>
#include <psp2/kernel/threadmgr.h> 
#include <psp2/touch.h>

#define CLIB_HEAP_SIZE 6 * 1024 * 1024
#define ONPRESS(flag) ((ctrl.buttons & (flag)) && !(ctrl_old.buttons & (flag)))
#define ONDEPRESS(flag) ((ctrl_old.buttons & (flag)) && !(ctrl.buttons & (flag)))

extern void* sceClibMspaceCreate(void* base, uint32_t size);

typedef struct {
	int visible;
	int x;
	int y;
} TouchPoint;

static vita2d_sys_button *button[5];
static vita2d_sys_button *button1;

static TouchPoint s_frontTouch;

int ctrl(SceSize argc, void* argv)
{
	int touch_old, touch_new;
	SceCtrlData ctrl, ctrl_old = {};
	SceTouchData tdf;

	while (1) {

		s_frontTouch.visible = 0;

		sceTouchRead(SCE_TOUCH_PORT_FRONT, &tdf, 1);

		if (tdf.reportNum > 0) {
			s_frontTouch.visible = 1;
			s_frontTouch.x = tdf.report[0].x / 2;
			s_frontTouch.y = tdf.report[0].y / 2;
		}
		else
			s_frontTouch.visible = 0;

		touch_new = s_frontTouch.visible;

		ctrl_old = ctrl;
		sceCtrlReadBufferPositive(0, &ctrl, 1);
		if (ctrl.buttons) {
			vita2d_sys_visibility_marker(1);
			vita2d_sys_refresh_marker();
		}
		if (ONPRESS(SCE_CTRL_UP)) {
			if (vita2d_sys_gety_marker() == VITA2D_SYS_HEADER_VSIZE) {}
			else if (vita2d_sys_gety_marker() == 454) {
				vita2d_sys_change_type_marker(0);
				vita2d_sys_coordinates_marker(VITA2D_SYS_NORMAL_BTN_X_MARGIN, vita2d_sys_gety_button(button[4]));
			}
			else
				vita2d_sys_move_marker(-VITA2D_SYS_NORMAL_BTN_VSIZE);
		}
		else if (ONPRESS(SCE_CTRL_DOWN)) {
			if (vita2d_sys_gety_marker() == VITA2D_SYS_HEADER_VSIZE + VITA2D_SYS_NORMAL_BTN_VSIZE * 4) {
				vita2d_sys_change_type_marker(1);
				vita2d_sys_coordinates_marker(0, 454);
			}
			else if (vita2d_sys_gety_marker() == 454) {}
			else
				vita2d_sys_move_marker(VITA2D_SYS_NORMAL_BTN_VSIZE);
		}
		else if (ONPRESS(SCE_CTRL_CROSS)) {
			for (int i = 0; i != 5; i++) {
				if (vita2d_sys_gety_marker() == vita2d_sys_gety_button(button[i]))
					switch (i) {
					case 0:
						vita2d_sys_text_button(button[0], "Button 1");
						break;
					case 1:
						vita2d_sys_text_button(button[1], "Button 2");
						break;
					}
			}
		}

		for (int i = 0; i != 5; i++) {
			if (s_frontTouch.y < vita2d_sys_gety_button(button[i]) + VITA2D_SYS_NORMAL_BTN_VSIZE && s_frontTouch.y > vita2d_sys_gety_button(button[i]) && s_frontTouch.visible == 1) {
				if (touch_old != touch_new) {
					vita2d_sys_visibility_marker(0);
					vita2d_sys_highlight_button(button[i], 1);
				}
			}
			else {
				vita2d_sys_highlight_button(button[i], 0);
			}
			if (s_frontTouch.y < vita2d_sys_gety_button(button[i]) + VITA2D_SYS_NORMAL_BTN_VSIZE && s_frontTouch.y > vita2d_sys_gety_button(button[i]) && s_frontTouch.visible == 0)
				vita2d_sys_text_button(button[i], "This button was pressed using touch");
		}


		if (s_frontTouch.y < 544 && s_frontTouch.y > VITA2D_SYS_L_CORNER_BTN_Y && s_frontTouch.x < VITA2D_SYS_L_CORNER_BTN_HSIZE && s_frontTouch.visible == 1) {
			if (touch_old != touch_new) {
				vita2d_sys_visibility_marker(0);
				vita2d_sys_highlight_button(button1, 1);
			}
		}
		else
			vita2d_sys_highlight_button(button1, 0);

		touch_old = touch_new;

		sceKernelDelayThread(100);
	}
}

void initTouchPanel(void)
{
	SceTouchPanelInfo panelInfo;

	s_frontTouch.visible = 0;
	s_frontTouch.x = 0;
	s_frontTouch.y = 0;

	sceTouchGetPanelInfo(SCE_TOUCH_PORT_FRONT, &panelInfo);
	sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
}

int main() {

	void* mspace;
	void* clibm_base;
	SceUID clib_heap = sceKernelAllocMemBlock("ClibHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, CLIB_HEAP_SIZE, NULL);
	sceKernelGetMemBlockBase(clib_heap, &clibm_base);

	mspace = sceClibMspaceCreate(clibm_base, CLIB_HEAP_SIZE);

	vita2d_clib_pass_mspace(mspace);
	vita2d_init();
	vita2d_set_vblank_wait(0);
	vita2d_set_clear_color(0xFF000000);

	initTouchPanel();

	vita2d_sys_load_tex_button_normal();
	vita2d_sys_load_tex_button_l_corner();

	int y;
	y = VITA2D_SYS_HEADER_VSIZE;
	
	for (int i = 0; i != 5; i++) {
		button[i] = vita2d_sys_create_button_normal(VITA2D_SYS_NORMAL_BTN_X_MARGIN, y, "Test text", "app0:icon.bmp");
		y = y + VITA2D_SYS_NORMAL_BTN_VSIZE;
	}

	button1 = vita2d_sys_create_button_l_corner(NULL, 100, 25, 150, 0);

	vita2d_sys_create_marker(VITA2D_SYS_NORMAL_BTN_X_MARGIN, VITA2D_SYS_HEADER_VSIZE, 0);
	vita2d_sys_create_settings_header("Test Header");

	SceUID id_ctrl = sceKernelCreateThread("CtrlThread", ctrl, 191, 0x10, 0, 0, NULL);
	sceKernelStartThread(id_ctrl, 0, NULL);

	while (1){

		vita2d_start_drawing();
		vita2d_clear_screen();

		vita2d_sys_draw_settings_header();
		vita2d_enable_clipping();
		for (int i = 0; i != 5; i++) {
			vita2d_sys_draw_button(button[i]);
		}
		vita2d_sys_draw_button(button1);
		vita2d_sys_draw_marker();
		vita2d_disable_clipping();

		vita2d_end_drawing();
		vita2d_wait_rendering_done();
		vita2d_end_shfb();
	}

	return 0;

}


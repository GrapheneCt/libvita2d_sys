#include <stdio.h>
#include <vita2d_sys.h>
#include <psp2/appmgr.h>

void _start(unsigned int args, void *argp) {
	
	int x, size;
	x = 0;
	size = 0;

   	vita2d_init();

	vita2d_set_vblank_wait(0);

	vita2d_set_clear_color(0xFF000000);
	
	vita2d_pgf *font = vita2d_load_default_pgf();

	char mes1[17];
	char mes2[19];
	SceAppMgrBudgetInfo info = { 0 };
	info.size = 0x88;
	sceAppMgrGetBudgetInfo(&info);
	sprintf(mes1, "Free LPDDR2: %d MB", info.freeMain / 1024 / 1024);
	sprintf(mes2, "LPDDR2 Budget: %d MB", info.budgetMain / 1024 / 1024);

    while (1) {

        vita2d_start_drawing();
        vita2d_clear_screen();

		if (x == 544)
			x = 0;
		if (size == 100)
			size = 0;
		vita2d_draw_rectangle(70, x, size, size, RGBA8(225, 0, 0, 255));
		vita2d_draw_fill_circle(890, x, size, RGBA8(0, 255, 0, 255));
		x++;
		size++;
		vita2d_pgf_draw_text(font, 100, 100, RGBA8(0, 255, 0, 255), 1.5f, mes2);
		vita2d_pgf_draw_text(font, 100, 200, RGBA8(0, 255, 0, 255), 1.5f, mes1);

		vita2d_end_drawing();
		vita2d_wait_rendering_done();
		vita2d_end_shfb();
        
	}

}


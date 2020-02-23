#include <stdio.h>
#include <vita2d_sys.h>
#include <psp2/appmgr.h>

#define CLIB_HEAP_SIZE 1 * 1024 * 1024

extern void* sceClibMspaceCreate(void* base, uint32_t size);

typedef struct sceAppMgrBudgetInfo {
	SceSize size;                        //must be 0x88
	uint32_t mode;                       //2 = Mode_ApplicationA, 3 = Mode_ApplicationB (allow phycont), 4 = Mode_Application (allow 0x0E208060, allow phycont)
	uint32_t unk_4;
	uint32_t budgetLPDDR2;          //LPDDR2 budget in bytes
	uint32_t freeLPDDR2;              //Free LPDDR2 memory in bytes
	uint32_t allow0x0E208060;      //1 if 0x0E208060 usage is allowed, otherwise 0
	uint32_t unk_14;                    //only if 0x0E208060 is used, otherwise 0
	uint32_t budget0x0E208060;   //0x0E208060 budget in bytes
	uint32_t free0x0E208060;       //Free 0x0E208060 memory in bytes
	uint32_t unk_20;                    //always 0
	uint32_t unk_24;                    //only if phycont is used, otherwise 0
	uint32_t budgetPHYCONT;       //PHYCONT budget in bytes
	uint32_t freePHYCONT;           //Free PHYCONT memory in bytes
	uint32_t allow;                       //Some memory type?
	char unk_34[0x20];                //Some memory type?
	uint32_t unk_54;
	uint32_t budgetCDRAM;          //CDRAM budget in bytes
	uint32_t freeCDRAM;              //Free CDRAM memory in bytes
	char reserved_60[0x24];
} sceAppMgrBudgetInfo;

int sceAppMgrGetBudgetInfo(sceAppMgrBudgetInfo* info);

int main() {

	void* mspace;
	void* clibm_base;
	SceUID clib_heap = sceKernelAllocMemBlock("ClibHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, CLIB_HEAP_SIZE, NULL);
	sceKernelGetMemBlockBase(clib_heap, &clibm_base);

	mspace = sceClibMspaceCreate(clibm_base, CLIB_HEAP_SIZE);
	
	int x, size;
	x = 0;
	size = 0;

	vita2d_clib_pass_mspace(mspace);
    	vita2d_init();
	
	vita2d_pgf *font = vita2d_load_default_pgf();

	char mes1[17];
	char mes2[19];
	sceAppMgrBudgetInfo info = { 0 };
	info.size = 0x88;
	sceAppMgrGetBudgetInfo(&info);
	sprintf(mes1, "Free LPDDR2: %d", info.freeLPDDR2 / 1024 / 1024);
	sprintf(mes2, "LPDDR2 Budget: %d", info.budgetLPDDR2 / 1024 / 1024);

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
	}

	return 0;

}


#include <math.h>
#include <kernel.h>
#include <libdbg.h>
#include <fios2.h>
#include <appmgr.h>

#include "utils.h"

extern void *psDevData;
extern int system_mode_flag;

int readFile(const char *fileName, unsigned char *pBuffer, SceSize bufSize)
{
	int ret;
	SceUID fd;
	int remainSize;

	fd = sceIoOpen(fileName, SCE_O_RDONLY, 0);

	if (fd < 0) {
		SCE_DBG_LOG_ERROR("[UTILS] Can't open file %s sceIoOpen(): 0x%X", fileName, fd);
		return fd;
	}

	remainSize = bufSize;
	while (remainSize > 0) {
		ret = sceIoRead(fd, pBuffer, remainSize);
		pBuffer += ret;
		remainSize -= ret;
	}
	sceIoClose(fd);

	return SCE_OK;
}

int readFileFIOS2(char *fileName, unsigned char *pBuffer, SceSize bufSize)
{
	int ret;
	SceFiosFH fd;
	int remainSize;

	ret = sceFiosFHOpenSync(NULL, &fd, fileName, NULL);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[UTILS] Can't open file %s sceFiosFHOpenSync(): 0x%X", fileName, ret);
		return ret;
	}

	remainSize = bufSize;
	while (remainSize > 0) {
		ret = sceFiosFHReadSync(NULL, fd, pBuffer, remainSize);
		pBuffer += ret;
		remainSize -= ret;
	}
	sceFiosFHCloseSync(NULL, fd);

	return SCE_OK;
}

void matrix_copy(float *dst, const float *src)
{
	sceClibMemcpy(dst, src, sizeof(float)*4*4);
}

void matrix_identity4x4(float *m)
{
	m[0] = m[5] = m[10] = m[15] = 1.0f;
	m[1] = m[2] = m[3] = 0.0f;
	m[4] = m[6] = m[7] = 0.0f;
	m[8] = m[9] = m[11] = 0.0f;
	m[12] = m[13] = m[14] = 0.0f;
}

void matrix_mult4x4(const float *src1, const float *src2, float *dst)
{
	int i, j, k;
	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			dst[i*4 + j] = 0.0f;
			for (k = 0; k < 4; k++) {
				dst[i*4 + j] += src1[i*4 + k]*src2[k*4 + j];
			}
		}
	}
}

void matrix_set_x_rotation(float *m, float rad)
{
	float c = cosf(rad);
	float s = sinf(rad);

	matrix_identity4x4(m);

	m[0] = c;
	m[2] = -s;
	m[8] = s;
	m[10] = c;
}

void matrix_set_y_rotation(float *m, float rad)
{
	float c = cosf(rad);
	float s = sinf(rad);

	matrix_identity4x4(m);

	m[5] = c;
	m[6] = s;
	m[9] = -s;
	m[10] = c;
}

void matrix_set_z_rotation(float *m, float rad)
{
	float c = cosf(rad);
	float s = sinf(rad);

	matrix_identity4x4(m);

	m[0] = c;
	m[1] = s;
	m[4] = -s;
	m[5] = c;
}

void matrix_rotate_x(float *m, float rad)
{
	float mr[4*4], mt[4*4];
	matrix_set_y_rotation(mr, rad);
	matrix_mult4x4(m, mr, mt);
	matrix_copy(m, mt);
}


void matrix_rotate_y(float *m, float rad)
{
	float mr[4*4], mt[4*4];
	matrix_set_x_rotation(mr, rad);
	matrix_mult4x4(m, mr, mt);
	matrix_copy(m, mt);
}

void matrix_rotate_z(float *m, float rad)
{
	float mr[4*4], mt[4*4];
	matrix_set_z_rotation(mr, rad);
	matrix_mult4x4(m, mr, mt);
	matrix_copy(m, mt);
}

void matrix_set_xyz_translation(float *m, float x, float y, float z)
{
	matrix_identity4x4(m);

	m[12] = x;
	m[13] = y;
	m[14] = z;
}

void matrix_translate_xyz(float *m, float x, float y, float z)
{
	float mr[4*4], mt[4*4];
	matrix_set_xyz_translation(mr, x, y, z);
	matrix_mult4x4(m, mr, mt);
	matrix_copy(m, mt);
}

void matrix_set_scaling(float *m, float x_scale, float y_scale, float z_scale)
{
	matrix_identity4x4(m);
	m[0] = x_scale;
	m[5] = y_scale;
	m[10] = z_scale;
}

void matrix_swap_xy(float *m)
{
	float ms[4*4], mt[4*4];
	matrix_identity4x4(ms);

	ms[0] = 0.0f;
	ms[1] = 1.0f;
	ms[4] = 1.0f;
	ms[5] = 0.0f;

	matrix_mult4x4(m, ms, mt);
	matrix_copy(m, mt);
}

void matrix_init_orthographic(float *m, float left, float right, float bottom, float top, float near, float far)
{
	m[0x0] = 2.0f/(right-left);
	m[0x4] = 0.0f;
	m[0x8] = 0.0f;
	m[0xC] = -(right+left)/(right-left);

	m[0x1] = 0.0f;
	m[0x5] = 2.0f/(top-bottom);
	m[0x9] = 0.0f;
	m[0xD] = -(top+bottom)/(top-bottom);

	m[0x2] = 0.0f;
	m[0x6] = 0.0f;
	m[0xA] = -2.0f/(far-near);
	m[0xE] = (far+near)/(far-near);

	m[0x3] = 0.0f;
	m[0x7] = 0.0f;
	m[0xB] = 0.0f;
	m[0xF] = 1.0f;
}

void matrix_init_frustum(float *m, float left, float right, float bottom, float top, float near, float far)
{
	m[0x0] = (2.0f*near)/(right-left);
	m[0x4] = 0.0f;
	m[0x8] = (right+left)/(right-left);
	m[0xC] = 0.0f;

	m[0x1] = 0.0f;
	m[0x5] = (2.0f*near)/(top-bottom);
	m[0x9] = (top+bottom)/(top-bottom);
	m[0xD] = 0.0f;

	m[0x2] = 0.0f;
	m[0x6] = 0.0f;
	m[0xA] = -(far+near)/(far-near);
	m[0xE] = (-2.0f*far*near)/(far-near);

	m[0x3] = 0.0f;
	m[0x7] = 0.0f;
	m[0xB] = -1.0f;
	m[0xF] = 0.0f;
}

void matrix_init_perspective(float *m, float fov, float aspect, float near, float far)
{
	float half_height = near * tan(DEG_TO_RAD(fov) * 0.5f);
	float half_width = half_height * aspect;

	matrix_init_frustum(m, -half_width, half_width, -half_height, half_height, near, far);
}

int utf8_to_ucs2(const char *utf8, unsigned int *character)
{
	if (((utf8[0] & 0xF0) == 0xE0) && ((utf8[1] & 0xC0) == 0x80) && ((utf8[2] & 0xC0) == 0x80)) {
		*character = ((utf8[0] & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F);
		return 3;
	} else if (((utf8[0] & 0xE0) == 0xC0) && ((utf8[1] & 0xC0) == 0x80)) {
		*character = ((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F);
		return 2;
	} else {
		*character = utf8[0];
		return 1;
	}
}

int check_free_memory(unsigned int type, SceSize size)
{
	int ret;
	SceSize free;

	if (type < 4) {
		if (type == SCE_GXM_DEVICE_HEAP_ID_USER_NC)
			type = SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE;
		else
			type = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW;
	}

	switch (type) {
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW:
		if (system_mode_flag) {
			SceAppMgrBudgetInfo info;
			info.size = sizeof(SceAppMgrBudgetInfo);
			sceAppMgrGetBudgetInfo(&info);
			if (size > info.freeCdram)
				ret = 0;
			free = info.freeCdram;
		}
		else {
			SceKernelFreeMemorySizeInfo info;
			info.size = sizeof(SceKernelFreeMemorySizeInfo);
			sceKernelGetFreeMemorySize(&info);
			if (size > info.sizeCdram)
				ret = 0;
			free = info.sizeCdram;
		}
		break;
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE:
		if (system_mode_flag) {
			SceAppMgrBudgetInfo info;
			info.size = sizeof(SceAppMgrBudgetInfo);
			sceAppMgrGetBudgetInfo(&info);
			if (size > info.freeMain)
				ret = 0;
			free = info.freeMain;
		}
		else {
			SceKernelFreeMemorySizeInfo info;
			info.size = sizeof(SceKernelFreeMemorySizeInfo);
			sceKernelGetFreeMemorySize(&info);
			if (size > info.sizeMain)
				ret = 0;
			free = info.sizeMain;
		}
		break;
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW:
	case SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW:
		if (system_mode_flag) {
			SceAppMgrBudgetInfo info;
			info.size = sizeof(SceAppMgrBudgetInfo);
			sceAppMgrGetBudgetInfo(&info);
			if (size > info.freePhycont)
				ret = 0;
			free = info.freePhycont;
		}
		else {
			SceKernelFreeMemorySizeInfo info;
			info.size = sizeof(SceKernelFreeMemorySizeInfo);
			sceKernelGetFreeMemorySize(&info);
			if (size > info.sizePhycont)
				ret = 0;
			free = info.sizePhycont;
		}
		break;
	default:
		ret = 1;
		break;
	}

	ret = 1;

	if (!ret)
		SCE_DBG_LOG_WARNING("\n[UTILS] System is out of memory!\nRequested: 0x%X of type 0x%X\nFree: 0x%X", size, type, free);

	return ret;
}
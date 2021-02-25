#ifndef UTILS_H
#define UTILS_H

#include <gxm.h>
#include <kernel.h>
#include <fios2.h>

#define GXM_TEX_MAX_SIZE 4096

typedef struct vita2d_shared_mem_info {
	SceUID memBlockId;
	void *mappedBase;
	uint32_t offset;

	union {
		uint32_t attribs;		//Driver layer allocation
		uint32_t size_gxm;		//GXM layer allocation
	};

	uint32_t heapId;

	union {
		uint32_t size_driver;	//Driver layer allocation
		uint32_t unsused;		//GXM layer allocation
	};

} vita2d_shared_mem_info;

/* Misc utils */
#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))
#define	UNUSED(a)	(void)(a)
#define SCREEN_DPI	220
int check_free_memory(SceKernelMemBlockType type, SceSize size);

/* IO utils */
int readFile(const char *fileName, unsigned char *pBuffer, SceSize bufSize);
int readFileFIOS2(char *fileName, unsigned char *pBuffer, SceSize bufSize);

/* Font utils */
int utf8_to_ucs2(const char *utf8, unsigned int *character);

/* Math utils */

#define _PI_OVER_180 0.0174532925199432957692369076849f
#define _180_OVER_PI 57.2957795130823208767981548141f

#define DEG_TO_RAD(x) (x * _PI_OVER_180)
#define RAD_TO_DEG(x) (x * _180_OVER_PI)

void matrix_copy(float *dst, const float *src);
void matrix_identity4x4(float *m);
void matrix_mult4x4(const float *src1, const float *src2, float *dst);
void matrix_set_x_rotation(float *m, float rad);
void matrix_set_y_rotation(float *m, float rad);
void matrix_set_z_rotation(float *m, float rad);
void matrix_rotate_x(float *m, float rad);
void matrix_rotate_y(float *m, float rad);
void matrix_rotate_z(float *m, float rad);
void matrix_set_xyz_translation(float *m, float x, float y, float z);
void matrix_translate_xyz(float *m, float x, float y, float z);
void matrix_set_scaling(float *m, float x_scale, float y_scale, float z_scale);
void matrix_swap_xy(float *m);
void matrix_init_orthographic(float *m, float left, float right, float bottom, float top, float near, float far);
void matrix_init_frustum(float *m, float left, float right, float bottom, float top, float near, float far);
void matrix_init_perspective(float *m, float fov, float aspect, float near, float far);

/* Text utils */

#endif

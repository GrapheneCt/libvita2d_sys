#include <arm_neon.h>
#include <kernel.h>
#include <kernel/dmacmgr.h>
#include <libsysmodule.h>
#include <gxm.h>
#include <jpegarm.h>
#include <libdbg.h>
#include <fios2.h>
#include "vita2d_sys.h"

#include "utils.h"
#include "heap.h"
#include "pvr.h"

#define likely(x)	__builtin_expect(!!(x), 1)
#define unlikely(x)	__builtin_expect(!!(x), 0)
#define CSC_FIX(x)	((int)(x * 1024 + 0.5))

extern void* vita2d_heap_internal;
extern int system_mode_flag;
static int usePhyCont = 0;

static int decoder_initialized = 0, decoder_arm_initialized = 0;

typedef struct {
	SceUID		bufferMemBlock;
	void	   *pBuffer;
	SceSize		streamBufSize;
	SceSize		decodeBufSize;
	SceSize		coefBufSize;
} JpegDecCtrl;

typedef struct {
	int			validWidth;
	int			validHeight;
	int			pitchWidth;
	int			pitchHeight;
} FrameInfo;

/* CSC coefficients for ITU-R BT.601 full-range */
static const int16x4_t s_cscCoef[] = {
	/* y_offset, uv_lvs, y0, alpha */
	{ 0, -128, CSC_FIX(1.000), 255 },
	/* v0, u1, v1, u2 */
	{ CSC_FIX(1.402), -CSC_FIX(0.344), -CSC_FIX(0.714), CSC_FIX(1.772) }
};

/* chroma selector for bilinear upsampling */
static const uint8x8_t s_selBlChroma[] = {
	{  2, 32,  2, 32,  3, 32,  3, 32 }, // x3 - current samples
	{  2, 32,  3, 32,  2, 32,  4, 32 }, // x1 - left edge
	{  1, 32,  3, 32,  2, 32,  4, 32 }, //      middle
	{  1, 32,  3, 32,  2, 32,  3, 32 }  //      right edge
};

__attribute__((noinline))
static void yuv422ToRgba8888(
	uint8_t * __restrict__ pRGBA,
	const uint8_t * __restrict__ pY,
	const uint8_t * __restrict__ pU,
	const uint8_t * __restrict__ pV,
	unsigned int width, unsigned int height,
	unsigned int chromaPitchDiff, unsigned int pitchDiff
)
{
	int16x8_t sq16_temp;
	int16x4_t y0, u, v, next_y0, next_u, next_v;
	int32x4_t y1;
	uint16x4_t y_offset;
	int16x4_t uv_lvs, y_coef, uv_coef;
	int16x4_t r0, g0, b0, r1, g1, b1;
	uint8x8x4_t rgba;
	uint8x8_t u8_temp0, u8_temp1, u8_temp2;
	int16x4_t s16_temp0, s16_temp1;
	uint8x8_t selChromaMain, selChromaSubL, selChromaSubR;
	int x, matrix = 0;

	pitchDiff <<= 2;
	y_offset = (uint16x4_t)vdup_lane_s16(s_cscCoef[matrix], 0);
	uv_lvs = vdup_lane_s16(s_cscCoef[matrix], 1);
	y_coef = vdup_lane_s16(s_cscCoef[matrix], 2);
	rgba.val[3] = vdup_lane_u8((uint8x8_t)s_cscCoef[matrix], 6);
	uv_coef = s_cscCoef[matrix + 1];

	pU -= 2;
	pV -= 2;

	selChromaMain = s_selBlChroma[0];
	do {
		x = width;
		selChromaSubL = s_selBlChroma[1];
		do {
			// load
			u8_temp2 = vld1_u8(pY);
			u8_temp0 = vld1_u8(pU);
			u8_temp1 = vld1_u8(pV);
			pY += 8;
			pU += 4;
			pV += 4;
			sq16_temp = (int16x8_t)vmovl_u8(u8_temp2);
			y0 = vget_low_s16(sq16_temp);
			next_y0 = vget_high_s16(sq16_temp);
			y0 = (int16x4_t)vqsub_u16((uint16x4_t)y0, y_offset);
			next_y0 = (int16x4_t)vqsub_u16((uint16x4_t)next_y0, y_offset);
			if (likely(x > 8)) {
				selChromaSubR = s_selBlChroma[2];
			}
			else {
				selChromaSubR = s_selBlChroma[3];
			}
			// upsample U
			s16_temp0 = (int16x4_t)vtbl1_u8(u8_temp0, selChromaMain);
			s16_temp1 = (int16x4_t)vtbl1_u8(u8_temp0, selChromaSubL);
			s16_temp1 = vmla_n_s16(s16_temp1, s16_temp0, 3);
			u = vrshr_n_s16(s16_temp1, 2);
			u8_temp0 = (uint8x8_t)vshr_n_u64((uint64x1_t)u8_temp0, 16);
			s16_temp0 = (int16x4_t)vtbl1_u8(u8_temp0, selChromaMain);
			s16_temp1 = (int16x4_t)vtbl1_u8(u8_temp0, selChromaSubR);
			s16_temp1 = vmla_n_s16(s16_temp1, s16_temp0, 3);
			next_u = vrshr_n_s16(s16_temp1, 2);
			// upsample V
			s16_temp0 = (int16x4_t)vtbl1_u8(u8_temp1, selChromaMain);
			s16_temp1 = (int16x4_t)vtbl1_u8(u8_temp1, selChromaSubL);
			s16_temp1 = vmla_n_s16(s16_temp1, s16_temp0, 3);
			v = vrshr_n_s16(s16_temp1, 2);
			u8_temp1 = (uint8x8_t)vshr_n_u64((uint64x1_t)u8_temp1, 16);
			s16_temp0 = (int16x4_t)vtbl1_u8(u8_temp1, selChromaMain);
			s16_temp1 = (int16x4_t)vtbl1_u8(u8_temp1, selChromaSubR);
			s16_temp1 = vmla_n_s16(s16_temp1, s16_temp0, 3);
			next_v = vrshr_n_s16(s16_temp1, 2);
			// CSC
			y1 = vmull_s16(y0, y_coef);
			u = vadd_s16(u, uv_lvs);
			v = vadd_s16(v, uv_lvs);
			r0 = vqrshrn_n_s32(vmlal_lane_s16(y1, v, uv_coef, 0), 10);
			g0 = vqrshrn_n_s32(vmlal_lane_s16(vmlal_lane_s16(y1, u, uv_coef, 1), v, uv_coef, 2), 10);
			b0 = vqrshrn_n_s32(vmlal_lane_s16(y1, u, uv_coef, 3), 10);
			y1 = vmull_s16(next_y0, y_coef);
			u = vadd_s16(next_u, uv_lvs);
			v = vadd_s16(next_v, uv_lvs);
			r1 = vqrshrn_n_s32(vmlal_lane_s16(y1, v, uv_coef, 0), 10);
			g1 = vqrshrn_n_s32(vmlal_lane_s16(vmlal_lane_s16(y1, u, uv_coef, 1), v, uv_coef, 2), 10);
			b1 = vqrshrn_n_s32(vmlal_lane_s16(y1, u, uv_coef, 3), 10);
			// store
			rgba.val[0] = vqmovun_s16(vcombine_s16(r0, r1));
			rgba.val[1] = vqmovun_s16(vcombine_s16(g0, g1));
			rgba.val[2] = vqmovun_s16(vcombine_s16(b0, b1));
			vst4_u8(pRGBA, rgba);
			x -= 8;
			pRGBA += 4 * 8;
			selChromaSubL = s_selBlChroma[2];
		} while (x > 0);
		pU += chromaPitchDiff;
		pV += chromaPitchDiff;
		pRGBA += pitchDiff;
	} while (--height);

	return;
}

__attribute__((noinline))
static void yuv420ToRgba8888(
	uint8_t * __restrict__ pRGBA,
	const uint8_t * __restrict__ pY,
	const uint8_t * __restrict__ pU,
	const uint8_t * __restrict__ pV,
	unsigned int width, unsigned int height,
	unsigned int chromaPitchDiff, unsigned int pitchDiff
)
{
	int16x8_t sq16_temp;
	int16x4_t y0, u, v, next_y0, next_u, next_v;
	int32x4_t y1;
	uint16x4_t y_offset;
	int16x4_t uv_lvs, y_coef, uv_coef;
	int16x4_t r0, g0, b0, r1, g1, b1;
	uint8x8x4_t rgba;
	const uint8_t * __restrict__ pLastU;
	const uint8_t * __restrict__ pLastV;
	uint8x8_t u8_temp0, u8_temp1, u8_temp2, u8_temp3, u8_temp4;
	int16x4_t s16_temp0, s16_temp1, s16_temp2, s16_temp3;
	uint8x8_t selChromaMain, selChromaSubL, selChromaSubR;
	intptr_t chromaLineSize, chroma2ndLineOffset;
	int x, matrix = 0;

	pitchDiff <<= 2;
	y_offset = (uint16x4_t)vdup_lane_s16(s_cscCoef[matrix], 0);
	uv_lvs = vdup_lane_s16(s_cscCoef[matrix], 1);
	y_coef = vdup_lane_s16(s_cscCoef[matrix], 2);
	rgba.val[3] = vdup_lane_u8((uint8x8_t)s_cscCoef[matrix], 6);
	uv_coef = s_cscCoef[matrix + 1];

	pU -= 2;
	pV -= 2;

	chromaLineSize = (width >> 1) + chromaPitchDiff;
	chroma2ndLineOffset = 0;
	pLastU = pU;
	pLastV = pV;
	selChromaMain = s_selBlChroma[0];
	do {
		// load
		u8_temp4 = vld1_u8(pY);
		u8_temp0 = vld1_u8(pU);
		u8_temp1 = vld1_u8(pU + chroma2ndLineOffset);
		u8_temp2 = vld1_u8(pV);
		u8_temp3 = vld1_u8(pV + chroma2ndLineOffset);
		pY += 8;
		pU += 4;
		pV += 4;
		x = width;
		selChromaSubL = s_selBlChroma[1];
		if (likely(x > 8)) {
			selChromaSubR = s_selBlChroma[2];
		}
		else {
			selChromaSubR = s_selBlChroma[3];
		}
		do {
			sq16_temp = (int16x8_t)vmovl_u8(u8_temp4);
			y0 = vget_low_s16(sq16_temp);
			next_y0 = vget_high_s16(sq16_temp);
			y0 = (int16x4_t)vqsub_u16((uint16x4_t)y0, y_offset);
			next_y0 = (int16x4_t)vqsub_u16((uint16x4_t)next_y0, y_offset);
			// upsample U
			s16_temp1 = (int16x4_t)vtbl1_u8(u8_temp0, selChromaSubL);
			s16_temp2 = (int16x4_t)vtbl1_u8(u8_temp1, selChromaMain);
			s16_temp0 = (int16x4_t)vtbl1_u8(u8_temp0, selChromaMain);
			s16_temp3 = (int16x4_t)vtbl1_u8(u8_temp1, selChromaSubL);
			u8_temp0 = (uint8x8_t)vshr_n_u64((uint64x1_t)u8_temp0, 16);
			u8_temp1 = (uint8x8_t)vshr_n_u64((uint64x1_t)u8_temp1, 16);
			s16_temp1 = vadd_s16(s16_temp1, s16_temp2);
			s16_temp3 = vmla_n_s16(s16_temp3, s16_temp0, 9);
			s16_temp3 = vmla_n_s16(s16_temp3, s16_temp1, 3);
			u = vrshr_n_s16(s16_temp3, 4);
			s16_temp1 = (int16x4_t)vtbl1_u8(u8_temp0, selChromaSubR);
			s16_temp2 = (int16x4_t)vtbl1_u8(u8_temp1, selChromaMain);
			s16_temp0 = (int16x4_t)vtbl1_u8(u8_temp0, selChromaMain);
			s16_temp3 = (int16x4_t)vtbl1_u8(u8_temp1, selChromaSubR);
			s16_temp1 = vadd_s16(s16_temp1, s16_temp2);
			s16_temp3 = vmla_n_s16(s16_temp3, s16_temp0, 9);
			s16_temp3 = vmla_n_s16(s16_temp3, s16_temp1, 3);
			next_u = vrshr_n_s16(s16_temp3, 4);
			// upsample V
			s16_temp1 = (int16x4_t)vtbl1_u8(u8_temp2, selChromaSubL);
			s16_temp2 = (int16x4_t)vtbl1_u8(u8_temp3, selChromaMain);
			s16_temp0 = (int16x4_t)vtbl1_u8(u8_temp2, selChromaMain);
			s16_temp3 = (int16x4_t)vtbl1_u8(u8_temp3, selChromaSubL);
			u8_temp2 = (uint8x8_t)vshr_n_u64((uint64x1_t)u8_temp2, 16);
			u8_temp3 = (uint8x8_t)vshr_n_u64((uint64x1_t)u8_temp3, 16);
			s16_temp1 = vadd_s16(s16_temp1, s16_temp2);
			s16_temp3 = vmla_n_s16(s16_temp3, s16_temp0, 9);
			s16_temp3 = vmla_n_s16(s16_temp3, s16_temp1, 3);
			v = vrshr_n_s16(s16_temp3, 4);
			s16_temp1 = (int16x4_t)vtbl1_u8(u8_temp2, selChromaSubR);
			s16_temp2 = (int16x4_t)vtbl1_u8(u8_temp3, selChromaMain);
			s16_temp0 = (int16x4_t)vtbl1_u8(u8_temp2, selChromaMain);
			s16_temp3 = (int16x4_t)vtbl1_u8(u8_temp3, selChromaSubR);
			s16_temp1 = vadd_s16(s16_temp1, s16_temp2);
			s16_temp3 = vmla_n_s16(s16_temp3, s16_temp0, 9);
			s16_temp3 = vmla_n_s16(s16_temp3, s16_temp1, 3);
			next_v = vrshr_n_s16(s16_temp3, 4);
			selChromaSubL = selChromaSubR;
			// CSC
			y1 = vmull_s16(y0, y_coef);
			u = vadd_s16(u, uv_lvs);
			v = vadd_s16(v, uv_lvs);
			r0 = vqrshrn_n_s32(vmlal_lane_s16(y1, v, uv_coef, 0), 10);
			g0 = vqrshrn_n_s32(vmlal_lane_s16(vmlal_lane_s16(y1, u, uv_coef, 1), v, uv_coef, 2), 10);
			b0 = vqrshrn_n_s32(vmlal_lane_s16(y1, u, uv_coef, 3), 10);
			y1 = vmull_s16(next_y0, y_coef);
			u = vadd_s16(next_u, uv_lvs);
			v = vadd_s16(next_v, uv_lvs);
			r1 = vqrshrn_n_s32(vmlal_lane_s16(y1, v, uv_coef, 0), 10);
			g1 = vqrshrn_n_s32(vmlal_lane_s16(vmlal_lane_s16(y1, u, uv_coef, 1), v, uv_coef, 2), 10);
			// store
			rgba.val[0] = vqmovun_s16(vcombine_s16(r0, r1));
			b1 = vqrshrn_n_s32(vmlal_lane_s16(y1, u, uv_coef, 3), 10);
			rgba.val[1] = vqmovun_s16(vcombine_s16(g0, g1));
			rgba.val[2] = vqmovun_s16(vcombine_s16(b0, b1));
			// load
			if (likely(x > 8)) {
				u8_temp4 = vld1_u8(pY);
				u8_temp0 = vld1_u8(pU);
				u8_temp1 = vld1_u8(pU + chroma2ndLineOffset);
				u8_temp2 = vld1_u8(pV);
				u8_temp3 = vld1_u8(pV + chroma2ndLineOffset);
				pY += 8;
				pU += 4;
				pV += 4;
			}
			vst4_u8(pRGBA, rgba);
			x -= 8;
			pRGBA += 4 * 8;
			if (unlikely(x <= 8)) {
				selChromaSubR = s_selBlChroma[3];
			}
		} while (x > 0);
		if (pLastU != NULL) {
			// restore U/V ptr to head of current line
			chroma2ndLineOffset = (height > 2) ? chromaLineSize : 0;
			pU = pLastU;
			pV = pLastV;
			pLastU = NULL;
		}
		else {
			// forward U/V ptr to next line
			chroma2ndLineOffset = -chromaLineSize;
			pU += chromaPitchDiff;
			pV += chromaPitchDiff;
			pLastU = pU;
			pLastV = pV;
		}
		pRGBA += pitchDiff;
	} while (--height);

	return;
}

int csc(void *pRGBA, const unsigned char *pYCbCr, int xysize, int iFrameWidth,
	int colorOption, int sampling)
{
	unsigned int width, height, chromaPitchDiff, pitchDiff, ySize, cSize;

	if (pRGBA == NULL || pYCbCr == NULL ||
		(((uintptr_t)pRGBA | (uintptr_t)pYCbCr) & 3u)) {
		return SCE_JPEG_ERROR_INVALID_POINTER;
	}
	if (colorOption != SCE_JPEG_PIXEL_RGBA8888) {
		return SCE_JPEG_ERROR_UNSUPPORT_COLORSPACE;
	}

	width = (unsigned int)xysize >> 16;
	height = xysize & 0xFFFF;
	if (width == 0 || height == 0 || (width & 7u) || (unsigned int)iFrameWidth < width) {
		return SCE_JPEG_ERROR_UNSUPPORT_IMAGE_SIZE;
	}

	chromaPitchDiff = (width & 16) >> 1;
	pitchDiff = iFrameWidth - width;
	ySize = width * height;
	switch (sampling & 0xFFFF) {
	case SCE_JPEG_CS_H2V1:
		cSize = ALIGN(width, 32) * height >> 1;
		yuv422ToRgba8888(
			(uint8_t * __restrict__)pRGBA,
			(const uint8_t * __restrict__)pYCbCr,
			(const uint8_t * __restrict__)(pYCbCr + ySize),
			(const uint8_t * __restrict__)(pYCbCr + ySize + cSize),
			width, height, chromaPitchDiff, pitchDiff);
		break;
	case SCE_JPEG_CS_H2V2:
		if ((height & 1u)) {
			return SCE_JPEG_ERROR_UNSUPPORT_IMAGE_SIZE;
		}
		cSize = ALIGN(width, 32) * height >> 2;
		yuv420ToRgba8888(
			(uint8_t * __restrict__)pRGBA,
			(const uint8_t * __restrict__)pYCbCr,
			(const uint8_t * __restrict__)(pYCbCr + ySize),
			(const uint8_t * __restrict__)(pYCbCr + ySize + cSize),
			width, height, chromaPitchDiff, pitchDiff);
		break;
	default:
		return SCE_JPEG_ERROR_UNSUPPORT_SAMPLING;
	}

	return 0;
}

int vita2d_JPEG_decoder_initialize(int usePhyContMemory)
{
	if (!decoder_initialized) {
		SceJpegMJpegInitParam initParam;

		/*E Initialize JPEG decoder. */
		initParam.size = sizeof(SceJpegMJpegInitParam);
		initParam.maxSplitDecoder = 0;
		if (usePhyContMemory || system_mode_flag) {
			initParam.option = SCE_JPEG_MJPEG_INIT_OPTION_LPDDR2_MEMORY;
			usePhyCont = 1;
		}
		else
			usePhyCont = 0;

		decoder_initialized = 1;

		return sceJpegInitMJpegWithParam(&initParam);
	}
	else {
		SCE_DBG_LOG_WARNING("[JPEG] Decoder already initialized!");
		return 0;
	}
}

int vita2d_JPEG_decoder_finish(void)
{
	if (decoder_initialized) {
		decoder_initialized = 0;
		return sceJpegFinishMJpeg();
	}
	else {
		SCE_DBG_LOG_WARNING("[JPEG] Decoder not initialized!");
		return 0;
	}
}

vita2d_texture *vita2d_load_JPEG_file(char *filename, vita2d_io_type io_type, int useDownScale, int downScalerHeight, int downScalerWidth)
{
	int ret;
	int pixelCount;
	JpegDecCtrl	decCtrl;
	SceSize totalBufSize;
	SceKernelMemBlockType memBlockType;
	SceJpegOutputInfo outputInfo;
	unsigned char *pJpeg;
	SceSize isize;
	unsigned char *pYCbCr;
	void *pCoefBuffer;
	int decodeMode = SCE_JPEG_MJPEG_WITH_DHT;
	int validWidth, validHeight;

	if (!decoder_initialized) {
		SCE_DBG_LOG_WARNING("[JPEG] Decoder not initialized!");
		return NULL;
	}

	/*E Determine memory types. */
	memBlockType = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW;
	SceSize memBlockAlign = 256 * 1024;
	if (usePhyCont) {
		memBlockAlign = 1024 * 1024;
		memBlockType = SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW;
	}

	/*E Allocate stream buffer. */
	if (io_type) {
		SceFiosStat fios_stat;
		sceClibMemset(&fios_stat, 0, sizeof(SceFiosStat));
		sceFiosStatSync(NULL, filename, &fios_stat);
		isize = (SceSize)fios_stat.fileSize;
	}
	else {
		SceIoStat stat;
		sceIoGetstat(filename, &stat);
		isize = (SceSize)stat.st_size;
	}

	decCtrl.streamBufSize = ALIGN(isize, memBlockAlign);

	SceUID streamBufMemblock = sceKernelAllocMemBlock("jpegdecStreamBuffer",
		memBlockType, decCtrl.streamBufSize, NULL);

	if (streamBufMemblock < 0)
		return NULL;

	sceKernelGetMemBlockBase(streamBufMemblock, (void **)&pJpeg);

	/*E Read JPEG file to buffer. */
	if (io_type)
		readFileFIOS2(filename, pJpeg, isize);
	else
		readFile(filename, pJpeg, isize);

	/*E Get JPEG output information. */
	ret = sceJpegGetOutputInfo(pJpeg, isize,
		SCE_JPEG_NO_CSC_OUTPUT, decodeMode, &outputInfo);
	if (ret == SCE_JPEG_ERROR_UNSUPPORT_SAMPLING &&
		outputInfo.colorSpace == (SCE_JPEG_CS_YCBCR | SCE_JPEG_CS_H1V1)) {
		/* Set SCE_JPEG_MJPEG_ANY_SAMPLING for decodeMode and retry sceJpegGetOutputInfo(),
		   if the JPEG's color space is YCbCr 4:4:4. */
		decodeMode = SCE_JPEG_MJPEG_ANY_SAMPLING;
		ret = sceJpegGetOutputInfo(pJpeg, isize,
			SCE_JPEG_NO_CSC_OUTPUT, decodeMode, &outputInfo);
	}

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[JPEG] sceJpegGetOutputInfo(): 0x%X", ret);
		goto error_free_file_hw_in_buf;
	}

	/*E Allocate decoder memory. */
	totalBufSize = ALIGN(outputInfo.outputBufferSize + outputInfo.coefBufferSize, memBlockAlign);

	if (!check_free_memory(memBlockType, totalBufSize))
		goto error_free_file_hw_in_buf;

	decCtrl.bufferMemBlock = sceKernelAllocMemBlock("jpegdecMainBuffer",
		memBlockType, totalBufSize, NULL);

	if (decCtrl.bufferMemBlock < 0)
		goto error_free_file_hw_in_buf;

	sceKernelGetMemBlockBase(decCtrl.bufferMemBlock, (void **)&pYCbCr);

	decCtrl.decodeBufSize = outputInfo.outputBufferSize;
	decCtrl.coefBufSize = outputInfo.coefBufferSize;

	/*E Calculate downscale ratio. */
	if (useDownScale)
	{
		float downScaleWidth, downScaleHeight, downScale;
		int downScaleDiv;

		downScaleWidth = (float)outputInfo.pitch[0].x / downScalerWidth;
		downScaleHeight = (float)outputInfo.pitch[0].y / downScalerHeight;
		downScale = (downScaleWidth >= downScaleHeight) ? downScaleWidth : downScaleHeight;
		if (downScale <= 1.f) {
			/*E Downscale is not needed. */
		}
		else if (downScale <= 2.f) {
			decodeMode |= SCE_JPEG_MJPEG_DOWNSCALE_1_2;
		}
		else if (downScale <= 4.f) {
			decodeMode |= SCE_JPEG_MJPEG_DOWNSCALE_1_4;
		}
		else if (downScale <= 8.f) {
			decodeMode |= SCE_JPEG_MJPEG_DOWNSCALE_1_8;
		}
		downScaleDiv = (decodeMode >> 3) & 0xe;
		if (downScaleDiv) {
			validWidth = (outputInfo.imageWidth + downScaleDiv - 1) / downScaleDiv;
			validHeight = (outputInfo.imageHeight + downScaleDiv - 1) / downScaleDiv;
		}
		else {
			validWidth = outputInfo.imageWidth;
			validHeight = outputInfo.imageHeight;
		}
	}
	else
	{
		validWidth = outputInfo.imageWidth;
		validHeight = outputInfo.imageHeight;
	}

	/*E Set output buffer and quantized coefficients buffer. */
	if (outputInfo.coefBufferSize > 0 && decCtrl.coefBufSize > 0) {
		pCoefBuffer = (void*)(pYCbCr + decCtrl.decodeBufSize);
	}
	else {
		pCoefBuffer = NULL;
	}

	/*E Decode JPEG stream */
	pixelCount = sceJpegDecodeMJpegYCbCr(
		pJpeg, isize,
		pYCbCr, decCtrl.decodeBufSize, decodeMode,
		pCoefBuffer, decCtrl.coefBufSize);

	/*E Free file buffer */
	if (streamBufMemblock >= 0) {
		sceKernelFreeMemBlock(streamBufMemblock);
	}

	if (pixelCount < 0) {
		SCE_DBG_LOG_ERROR("[JPEG] sceJpegDecodeMJpegYCbCr(): 0x%X", pixelCount);
		goto error_free_file_hw_both_buf;
	}

	FrameInfo pFrameInfo;

	pFrameInfo.pitchWidth = pixelCount >> 16;
	pFrameInfo.pitchHeight = pixelCount & 0xFFFF;
	pFrameInfo.validWidth = validWidth;
	pFrameInfo.validHeight = validHeight;

	if (pFrameInfo.pitchWidth > GXM_TEX_MAX_SIZE || pFrameInfo.pitchHeight > GXM_TEX_MAX_SIZE) {
		SCE_DBG_LOG_ERROR("[JPEG] %s texture is too big!", filename);
		goto error_free_file_hw_both_buf;
	}

	unsigned int size = ALIGN(4 * 1024 * pFrameInfo.pitchHeight, memBlockAlign);

	SceUID tex_data_uid = sceKernelAllocMemBlock("gpu_mem", memBlockType, size, NULL);

	if (tex_data_uid < 0)
		goto error_free_file_hw_both_buf;

	void* texture_data;

	sceKernelGetMemBlockBase(tex_data_uid, &texture_data);

	ret = sceGxmMapMemory(texture_data, size, SCE_GXM_MEMORY_ATTRIB_READ);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[JPEG] sceGxmMapMemory(): 0x%X", ret);
		goto error_free_file_hw_all_buf;
	}

	/* Clear the texture */
	if (size < 128 * 1024)
		sceClibMemset(texture_data, 0, size);
	else
		sceDmacMemset(texture_data, 0, size);

	//E CSC (YCbCr -> RGBA) 
	if ((decodeMode & 3) == SCE_JPEG_MJPEG_WITH_DHT) {
		if (pFrameInfo.pitchWidth >= 64 && pFrameInfo.pitchHeight >= 64) {
			//E YCbCr 4:2:0 or YCbCr 4:2:2 (fast, processed on dedicated hardware) 
			ret = sceJpegMJpegCsc(
				texture_data, pYCbCr, pixelCount, pFrameInfo.pitchWidth,
				SCE_JPEG_PIXEL_RGBA8888, outputInfo.colorSpace & 0xFFFF);
		}
		else {
			//E YCbCr 4:2:0 or YCbCr 4:2:2, image width < 64 or height < 64
				//(slow, processed on the CPU) 
			ret = csc(
				texture_data, pYCbCr, pixelCount, pFrameInfo.pitchWidth,
				SCE_JPEG_PIXEL_RGBA8888, outputInfo.colorSpace & 0xFFFF);
		}
	}
	else {
		//E YCbCr 4:4:4 (slow, processed on the codec engine) 
		ret = sceJpegCsc(
			texture_data, pYCbCr, pixelCount, pFrameInfo.pitchWidth,
			SCE_JPEG_PIXEL_RGBA8888, outputInfo.colorSpace & 0xFFFF);
	}

	vita2d_texture *texture = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(vita2d_texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[JPEG] heap_alloc_heap_memory() returned NULL");
		goto error_free_file_hw_all_buf;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	texture->data_mem = (SceGxmDeviceMemInfo *)PVRSRVAllocUserModeMem(sizeof(SceGxmDeviceMemInfo));

	texture->data_mem->memBlockId = tex_data_uid;
	texture->data_mem->mappedBase = texture_data;
	texture->data_mem->offset = 0;
	texture->data_mem->size = size;
	texture->data_mem->heapId = SCE_GXM_DEVICE_HEAP_ID_CDRAM;

	/* Create the gxm texture */
	sceGxmTextureInitLinear(
		&texture->gxm_tex,
		texture_data,
		SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,
		pFrameInfo.pitchWidth,
		pFrameInfo.pitchHeight,
		0);

	if (decCtrl.bufferMemBlock >= 0) {
		sceKernelFreeMemBlock(decCtrl.bufferMemBlock);
	}

	return texture;

error_free_file_hw_all_buf:

	sceGxmFreeDeviceMemLinux(texture->data_mem);

error_free_file_hw_both_buf:

	if (decCtrl.bufferMemBlock >= 0) {
		sceKernelFreeMemBlock(decCtrl.bufferMemBlock);
	}

error_free_file_hw_in_buf:

	/*E Free file buffer */
	if (streamBufMemblock >= 0) {
		sceKernelFreeMemBlock(streamBufMemblock);
	}

	return NULL;
}

vita2d_texture *vita2d_load_JPEG_buffer(const void *buffer, unsigned long buffer_size, int useDownScale, int downScalerHeight, int downScalerWidth)
{
	int ret;
	int pixelCount;
	JpegDecCtrl	decCtrl;
	SceSize totalBufSize;
	SceKernelMemBlockType memBlockType;
	SceJpegOutputInfo outputInfo;
	unsigned char *pJpeg;
	SceSize isize;
	unsigned char *pYCbCr;
	void *pCoefBuffer;
	int decodeMode = SCE_JPEG_MJPEG_WITH_DHT;
	int validWidth, validHeight;

	if (!decoder_initialized) {
		SCE_DBG_LOG_WARNING("[JPEG] Decoder not initialized!");
		return NULL;
	}

	/*E Determine memory types. */
	memBlockType = SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW;
	SceSize memBlockAlign = 256 * 1024;
	if (usePhyCont) {
		memBlockAlign = 1024 * 1024;
		memBlockType = SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW;
	}

	/*E Allocate stream buffer. */
	isize = buffer_size;

	decCtrl.streamBufSize = ALIGN(isize, memBlockAlign);

	SceUID streamBufMemblock = sceKernelAllocMemBlock("jpegdecStreamBuffer",
		memBlockType, decCtrl.streamBufSize, NULL);

	if (streamBufMemblock < 0)
		return NULL;

	sceKernelGetMemBlockBase(streamBufMemblock, (void **)&pJpeg);

	/*E Read JPEG buffer to buffer. */
	sceClibMemcpy(pJpeg, buffer, isize);

	/*E Get JPEG output information. */
	ret = sceJpegGetOutputInfo(pJpeg, isize,
		SCE_JPEG_NO_CSC_OUTPUT, decodeMode, &outputInfo);
	if (ret == SCE_JPEG_ERROR_UNSUPPORT_SAMPLING &&
		outputInfo.colorSpace == (SCE_JPEG_CS_YCBCR | SCE_JPEG_CS_H1V1)) {
		/* Set SCE_JPEG_MJPEG_ANY_SAMPLING for decodeMode and retry sceJpegGetOutputInfo(),
		   if the JPEG's color space is YCbCr 4:4:4. */
		decodeMode = SCE_JPEG_MJPEG_ANY_SAMPLING;
		ret = sceJpegGetOutputInfo(pJpeg, isize,
			SCE_JPEG_NO_CSC_OUTPUT, decodeMode, &outputInfo);
	}

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[JPEG] sceJpegGetOutputInfo(): 0x%X", ret);
		goto error_free_buf_hw_in_buf;
	}

	/*E Allocate decoder memory. */
	totalBufSize = ALIGN(outputInfo.outputBufferSize + outputInfo.coefBufferSize, memBlockAlign);

	if (!check_free_memory(memBlockType, totalBufSize))
		goto error_free_buf_hw_in_buf;

	decCtrl.bufferMemBlock = sceKernelAllocMemBlock("jpegdecMainBuffer",
		memBlockType, totalBufSize, NULL);

	if (decCtrl.bufferMemBlock < 0)
		goto error_free_buf_hw_in_buf;

	sceKernelGetMemBlockBase(decCtrl.bufferMemBlock, (void **)&pYCbCr);

	decCtrl.decodeBufSize = outputInfo.outputBufferSize;
	decCtrl.coefBufSize = outputInfo.coefBufferSize;

	/*E Calculate downscale ratio. */
	if (useDownScale)
	{
		float downScaleWidth, downScaleHeight, downScale;
		int downScaleDiv;

		downScaleWidth = (float)outputInfo.pitch[0].x / downScalerWidth;
		downScaleHeight = (float)outputInfo.pitch[0].y / downScalerHeight;
		downScale = (downScaleWidth >= downScaleHeight) ? downScaleWidth : downScaleHeight;
		if (downScale <= 1.f) {
			/*E Downscale is not needed. */
		}
		else if (downScale <= 2.f) {
			decodeMode |= SCE_JPEG_MJPEG_DOWNSCALE_1_2;
		}
		else if (downScale <= 4.f) {
			decodeMode |= SCE_JPEG_MJPEG_DOWNSCALE_1_4;
		}
		else if (downScale <= 8.f) {
			decodeMode |= SCE_JPEG_MJPEG_DOWNSCALE_1_8;
		}
		downScaleDiv = (decodeMode >> 3) & 0xe;
		if (downScaleDiv) {
			validWidth = (outputInfo.imageWidth + downScaleDiv - 1) / downScaleDiv;
			validHeight = (outputInfo.imageHeight + downScaleDiv - 1) / downScaleDiv;
		}
		else {
			validWidth = outputInfo.imageWidth;
			validHeight = outputInfo.imageHeight;
		}
	}
	else
	{
		validWidth = outputInfo.imageWidth;
		validHeight = outputInfo.imageHeight;
	}

	/*E Set output buffer and quantized coefficients buffer. */
	if (outputInfo.coefBufferSize > 0 && decCtrl.coefBufSize > 0) {
		pCoefBuffer = (void*)(pYCbCr + decCtrl.decodeBufSize);
	}
	else {
		pCoefBuffer = NULL;
	}

	/*E Decode JPEG stream */
	pixelCount = sceJpegDecodeMJpegYCbCr(
		pJpeg, isize,
		pYCbCr, decCtrl.decodeBufSize, decodeMode,
		pCoefBuffer, decCtrl.coefBufSize);

	/*E Free file buffer */
	if (streamBufMemblock >= 0) {
		sceKernelFreeMemBlock(streamBufMemblock);
	}

	if (pixelCount < 0) {
		SCE_DBG_LOG_ERROR("[JPEG] sceJpegDecodeMJpegYCbCr(): 0x%X", pixelCount);
		goto error_free_buf_hw_both_buf;
	}

	FrameInfo pFrameInfo;

	pFrameInfo.pitchWidth = pixelCount >> 16;
	pFrameInfo.pitchHeight = pixelCount & 0xFFFF;
	pFrameInfo.validWidth = validWidth;
	pFrameInfo.validHeight = validHeight;

	if (pFrameInfo.pitchWidth > GXM_TEX_MAX_SIZE || pFrameInfo.pitchHeight > GXM_TEX_MAX_SIZE)
		goto error_free_buf_hw_both_buf;

	unsigned int size = ALIGN(4 * 1024 * pFrameInfo.pitchHeight, memBlockAlign);

	SceUID tex_data_uid = sceKernelAllocMemBlock("gpu_mem", memBlockType, size, NULL);

	if (tex_data_uid < 0)
		goto error_free_buf_hw_both_buf;

	void* texture_data;

	sceKernelGetMemBlockBase(tex_data_uid, &texture_data);

	ret = sceGxmMapMemory(texture_data, size, SCE_GXM_MEMORY_ATTRIB_READ);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[JPEG] sceGxmMapMemory(): 0x%X", ret);
		goto error_free_buf_hw_all_buf;
	}

	/* Clear the texture */
	if (size < 128 * 1024)
		sceClibMemset(texture_data, 0, size);
	else
		sceDmacMemset(texture_data, 0, size);

	//E CSC (YCbCr -> RGBA) 
	if ((decodeMode & 3) == SCE_JPEG_MJPEG_WITH_DHT) {
		if (pFrameInfo.pitchWidth >= 64 && pFrameInfo.pitchHeight >= 64) {
			//E YCbCr 4:2:0 or YCbCr 4:2:2 (fast, processed on dedicated hardware) 
			ret = sceJpegMJpegCsc(
				texture_data, pYCbCr, pixelCount, pFrameInfo.pitchWidth,
				SCE_JPEG_PIXEL_RGBA8888, outputInfo.colorSpace & 0xFFFF);
		}
		else {
			//E YCbCr 4:2:0 or YCbCr 4:2:2, image width < 64 or height < 64
				//(slow, processed on the CPU) 
			ret = csc(
				texture_data, pYCbCr, pixelCount, pFrameInfo.pitchWidth,
				SCE_JPEG_PIXEL_RGBA8888, outputInfo.colorSpace & 0xFFFF);
		}
	}
	else {
		//E YCbCr 4:4:4 (slow, processed on the codec engine) 
		ret = sceJpegCsc(
			texture_data, pYCbCr, pixelCount, pFrameInfo.pitchWidth,
			SCE_JPEG_PIXEL_RGBA8888, outputInfo.colorSpace & 0xFFFF);
	}

	vita2d_texture *texture = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(vita2d_texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[JPEG] heap_alloc_heap_memory() returned NULL");
		goto error_free_buf_hw_all_buf;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	texture->data_mem = (SceGxmDeviceMemInfo *)PVRSRVAllocUserModeMem(sizeof(SceGxmDeviceMemInfo));

	texture->data_mem->memBlockId = tex_data_uid;
	texture->data_mem->mappedBase = texture_data;
	texture->data_mem->offset = 0;
	texture->data_mem->size = size;
	texture->data_mem->heapId = SCE_GXM_DEVICE_HEAP_ID_CDRAM;

	/* Create the gxm texture */
	sceGxmTextureInitLinear(
		&texture->gxm_tex,
		texture_data,
		SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,
		pFrameInfo.pitchWidth,
		pFrameInfo.pitchHeight,
		0);

	if (decCtrl.bufferMemBlock >= 0) {
		sceKernelFreeMemBlock(decCtrl.bufferMemBlock);
	}

	return texture;

error_free_buf_hw_all_buf:

	sceGxmFreeDeviceMemLinux(texture->data_mem);

error_free_buf_hw_both_buf:

	if (decCtrl.bufferMemBlock >= 0) {
		sceKernelFreeMemBlock(decCtrl.bufferMemBlock);
	}

error_free_buf_hw_in_buf:

	/*E Free file buffer */
	if (streamBufMemblock >= 0) {
		sceKernelFreeMemBlock(streamBufMemblock);
	}

	return NULL;
}

int vita2d_JPEG_ARM_decoder_initialize(void)
{
	if (!decoder_arm_initialized) {
		decoder_arm_initialized = 1;
		return sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_JPEG_ARM);
	}
	else {
		SCE_DBG_LOG_WARNING("[JPEG] ARM decoder already initialized!");
		return 0;
	}
}

int vita2d_JPEG_ARM_decoder_finish(void)
{
	if (decoder_arm_initialized) {
		decoder_arm_initialized = 0;
		return sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_JPEG_ARM);
	}
	else {
		SCE_DBG_LOG_WARNING("[JPEG] ARM decoder not initialized!");
		return 0;
	}
}

vita2d_texture *vita2d_load_JPEG_ARM_file(char *filename, vita2d_io_type io_type, int useDownScale, int downScalerHeight, int downScalerWidth)
{
	int ret;
	JpegDecCtrl	decCtrl;
	SceSize totalBufSize;
	SceJpegOutputInfo outputInfo;
	unsigned char *pJpeg;
	SceSize isize;
	unsigned char *texture_data;
	void *pCoefBuffer;
	int decodeMode = SCE_JPEG_MJPEG_WITH_DHT;
	int validWidth, validHeight;

	if (!decoder_arm_initialized) {
		SCE_DBG_LOG_WARNING("[JPEG] ARM decoder not initialized!");
		return NULL;
	}

	vita2d_texture *texture = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[JPEG] heap_alloc_heap_memory() returned NULL");
		return NULL;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	/*E Allocate stream buffer. */
	if (io_type) {
		SceFiosStat fios_stat;
		sceClibMemset(&fios_stat, 0, sizeof(SceFiosStat));
		sceFiosStatSync(NULL, filename, &fios_stat);
		isize = (SceSize)fios_stat.fileSize;
	}
	else {
		SceIoStat stat;
		sceIoGetstat(filename, &stat);
		isize = (SceSize)stat.st_size;
	}

	decCtrl.streamBufSize = ALIGN(isize, 4 * 1024);

	SceUID streamBufMemblock = sceKernelAllocMemBlock("jpegdecStreamBuffer",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, decCtrl.streamBufSize, NULL);

	if (streamBufMemblock < 0)
		return NULL;

	sceKernelGetMemBlockBase(streamBufMemblock, (void **)&pJpeg);

	/*E Read JPEG file to buffer. */
	if (io_type)
		readFileFIOS2(filename, pJpeg, isize);
	else
		readFile(filename, pJpeg, isize);

	/*E Get JPEG output information. */
	ret = sceJpegArmGetOutputInfo(pJpeg, isize,
		SCE_JPEG_PIXEL_RGBA8888, decodeMode, &outputInfo);
	if (ret == SCE_JPEG_ARM_ERROR_UNSUPPORT_SAMPLING &&
		outputInfo.colorSpace == (SCE_JPEG_CS_YCBCR | SCE_JPEG_CS_H1V1)) {
		/* Set SCE_JPEG_MJPEG_ANY_SAMPLING for decodeMode and retry sceJpegGetOutputInfo(),
		   if the JPEG's color space is YCbCr 4:4:4.*/
		decodeMode = SCE_JPEG_MJPEG_ANY_SAMPLING;
		ret = sceJpegArmGetOutputInfo(pJpeg, isize,
			SCE_JPEG_PIXEL_RGBA8888, decodeMode, &outputInfo);
	}

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[JPEG] sceJpegArmGetOutputInfo(): 0x%X", ret);
		goto error_free_file_in_buf;
	}

	/*E Allocate decoder memory. */
	totalBufSize = ALIGN(outputInfo.outputBufferSize + outputInfo.coefBufferSize, 4 * 1024);

	if (!check_free_memory(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, totalBufSize))
		goto error_free_file_in_buf;

	ret = sceGxmAllocDeviceMemLinux(SCE_GXM_DEVICE_HEAP_ID_USER_NC, SCE_GXM_MEMORY_ATTRIB_READ, totalBufSize, 4096, &texture->data_mem);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[JPEG] sceGxmAllocDeviceMemLinux(): 0x%X", ret);
		goto error_free_file_in_buf;
	}

	texture_data = (unsigned char *)texture->data_mem->mappedBase;
	decCtrl.bufferMemBlock = texture->data_mem->memBlockId;
	decCtrl.decodeBufSize = outputInfo.outputBufferSize;
	decCtrl.coefBufSize = outputInfo.coefBufferSize;

	/*E Calculate downscale ratio. */
	if (useDownScale)
	{
		float downScaleWidth, downScaleHeight, downScale;
		int downScaleDiv;

		downScaleWidth = (float)outputInfo.pitch[0].x / downScalerWidth / 4;
		downScaleHeight = (float)outputInfo.pitch[0].y / downScalerHeight;
		downScale = (downScaleWidth >= downScaleHeight) ? downScaleWidth : downScaleHeight;
		if (downScale <= 1.f) {
			/*E Downscale is not needed. */
		}
		else if (downScale <= 2.f) {
			decodeMode |= SCE_JPEG_MJPEG_DOWNSCALE_1_2;
		}
		else if (downScale <= 4.f) {
			decodeMode |= SCE_JPEG_MJPEG_DOWNSCALE_1_4;
		}
		else if (downScale <= 8.f) {
			decodeMode |= SCE_JPEG_MJPEG_DOWNSCALE_1_8;
		}
		downScaleDiv = (decodeMode >> 3) & 0xe;
		if (downScaleDiv) {
			validWidth = (outputInfo.imageWidth + downScaleDiv - 1) / downScaleDiv;
			validHeight = (outputInfo.imageHeight + downScaleDiv - 1) / downScaleDiv;
		}
		else {
			validWidth = outputInfo.imageWidth;
			validHeight = outputInfo.imageHeight;
		}
	}
	else
	{
		validWidth = outputInfo.imageWidth;
		validHeight = outputInfo.imageHeight;
	}

	/*E Set output buffer and quantized coefficients buffer. */
	if (outputInfo.coefBufferSize > 0 && decCtrl.coefBufSize > 0) {
		pCoefBuffer = (void*)(texture_data + decCtrl.decodeBufSize);
	}
	else {
		pCoefBuffer = NULL;
	}

	/*E Decode JPEG stream */
	ret = sceJpegArmDecodeMJpeg(
		pJpeg, isize,
		texture_data, decCtrl.decodeBufSize, decodeMode,
		pCoefBuffer, decCtrl.coefBufSize);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[JPEG] sceJpegArmDecodeMJpeg(): 0x%X", ret);
		goto error_free_file_both_buf;
	}

	FrameInfo pFrameInfo;

	pFrameInfo.pitchWidth = ret >> 16;
	pFrameInfo.pitchHeight = ret & 0xFFFF;
	pFrameInfo.validWidth = validWidth;
	pFrameInfo.validHeight = validHeight;

	if (pFrameInfo.pitchWidth > GXM_TEX_MAX_SIZE || pFrameInfo.pitchHeight > GXM_TEX_MAX_SIZE) {
		SCE_DBG_LOG_ERROR("[JPEG] %s texture is too big!", filename);
		goto error_free_file_both_buf;
	}

	/*E Free file buffer */
	if (streamBufMemblock >= 0) {
		sceKernelFreeMemBlock(streamBufMemblock);
	}

	/* Create the gxm texture */
	ret = sceGxmTextureInitLinear(
		&texture->gxm_tex,
		(void*)texture_data,
		SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,
		pFrameInfo.pitchWidth,
		pFrameInfo.pitchHeight,
		0);

	return texture;

error_free_file_both_buf:

	/*E Free decoder buffer */
	sceGxmFreeDeviceMemLinux(texture->data_mem);

error_free_file_in_buf:

	heap_free_heap_memory(vita2d_heap_internal, texture);

	/*E Free file buffer */
	if (streamBufMemblock >= 0) {
		sceKernelFreeMemBlock(streamBufMemblock);
	}

	return NULL;
}

vita2d_texture *vita2d_load_JPEG_ARM_buffer(const void *buffer, unsigned long buffer_size, int useDownScale, int downScalerHeight, int downScalerWidth)
{
	int ret;
	JpegDecCtrl	decCtrl;
	SceSize totalBufSize;
	SceJpegOutputInfo outputInfo;
	unsigned char *pJpeg;
	SceSize isize;
	unsigned char *texture_data;
	void *pCoefBuffer;
	int decodeMode = SCE_JPEG_MJPEG_WITH_DHT;
	int validWidth, validHeight;

	if (!decoder_arm_initialized) {
		SCE_DBG_LOG_WARNING("[JPEG] ARM decoder not initialized!");
		return NULL;
	}

	isize = buffer_size;
	pJpeg = (unsigned char *)buffer;

	/*E Get JPEG output information. */
	ret = sceJpegArmGetOutputInfo(pJpeg, isize,
		SCE_JPEG_PIXEL_RGBA8888, decodeMode, &outputInfo);
	if (ret == SCE_JPEG_ARM_ERROR_UNSUPPORT_SAMPLING &&
		outputInfo.colorSpace == (SCE_JPEG_CS_YCBCR | SCE_JPEG_CS_H1V1)) {
		/* Set SCE_JPEG_MJPEG_ANY_SAMPLING for decodeMode and retry sceJpegGetOutputInfo(),
		   if the JPEG's color space is YCbCr 4:4:4.*/
		decodeMode = SCE_JPEG_MJPEG_ANY_SAMPLING;
		ret = sceJpegArmGetOutputInfo(pJpeg, isize,
			SCE_JPEG_PIXEL_RGBA8888, decodeMode, &outputInfo);
	}

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[JPEG] sceJpegArmGetOutputInfo(): 0x%X", ret);
		return NULL;
	}

	/*E Allocate decoder memory. */
	totalBufSize = ALIGN(outputInfo.outputBufferSize + outputInfo.coefBufferSize, 4 * 1024);

	if (!check_free_memory(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, totalBufSize))
		return NULL;

	vita2d_texture *texture = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[JPEG] heap_alloc_heap_memory() returned NULL");
		return NULL;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	ret = sceGxmAllocDeviceMemLinux(SCE_GXM_DEVICE_HEAP_ID_USER_NC, SCE_GXM_MEMORY_ATTRIB_READ, totalBufSize, 4096, &texture->data_mem);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[JPEG] sceGxmAllocDeviceMemLinux(): 0x%X", ret);
		heap_free_heap_memory(vita2d_heap_internal, texture);
		return NULL;
	}

	texture_data = (unsigned char *)texture->data_mem->mappedBase;
	decCtrl.bufferMemBlock = texture->data_mem->memBlockId;
	decCtrl.decodeBufSize = outputInfo.outputBufferSize;
	decCtrl.coefBufSize = outputInfo.coefBufferSize;

	/*E Calculate downscale ratio. */
	if (useDownScale)
	{
		float downScaleWidth, downScaleHeight, downScale;
		int downScaleDiv;

		downScaleWidth = (float)outputInfo.pitch[0].x / downScalerWidth / 4;
		downScaleHeight = (float)outputInfo.pitch[0].y / downScalerHeight;
		downScale = (downScaleWidth >= downScaleHeight) ? downScaleWidth : downScaleHeight;
		if (downScale <= 1.f) {
			/*E Downscale is not needed. */
		}
		else if (downScale <= 2.f) {
			decodeMode |= SCE_JPEG_MJPEG_DOWNSCALE_1_2;
		}
		else if (downScale <= 4.f) {
			decodeMode |= SCE_JPEG_MJPEG_DOWNSCALE_1_4;
		}
		else if (downScale <= 8.f) {
			decodeMode |= SCE_JPEG_MJPEG_DOWNSCALE_1_8;
		}
		downScaleDiv = (decodeMode >> 3) & 0xe;
		if (downScaleDiv) {
			validWidth = (outputInfo.imageWidth + downScaleDiv - 1) / downScaleDiv;
			validHeight = (outputInfo.imageHeight + downScaleDiv - 1) / downScaleDiv;
		}
		else {
			validWidth = outputInfo.imageWidth;
			validHeight = outputInfo.imageHeight;
		}
	}
	else
	{
		validWidth = outputInfo.imageWidth;
		validHeight = outputInfo.imageHeight;
	}

	/*E Set output buffer and quantized coefficients buffer. */
	if (outputInfo.coefBufferSize > 0 && decCtrl.coefBufSize > 0) {
		pCoefBuffer = (void*)(texture_data + decCtrl.decodeBufSize);
	}
	else {
		pCoefBuffer = NULL;
	}

	/*E Decode JPEG stream */
	ret = sceJpegArmDecodeMJpeg(
		pJpeg, isize,
		texture_data, decCtrl.decodeBufSize, decodeMode,
		pCoefBuffer, decCtrl.coefBufSize);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[JPEG] sceJpegArmDecodeMJpeg(): 0x%X", ret);
		goto error_free_buf_dec_buf;
	}

	FrameInfo pFrameInfo;

	pFrameInfo.pitchWidth = ret >> 16;
	pFrameInfo.pitchHeight = ret & 0xFFFF;
	pFrameInfo.validWidth = validWidth;
	pFrameInfo.validHeight = validHeight;

	if (pFrameInfo.pitchWidth > GXM_TEX_MAX_SIZE || pFrameInfo.pitchHeight > GXM_TEX_MAX_SIZE) {
		SCE_DBG_LOG_ERROR("[JPEG] Texture is too big!");
		goto error_free_buf_dec_buf;
	}

	/* Create the gxm texture */
	ret = sceGxmTextureInitLinear(
		&texture->gxm_tex,
		(void*)texture_data,
		SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,
		pFrameInfo.pitchWidth,
		pFrameInfo.pitchHeight,
		0);

	return texture;

error_free_buf_dec_buf:

	heap_free_heap_memory(vita2d_heap_internal, texture);

	/*E Free decoder buffer */
	sceGxmFreeDeviceMemLinux(texture->data_mem);

	return NULL;
}

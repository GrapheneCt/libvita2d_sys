#include <kernel.h>
#include <gxm.h>
#include <libdbg.h>
#include <fios2.h>
#include <scepng.h>
#include "vita2d_sys.h"

#include "utils.h"
#include "heap.h"

#define PNG_SIGSIZE (8)

extern void* vita2d_heap_internal;

vita2d_texture *vita2d_load_PNG_file(char *filename, vita2d_io_type io_type)
{
	SceUID streamBufMemblock = SCE_UID_INVALID_UID;
	SceUID decBufMemblock = SCE_UID_INVALID_UID;
	int ret;
	SceSize totalBufSize;
	unsigned char *pPng;
	SceSize isize;
	unsigned char *texture_data;
	int width, height, outputFormat, streamFormat;

	vita2d_texture *texture = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[PNG] heap_alloc_heap_memory() returned NULL");
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

	streamBufMemblock = sceKernelAllocMemBlock("pngdecStreamBuffer",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, ALIGN(isize, 4 * 1024), NULL);

	if (streamBufMemblock < 0)
		return NULL;

	sceKernelGetMemBlockBase(streamBufMemblock, &pPng);

	/*E Read PNG file to buffer. */
	if (io_type)
		readFileFIOS2(filename, pPng, isize);
	else
		readFile(filename, pPng, isize);

	/*E Get PNG output information. */
	ret = scePngGetOutputInfo(pPng, isize, &width, &height, &outputFormat, &streamFormat);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[PNG] scePngGetOutputInfo(): 0x%X", ret);
		goto error_free_file_in_buf;
	}

	/*E Allocate decoder memory. */
	totalBufSize = ALIGN(ret, 4 * 1024);

	if (!check_free_memory(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, totalBufSize))
		goto error_free_file_in_buf;

	if (outputFormat != SCE_PNG_FORMAT_RGBA8888) {
		 decBufMemblock = sceKernelAllocMemBlock("pngdecDecBuffer",
			SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, totalBufSize, NULL);

		 if (decBufMemblock < 0) {
			 SCE_DBG_LOG_ERROR("[PNG] sceKernelAllocMemBlock(): 0x%X", decBufMemblock);
			 goto error_free_file_in_buf;
		 }

		sceKernelGetMemBlockBase(decBufMemblock, &texture_data);
	}
	else {
		ret = sceGxmAllocDeviceMemLinux(SCE_GXM_DEVICE_HEAP_ID_USER_NC, SCE_GXM_MEMORY_ATTRIB_READ, totalBufSize, 4096, &texture->data_mem);
		if (ret < 0) {
			SCE_DBG_LOG_ERROR("[PNG] sceGxmAllocDeviceMemLinux(): 0x%X", ret);
			goto error_free_file_in_buf;
		}

		texture_data = (unsigned char *)texture->data_mem->mappedBase;
	}

	/*E Decode PNG stream */
	ret = scePngDec(
		texture_data, totalBufSize,
		pPng, isize, &width,
		&height, &outputFormat);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[PNG] scePngDec(): 0x%X", ret);
		goto error_free_file_both_buf;
	}

	if (width > GXM_TEX_MAX_SIZE || height > GXM_TEX_MAX_SIZE) {
		SCE_DBG_LOG_ERROR("[PNG] %s texture is too big!", filename);
		goto error_free_file_both_buf;
	}

	/*E Free file buffer */
	if (streamBufMemblock >= 0) {
		sceKernelFreeMemBlock(streamBufMemblock);
		streamBufMemblock = SCE_UID_INVALID_UID;
	}

	if (outputFormat != SCE_PNG_FORMAT_RGBA8888) {

		ret = sceGxmAllocDeviceMemLinux(SCE_GXM_DEVICE_HEAP_ID_USER_NC, SCE_GXM_MEMORY_ATTRIB_READ, ((width + 7) & ~7) * height * 4, 4096, &texture->data_mem);
		if (ret < 0) {
			SCE_DBG_LOG_ERROR("[PNG] sceGxmAllocDeviceMemLinux(): 0x%X", ret);
			goto error_free_file_both_buf;
		}

		ret = scePngConvertToRGBA(texture->data_mem->mappedBase, texture_data, width, height, outputFormat);

		if (decBufMemblock >= 0) {
			sceKernelFreeMemBlock(decBufMemblock);
			decBufMemblock = SCE_UID_INVALID_UID;
		}

		if (ret < 0) {
			SCE_DBG_LOG_ERROR("[PNG] scePngConvertToRGBA(): 0x%X", ret);
			goto error_free_file_both_buf;
		}

		texture_data = (unsigned char *)texture->data_mem->mappedBase;
	}

	/* Create the gxm texture */
	ret = sceGxmTextureInitLinear(
		&texture->gxm_tex,
		(void*)texture_data,
		SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,
		width,
		height,
		0);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[PNG] sceGxmTextureInitLinear(): 0x%X", ret);
		goto error_free_file_both_buf;
	}

	return texture;

error_free_file_both_buf:

	/*E Free decoder buffer */
	sceGxmFreeDeviceMemLinux(texture->data_mem);

	if (decBufMemblock >= 0) {
		sceKernelFreeMemBlock(decBufMemblock);
	}

error_free_file_in_buf:

	heap_free_heap_memory(vita2d_heap_internal, texture);

	/*E Free file buffer */
	if (streamBufMemblock >= 0) {
		sceKernelFreeMemBlock(streamBufMemblock);
	}

	return NULL;
}

vita2d_texture *vita2d_load_PNG_buffer(const void *buffer, unsigned long buffer_size)
{
	int ret;
	SceSize totalBufSize;
	unsigned char *pPng = (unsigned char *)buffer;
	SceSize isize = buffer_size;
	unsigned char *texture_data;
	int width, height, outputFormat, streamFormat;

	vita2d_texture *texture = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[PNG] heap_alloc_heap_memory() returned NULL");
		return NULL;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	/*E Get PNG output information. */
	ret = scePngGetOutputInfo(pPng, isize, &width, &height, &outputFormat, &streamFormat);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[PNG] scePngGetOutputInfo(): 0x%X", ret);
		goto error_free_heap;
	}

	/*E Allocate decoder memory. */
	totalBufSize = ALIGN(ret, 4 * 1024);

	if (!check_free_memory(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, totalBufSize))
		goto error_free_heap;

	ret = sceGxmAllocDeviceMemLinux(SCE_GXM_DEVICE_HEAP_ID_USER_NC, SCE_GXM_MEMORY_ATTRIB_READ, totalBufSize, 4096, &texture->data_mem);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[PNG] sceGxmAllocDeviceMemLinux(): 0x%X", ret);
		goto error_free_heap;
	}

	texture_data = (unsigned char *)texture->data_mem->mappedBase;

	/*E Decode PNG stream */
	ret = scePngDec(
		texture_data, totalBufSize,
		pPng, isize, &width,
		&height, &outputFormat);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[PNG] scePngDec(): 0x%X", ret);
		goto error_free_out_buf;
	}

	if (width > GXM_TEX_MAX_SIZE || height > GXM_TEX_MAX_SIZE) {
		SCE_DBG_LOG_ERROR("[PNG] texture is too big!");
		goto error_free_out_buf;
	}

	/* Create the gxm texture */
	ret = sceGxmTextureInitLinear(
		&texture->gxm_tex,
		(void*)texture_data,
		SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR,
		width,
		height,
		0);

	return texture;

error_free_out_buf:

	/*E Free decoder buffer */
	sceGxmFreeDeviceMemLinux(texture->data_mem);

error_free_heap:

	heap_free_heap_memory(vita2d_heap_internal, texture);

	return NULL;
}
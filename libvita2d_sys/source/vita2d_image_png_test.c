#include <psp2/kernel/iofilemgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/gxm.h>
#include <psp2/libdbg.h>
#include <psp2/scepng.h>
#include "vita2d_sys.h"
#include "fios2ac.h"

#define ROUND_UP(x, a)	((((unsigned int)x)+((a)-1u))&(~((a)-1u)))

extern void* mspace_internal;

static vita2d_texture *_vita2d_load_PNG_generic(const void *io_ptr, png_rw_ptr read_data_fn)
{
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		SCE_DBG_LOG_ERROR("[PNG] png_create_read_struct() returned NULL");
		goto error_create_read;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		SCE_DBG_LOG_ERROR("[PNG] png_create_info_struct() returned NULL");
		goto error_create_info;
	}

	png_bytep *row_ptrs = NULL;

	if (setjmp(png_jmpbuf(png_ptr))) {
		SCE_DBG_LOG_ERROR("[PNG] Invalid png data");
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
		return NULL;
	}

	png_set_read_fn(png_ptr, (png_voidp)io_ptr, read_data_fn);
	png_set_sig_bytes(png_ptr, PNG_SIGSIZE);
	png_read_info(png_ptr, info_ptr);

	unsigned int width, height;
	int bit_depth, color_type;

	png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth,
		&color_type, NULL, NULL, NULL);

	if ((color_type == PNG_COLOR_TYPE_PALETTE && bit_depth <= 8)
		|| (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		|| png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)
		|| (bit_depth == 16)) {
			png_set_expand(png_ptr);
	}

	if (bit_depth == 16)
		png_set_scale_16(png_ptr);

	if (bit_depth == 8 && color_type == PNG_COLOR_TYPE_RGB)
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

	if (color_type == PNG_COLOR_TYPE_GRAY ||
	    color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb(png_ptr);

	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
		png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);
	}

	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
		png_set_expand_gray_1_2_4_to_8(png_ptr);

	if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);

	if (bit_depth < 8)
		png_set_packing(png_ptr);

	png_read_update_info(png_ptr, info_ptr);

	row_ptrs = (png_bytep *)sceClibMspaceMalloc(mspace_internal, sizeof(png_bytep) * height);
	if (!row_ptrs) {
		SCE_DBG_LOG_ERROR("[PNG] sceClibMspaceMalloc() returned NULL");
		goto error_alloc_rows;
	}

	vita2d_texture *texture = vita2d_create_empty_texture(width, height);
	if (!texture) {
		SCE_DBG_LOG_ERROR("[PNG] vita2d_create_empty_texture() returned NULL");
		goto error_create_tex;
	}

	void *texture_data = vita2d_texture_get_datap(texture);
	unsigned int stride = vita2d_texture_get_stride(texture);

	int i;
	for (i = 0; i < height; i++) {
		row_ptrs[i] = (png_bytep)(texture_data + i*stride);
	}

	png_read_image(png_ptr, row_ptrs);

	png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)0);
	sceClibMspaceFree(mspace_internal, row_ptrs);

	return texture;

error_create_tex:
	sceClibMspaceFree(mspace_internal, row_ptrs);
error_alloc_rows:
	png_destroy_info_struct(png_ptr, &info_ptr);
error_create_info:
	png_destroy_read_struct(&png_ptr, (png_infopp)0, (png_infopp)0);
error_create_read:
	return NULL;
}

vita2d_texture *vita2d_load_PNG_file_FIOS2(char *mountedFilePath)
{
	int ret;
	SceFiosFH fd;

	ret = sceFiosFHOpenSync(NULL, &fd, mountedFilePath, NULL);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[PNG] Can't open file %s sceFiosFHOpenSync(): 0x%X", mountedFilePath, ret);
		goto exit_error;
	}



	vita2d_texture *texture = _vita2d_load_PNG_generic((void *)&fd, _vita2d_read_png_file_fn_FIOS2);
	sceFiosFHCloseSync(NULL, fd);
	return texture;

exit_close:
	sceFiosFHCloseSync(NULL, fd);
exit_error:
	return NULL;
}

vita2d_texture *vita2d_load_PNG_file(char *filename, int io_type)
{
	if (io_type == 1)
		return vita2d_load_PNG_file_FIOS2(filename);

	int ret;
	unsigned int isize;
	void *streamBuf;
	SceUID streamBufMemblock;
	SceIoStat stat;

	SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0777);
	if (fd < 0) {
		SCE_DBG_LOG_ERROR("[PNG] Can't open file %s sceIoOpen(): 0x%X", filename, fd);
		goto exit_error;
	}

	ret = sceIoGetstat(filename, &stat);
	if (fd < 0) {
		SCE_DBG_LOG_ERROR("[PNG] sceIoGetstat(): 0x%X", ret);
		goto exit_close;
	}

	isize = ROUND_UP(stat.st_size, 4 * 1024);

	streamBufMemblock = sceKernelAllocMemBlock("pngdecStreamBuffer",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, isize, NULL);

	if (streamBufMemblock < 0)
		goto exit_close;

	sceKernelGetMemBlockBase(streamBufMemblock, &streamBuf);

	ret = sceIoRead(fd, streamBuf, stat.st_size);
	if (ret != stat.st_size) {
		SCE_DBG_LOG_ERROR("[PNG] sceIoRead(): 0x%X", ret);
		goto exit_free;
	}

	vita2d_texture *texture = _vita2d_load_PNG_generic(streamBuf, (unsigned int)stat.st_size);

	sceKernelFreeMemBlock(streamBufMemblock);
	sceIoClose(fd);

	return texture;

exit_free:
	sceKernelFreeMemBlock(streamBufMemblock);
exit_close:
	sceIoClose(fd);
exit_error:
	return NULL;
}

vita2d_texture *vita2d_load_PNG_buffer(const void *buffer)
{
	return _vita2d_load_PNG_generic((void *)&buffer_address, _vita2d_read_png_buffer_fn);
}

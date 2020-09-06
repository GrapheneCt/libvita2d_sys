#include <psp2/kernel/iofilemgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/gxm.h>
#include <psp2/fios2.h>
#include <psp2/libdbg.h>
#include "vita2d_sys.h"

#include "utils.h"
#include "heap.h"
#include "gim.h"

extern void* vita2d_heap_internal;

vita2d_texture *vita2d_load_GIM_file_FIOS2(char* mountedFilePath)
{
	int ret;

	SceKernelMemBlockType mem_type = vita2d_texture_get_alloc_memblock_type();

	vita2d_texture *texture = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[GIM] heap_alloc_heap_memory() returned NULL");
		goto exit_error;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	SceFiosStat fios_stat;
	sceFiosStatSync(NULL, mountedFilePath, &fios_stat);
	unsigned int size = ALIGN((SceSize)fios_stat.fileSize, mem_type == SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE ? 4 * 1024 : 256 * 1024);

	SceUID tex_data_uid = sceKernelAllocMemBlock("gpu_mem", mem_type, size, NULL);

	texture->data_UID = tex_data_uid;

	void* texture_data;
	if (sceKernelGetMemBlockBase(tex_data_uid, &texture_data) < 0)
		goto exit_error_free;

	ret = sceGxmMapMemory(texture_data, size, SCE_GXM_MEMORY_ATTRIB_READ);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGxmMapMemory(): 0x%X", ret);
		goto exit_error_free;
	}

	unsigned int read_size;

	SceFiosFH fd;
	ret = sceFiosFHOpenSync(NULL, &fd, mountedFilePath, NULL);

	if (ret < 0)
		SCE_DBG_LOG_ERROR("[GIM] Can't open file %s sceFiosFHOpenSync(): 0x%X", mountedFilePath, ret);

	read_size = sceFiosFHReadSync(NULL, fd, texture_data, fios_stat.fileSize);
	sceFiosFHCloseSync(NULL, fd);

	if (read_size != fios_stat.fileSize) {
		SCE_DBG_LOG_ERROR("[GIM] Can't read file %s", mountedFilePath);
		goto exit_error_free;
	}

	ret = sceGimCheckData(texture_data);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGimCheckData(): 0x%X", ret);
		goto exit_error_free;
	}

	ret = sceGimInitTexture(&texture->gxm_tex, texture_data);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGimInitTexture(): 0x%X", ret);
		goto exit_error_free;
	}

	return texture;

exit_error_free:
	sceGxmUnmapMemory(texture_data);
	sceKernelFreeMemBlock(tex_data_uid);
	return NULL;
exit_error:
	return NULL;
}

vita2d_texture *vita2d_load_GIM_file(char *filename, int io_type)
{
	int ret;

	if (io_type == 1)
		return vita2d_load_GIM_file_FIOS2(filename);

	SceKernelMemBlockType mem_type = vita2d_texture_get_alloc_memblock_type();

	vita2d_texture *texture = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[GIM] heap_alloc_heap_memory() returned NULL");
		goto exit_error;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	SceIoStat file_stat;
	sceIoGetstat(filename, &file_stat);
	unsigned int size = ALIGN(file_stat.st_size, mem_type == SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE ? 4 * 1024 : 256 * 1024);

	SceUID tex_data_uid = sceKernelAllocMemBlock("gpu_mem", mem_type, size, NULL);
	texture->data_UID = tex_data_uid;

	void* texture_data;
	if (sceKernelGetMemBlockBase(tex_data_uid, &texture_data) < 0)
		goto exit_error_free;

	ret = sceGxmMapMemory(texture_data, size, SCE_GXM_MEMORY_ATTRIB_READ);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGxmMapMemory(): 0x%X", ret);
		goto exit_error_free;
	}

	unsigned int read_size;

	SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0);

	if (fd < 0)
		SCE_DBG_LOG_ERROR("[GIM] Can't open file %s sceIoOpen(): 0x%X", filename, ret);

	read_size = sceIoRead(fd, texture_data, file_stat.st_size);
	sceIoClose(fd);

	if (read_size != file_stat.st_size) {
		SCE_DBG_LOG_ERROR("[GIM] Can't read file %s", filename);
		goto exit_error_free;
	}

	ret = sceGimCheckData(texture_data);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGimCheckData(): 0x%X", ret);
		goto exit_error_free;
	}

	ret = sceGimInitTexture(&texture->gxm_tex, texture_data);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGimInitTexture(): 0x%X", ret);
		goto exit_error_free;
	}

	return texture;

exit_error_free:
	sceGxmUnmapMemory(texture_data);
	sceKernelFreeMemBlock(tex_data_uid);
	return NULL;
exit_error:
	return NULL;
}

vita2d_texture *vita2d_load_GIM_buffer(void* buffer)
{
	int ret;

	vita2d_texture *texture = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[GIM] heap_alloc_heap_memory() returned NULL");
		goto exit_error;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	ret = sceGimCheckData(buffer);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGimCheckData(): 0x%X", ret);
		goto exit_error;
	}

	ret = sceGimInitTexture(&texture->gxm_tex, buffer);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGimInitTexture(): 0x%X", ret);
		goto exit_error;
	}

	return texture;

exit_error:
	return NULL;
}
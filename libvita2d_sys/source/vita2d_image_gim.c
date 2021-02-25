#include <kernel.h>
#include <gxm.h>
#include <gim.h>
#include <fios2.h>
#include <libdbg.h>
#include "vita2d_sys.h"

#include "utils.h"
#include "heap.h"

extern void* vita2d_heap_internal;

vita2d_texture *vita2d_load_GIM_file_FIOS2(char* mountedFilePath)
{
	int ret;

	SceGxmDeviceHeapId mem_type = vita2d_texture_get_heap_type();

	vita2d_texture *texture = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[GIM] heap_alloc_heap_memory() returned NULL");
		goto exit_error;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	SceFiosStat fios_stat;
	sceFiosStatSync(NULL, mountedFilePath, &fios_stat);

	ret = sceGxmAllocDeviceMemLinux(mem_type, SCE_GXM_MEMORY_ATTRIB_READ, (SceSize)fios_stat.fileSize, 4096, &texture->data_mem);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGxmAllocDeviceMemLinux(): 0x%X", ret);
		heap_free_heap_memory(vita2d_heap_internal, texture);
		return NULL;
	}

	unsigned int read_size;

	SceFiosFH fd;
	ret = sceFiosFHOpenSync(NULL, &fd, mountedFilePath, NULL);

	if (ret < 0)
		SCE_DBG_LOG_ERROR("[GIM] Can't open file %s sceFiosFHOpenSync(): 0x%X", mountedFilePath, ret);

	read_size = sceFiosFHReadSync(NULL, fd, texture->data_mem->mappedBase, fios_stat.fileSize);
	sceFiosFHCloseSync(NULL, fd);

	if (read_size != fios_stat.fileSize) {
		SCE_DBG_LOG_ERROR("[GIM] Can't read file %s", mountedFilePath);
		goto exit_error_free;
	}

	ret = sceGimCheckData(texture->data_mem->mappedBase);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGimCheckData(): 0x%X", ret);
		goto exit_error_free;
	}

	ret = sceGimInitTexture(&texture->gxm_tex, texture->data_mem->mappedBase);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGimInitTexture(): 0x%X", ret);
		goto exit_error_free;
	}

	return texture;

exit_error_free:
	sceGxmFreeDeviceMemLinux(texture->data_mem);
	heap_free_heap_memory(vita2d_heap_internal, texture);
	return NULL;
exit_error:
	return NULL;
}

vita2d_texture *vita2d_load_GIM_file(char *filename, vita2d_io_type io_type)
{
	int ret;

	if (io_type == 1)
		return vita2d_load_GIM_file_FIOS2(filename);

	SceGxmDeviceHeapId mem_type = vita2d_texture_get_heap_type();

	vita2d_texture *texture = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[GIM] heap_alloc_heap_memory() returned NULL");
		goto exit_error;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	SceIoStat file_stat;
	sceIoGetstat(filename, &file_stat);

	ret = sceGxmAllocDeviceMemLinux(mem_type, SCE_GXM_MEMORY_ATTRIB_READ, file_stat.st_size, 4096, &texture->data_mem);
	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGxmAllocDeviceMemLinux(): 0x%X", ret);
		heap_free_heap_memory(vita2d_heap_internal, texture);
		return NULL;
	}

	unsigned int read_size;

	SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0);

	if (fd < 0)
		SCE_DBG_LOG_ERROR("[GIM] Can't open file %s sceIoOpen(): 0x%X", filename, ret);

	read_size = sceIoRead(fd, texture->data_mem->mappedBase, file_stat.st_size);
	sceIoClose(fd);

	if (read_size != file_stat.st_size) {
		SCE_DBG_LOG_ERROR("[GIM] Can't read file %s", filename);
		goto exit_error_free;
	}

	ret = sceGimCheckData(texture->data_mem->mappedBase);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGimCheckData(): 0x%X", ret);
		goto exit_error_free;
	}

	ret = sceGimInitTexture(&texture->gxm_tex, texture->data_mem->mappedBase);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GIM] sceGimInitTexture(): 0x%X", ret);
		goto exit_error_free;
	}

	return texture;

exit_error_free:
	sceGxmFreeDeviceMemLinux(texture->data_mem);
	heap_free_heap_memory(vita2d_heap_internal, texture);
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
#include <psp2/kernel/iofilemgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/gxm.h>
#include "gxt.h"
#include <psp2/libdbg.h>
#include "vita2d_sys.h"

#include "fios2ac.h"
#include "utils.h"
#include "heap.h"

extern void* heap_internal;

vita2d_texture *vita2d_load_additional_GXT(vita2d_texture *initial_tex, int texture_index)
{
	int ret;

	vita2d_texture *texture = heap_alloc_heap_memory(heap_internal, sizeof(*texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[GXT] heap_alloc_heap_memory() returned NULL");
		goto exit_error;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	void *actual_texture_data = sceGxtGetDataAddress(initial_tex->gxt_data);

	SceGxmTexture texture_gxm;
	ret = sceGxtInitTexture(&texture_gxm, initial_tex->gxt_data, actual_texture_data, texture_index);

	if (ret < 0)
		SCE_DBG_LOG_ERROR("[GXT] sceGxtInitTexture(): 0x%X", ret);

	texture->gxm_tex = texture_gxm;

	return texture;
exit_error:
	return NULL;

}

vita2d_texture *vita2d_load_GXT_file_FIOS2(char* mountedFilePath, int texture_index)
{
	int ret;

	vita2d_texture *texture = heap_alloc_heap_memory(heap_internal, sizeof(*texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[GXT] heap_alloc_heap_memory() returned NULL");
		goto exit_error;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	SceFiosStat fios_stat;
	sceFiosStatSync(NULL, mountedFilePath, &fios_stat);
	unsigned int size = ALIGN((SceSize)fios_stat.fileSize, 4 * 1024);

	SceUID tex_data_uid = sceKernelAllocMemBlock("gpu_mem", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);

	texture->data_UID = tex_data_uid;

	void* texture_data;
	if (sceKernelGetMemBlockBase(tex_data_uid, &texture_data) < 0)
		goto exit_error_free;

	ret = sceGxmMapMemory(texture_data, size, SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GXT] sceGxmMapMemory(): 0x%X", ret);
		goto exit_error_free;
	}

	unsigned int read_size;

	SceFiosFH fd;
	ret = sceFiosFHOpenSync(NULL, &fd, mountedFilePath, NULL);

	if (ret < 0)
		SCE_DBG_LOG_ERROR("[GXT] Can't open file %s sceFiosFHOpenSync(): 0x%X", mountedFilePath, ret);

	read_size = sceFiosFHReadSync(NULL, fd, texture_data, fios_stat.fileSize);
	sceFiosFHCloseSync(NULL, fd);

	if (read_size != fios_stat.fileSize) {
		SCE_DBG_LOG_ERROR("[GXT] Can't read file %s", mountedFilePath);
		goto exit_error_free;
	}

	ret = sceGxtCheckData(texture_data);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GXT] sceGxtCheckData(): 0x%X", ret);
		goto exit_error_free;
	}

	texture->gxt_data = texture_data;

	void *actual_texture_data = sceGxtGetDataAddress(texture_data);

	ret = sceGxtInitTexture(&texture->gxm_tex, texture_data, actual_texture_data, texture_index);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GXT] sceGxtInitTexture(): 0x%X", ret);
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

vita2d_texture *vita2d_load_GXT_file(char *filename, int texture_index, int io_type)
{
	int ret;

	if (io_type == 1)
		return vita2d_load_GXT_file_FIOS2(filename, texture_index);

	vita2d_texture *texture = heap_alloc_heap_memory(heap_internal, sizeof(*texture));
	if (!texture) {
		SCE_DBG_LOG_ERROR("[GXT] heap_alloc_heap_memory() returned NULL");
		goto exit_error;
	}

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	SceIoStat file_stat;
	sceIoGetstat(filename, &file_stat);
	unsigned int size = ALIGN(file_stat.st_size, 4 * 1024);

	SceUID tex_data_uid = sceKernelAllocMemBlock("gpu_mem", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);

	texture->data_UID = tex_data_uid;

	void* texture_data;
	if (sceKernelGetMemBlockBase(tex_data_uid, &texture_data) < 0)
		goto exit_error_free;

	ret = sceGxmMapMemory(texture_data, size, SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GXT] sceGxmMapMemory(): 0x%X", ret);
		goto exit_error_free;
	}

	unsigned int read_size;

	SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0);

	if (fd < 0)
		SCE_DBG_LOG_ERROR("[GXT] Can't open file %s sceIoOpen(): 0x%X", filename, ret);

	read_size = sceIoRead(fd, texture_data, file_stat.st_size);
	sceIoClose(fd);

	if (read_size != file_stat.st_size) {
		SCE_DBG_LOG_ERROR("[GXT] Can't read file %s", filename);
		goto exit_error_free;
	}

	ret = sceGxtCheckData(texture_data);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GXT] sceGxtCheckData(): 0x%X", ret);
		goto exit_error_free;
	}

	texture->gxt_data = texture_data;

	void *actual_texture_data = sceGxtGetDataAddress(texture_data);

	ret = sceGxtInitTexture(&texture->gxm_tex, texture_data, actual_texture_data, texture_index);

	if (ret < 0) {
		SCE_DBG_LOG_ERROR("[GXT] sceGxtInitTexture(): 0x%X", ret);
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

void vita2d_free_additional_GXT(vita2d_texture *tex)
{
	heap_free_heap_memory(heap_internal, tex);
}

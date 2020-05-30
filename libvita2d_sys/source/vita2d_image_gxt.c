#include <psp2/io/fcntl.h>
#include <psp2/kernel/clib.h>
#include <psp2/gxm.h>
#include <psp2/gxt.h>
#include <psp2/io/stat.h>
#include "vita2d_sys.h"

#include "fios2ac.h"
#include "utils.h"

extern void* mspace_internal;

vita2d_texture *vita2d_load_additional_GXT(vita2d_texture *initial_tex, int texture_index)
{
	vita2d_texture *texture = sceClibMspaceMalloc(mspace_internal, sizeof(*texture));
	if (!texture)
		goto exit_error;

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	void *actual_texture_data = sceGxtGetDataAddress(initial_tex->gxt_data);

	SceGxmTexture texture_gxm;
	sceGxtInitTexture(&texture_gxm, initial_tex->gxt_data, actual_texture_data, texture_index);

	texture->gxm_tex = texture_gxm;

	return texture;
exit_error:
	return NULL;

}

vita2d_texture *vita2d_load_GXT_file_FIOS2(char* mountedFilePath, int texture_index)
{

	vita2d_texture *texture = sceClibMspaceMalloc(mspace_internal, sizeof(*texture));
	if (!texture)
		goto exit_error;

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	SceFiosStat fios_stat;
	sceFiosStatSync(NULL, mountedFilePath, &fios_stat);
	unsigned int size = ALIGN((SceSize)fios_stat.fileSize, 4 * 1024);

	SceUID tex_data_uid = sceKernelAllocMemBlock("gpu_mem", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);

	texture->data_UID = tex_data_uid;

	void* texture_data;
	if (sceKernelGetMemBlockBase(tex_data_uid, &texture_data) < 0)
		goto exit_error_free;

	if (sceGxmMapMemory(texture_data, size, SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE) < 0)
		goto exit_error_free;

	unsigned int read_size;

	SceFiosFH fd;
	sceFiosFHOpenSync(NULL, &fd, mountedFilePath, NULL);
	read_size = sceFiosFHReadSync(NULL, fd, texture_data, fios_stat.fileSize);
	sceFiosFHCloseSync(NULL, fd);

	if (read_size != fios_stat.fileSize)
		goto exit_error_free;

	if (sceGxtCheckData(texture_data) < 0)
		goto exit_error_free;

	texture->gxt_data = texture_data;

	void *actual_texture_data = sceGxtGetDataAddress(texture_data);

	if (sceGxtInitTexture(&texture->gxm_tex, texture_data, actual_texture_data, texture_index) < 0)
		goto exit_error_free;

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
	if (io_type == 1)
		return vita2d_load_GXT_file_FIOS2(filename, texture_index);

	vita2d_texture *texture = sceClibMspaceMalloc(mspace_internal, sizeof(*texture));
	if (!texture)
		goto exit_error;

	sceClibMemset(texture, 0, sizeof(vita2d_texture));

	SceIoStat file_stat;
	sceIoGetstat(filename, &file_stat);
	unsigned int size = ALIGN(file_stat.st_size, 4 * 1024);

	SceUID tex_data_uid = sceKernelAllocMemBlock("gpu_mem", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, size, NULL);

	texture->data_UID = tex_data_uid;

	void* texture_data;
	if (sceKernelGetMemBlockBase(tex_data_uid, &texture_data) < 0)
		goto exit_error_free;

	if (sceGxmMapMemory(texture_data, size, SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE) < 0)
		goto exit_error_free;

	unsigned int read_size;

	SceUID fd = sceIoOpen(filename, SCE_O_RDONLY, 0);
	read_size = sceIoRead(fd, texture_data, file_stat.st_size);
	sceIoClose(fd);

	if (read_size != file_stat.st_size)
		goto exit_error_free;

	if (sceGxtCheckData(texture_data) < 0)
		goto exit_error_free;

	texture->gxt_data = texture_data;

	void *actual_texture_data = sceGxtGetDataAddress(texture_data);

	if (sceGxtInitTexture(&texture->gxm_tex, texture_data, actual_texture_data, texture_index) < 0)
		goto exit_error_free;

	return texture;

exit_error_free:
	sceGxmUnmapMemory(texture_data);
	sceKernelFreeMemBlock(tex_data_uid);
	return NULL;
exit_error:
	return NULL;
}

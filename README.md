## VITA2DLIB_SYS

Simple and Fast (using the GPU) 2D library for the PSVita system mode applications

## USAGE

This library is disigned to be used only with applications running in system mode. General usage is the same as standard vita2d. However, initialization proccess is different.

**Textures can only be used in applications with "allow c heap" boot param != 0**


**If you want to use dynamic memory allocation in your application and "allow c heap" boot param is set to 0, sceClibMspace functions must be used**

## INITIALIZATION

To initialize vita2d in your application following must be done:
```
void *mspace;
void *clibm_base;
SceUID clib_heap = sceKernelAllocMemBlock("ClibHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, CLIB_HEAP_SIZE, NULL);
sceKernelGetMemBlockBase(clib_heap, &clibm_base);
mspace = sceClibMspaceCreate(clibm_base, CLIB_HEAP_SIZE);

vita2d_clib_pass_mspace(mspace);
```

## VITA2DLIB_SYS

Simple and Fast (using the GPU) 2D library for the PSVita system mode applications

## Usage

This library is disigned to be used only with applications running in system mode. General usage is the same as standard vita2d. However, initialization proccess is different.

You can control shared fb directly from your application using the following:
```
SceUID vita2d_get_shfbid(); //Returns shared fb id opened by application.
```

**- BMP textures can be used in all applications**

**- JPEG are decoded using hardware decoder. Make sure your application is allowed to use phycont memory**

```
vita2d_JPEG_decoder_initialize();
/* Load your JPEG textures here */
vita2d_JPEG_decoder_finish();
```

**- PNG textures can only be used in applications with "allow C heap" boot param != 0**

**- If you want to use dynamic memory allocation in your application and "allow C heap" boot param is set to 0, sceClibMspace functions must be used**

## Initialization

To initialize vita2d_sys in your application following must be done. Minimum value for CLIB_HEAP_SIZE is 1MiB.
```
void *mspace;
void *clibm_base;
SceUID clib_heap = sceKernelAllocMemBlock("ClibHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, CLIB_HEAP_SIZE, NULL);
sceKernelGetMemBlockBase(clib_heap, &clibm_base);
mspace = sceClibMspaceCreate(clibm_base, CLIB_HEAP_SIZE);

vita2d_clib_pass_mspace(mspace);

vita2d_init();
```

## Drawing process

```
vita2d_start_drawing();
.
.
.
vita2d_end_drawing();
vita2d_wait_rendering_done();
vita2d_end_shfb();
```

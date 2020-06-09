## VITA2DLIB_SYS

Simple and Fast (using the GPU) 2D library for the PSVita with "system mode" applications support.

libvita2d_sys can be used in both "system mode" and "game mode" applications.  Appropriate GXM mode and framebuffer mode are set automatically when vita2d_init() is called.

## Usage

If your application is running in "system mode", you can control shared fb directly from your application using the following:
```
SceUID vita2d_get_shfbid(); //Returns shared fb id opened by application.
```
## Textures

Loading from FIOS2 overlay (for example, loading directly from PSARC archive) is supported. When calling vita2d_load_XXX_file() specify 1 for io_type to use FIOS2 or 0 to use SceIo. When using FIOS2, remember to specify your mounted path to the file instead of the actual file path. 

**- BMP textures can be used in all applications**

**- JPEG textures are decoded using hardware decoder. Specify 0 for useMainMemory to use phycont memory or 1 to use main user mamory**

```
vita2d_JPEG_decoder_initialize(1);
/* Load your JPEG textures here */
vita2d_JPEG_decoder_finish();
```

**- PNG textures can only be used in applications with newlib heap available**

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

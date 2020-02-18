## VITA2DLIB_SYS

Simple and Fast (using the GPU) 2D library for the PSVita system mode applications

## USAGE

This library is disigned to be used only with applications running in system mode. General usage is the same as standard vita2d. However, initialization proccess is different.

You can control some shared fb pramaters using the following:
```
SceUID vita2d_get_shfbid(); //Returns shared fb id opened by application. This id can later be used to directly control shared fb from your application.
void vita2d_set_shfb_mode(int mode); //Set shfb swapping mode. This value can be set to 1 or 2. If your application isn't displayed properly, change this value.
void vita2d_set_shfb_delay(unsigned int delay); //Set delay before swapping shfb buffers in microseconds. Increase this value if your application have screen tearing issues or other dispaly problems
```

**Textures can only be used in applications with "allow c heap" boot param != 0**


**If you want to use dynamic memory allocation in your application and "allow c heap" boot param is set to 0, sceClibMspace functions must be used**

## INITIALIZATION

To initialize vita2d_sys in your application following must be done. Minimum value for CLIB_HEAP_SIZE is 1MiB.
```
void *mspace;
void *clibm_base;
SceUID clib_heap = sceKernelAllocMemBlock("ClibHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, CLIB_HEAP_SIZE, NULL);
sceKernelGetMemBlockBase(clib_heap, &clibm_base);
mspace = sceClibMspaceCreate(clibm_base, CLIB_HEAP_SIZE);

vita2d_clib_pass_mspace(mspace);

vita2d_set_shfb_mode(mode);
vita2d_set_shfb_delay(delay);

vita2d_init();
```

## ISSUES

SceShell overlay, including IME and common dialog, is not displayed properly due to sharedfb overwriting

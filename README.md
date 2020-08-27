## VITA2DLIB_SYS

Simple and Fast (using the GPU) 2D library for the PSVita with "system mode" applications support.

libvita2d_sys can be used in both "system mode" and "game mode" applications.  Appropriate GXM mode and framebuffer mode are set automatically when vita2d_init() is called.

## Usage

If your application is running in "system mode", you can control shared fb directly from your application using the following:
```
SceUID vita2d_get_shfbid(); //Returns shared fb id opened by application.
```

## Custom resolution

To set custom resolution, call vita2d_display_set_resolution() before initializing the library:
```
void vita2d_display_set_resolution(int hRes, int vRes); //Set display resolution.
```

Native 1280x725 and 1920x1088 resolutions are supported for PS TV.
Use Sharpscale to be able to use 1280x720 and 1920x1080 or for normal Vita: https://forum.devchroma.nl/index.php/topic,112.0.html

## Textures

Loading from FIOS2 overlay (for example, loading directly from PSARC archive) is supported. When calling vita2d_load_XXX_file() specify 1 for io_type to use FIOS2 or 0 to use SceIo. When using FIOS2, remember to specify your mounted path to the file instead of the actual file path. 

**- BMP textures can be used in all applications**

**- JPEG textures: Codec Engine decoder. Specify 0 for useMainMemory to use phycont memory or 1 to use main user memory**

```
vita2d_JPEG_decoder_initialize();
/* Load your JPEG textures here */
vita2d_JPEG_decoder_finish();
```

**- JPEG textures: ARM decoder.**

```
vita2d_JPEG_ARM_decoder_initialize();
/* Load your JPEG textures here */
vita2d_JPEG_ARM_decoder_finish();
```

**- PNG textures can only be used in applications with newlib or SceLibc heap available**

## Changing internal heap size

Default internal vita2d_sys heap size is set to 1MB. It can be changed with:
```
vita2d_set_heap_size(VITA2D_HEAP_SIZE);
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

## VITA2DLIB_SYS

Simple and Fast (using the GPU) 2D library for the PSVita with "system mode" applications support.

libvita2d_sys can be used in both "system mode" and "game mode" applications.  Appropriate GXM mode and framebuffer mode are set automatically when vita2d_init() is called.

## Usage

If your application is running in "system mode", you can control shared fb directly from your application using the following:
```
SceUID vita2d_get_shfbid(); //Returns shared fb id opened by application.
```

## Custom resolution

To set custom resolution, call vita2d_display_set_max_resolution() before initializing the library. This will set maximum resolution that application will be able to use.
After that vita2d_display_set_resolution() can be called at any time to change rendering resolution:
```
vita2d_display_set_max_resolution(1920, 1088);

vita2d_init();

vita2d_start_drawing();
.
.
vita2d_display_set_resolution(1280, 725); //This can be done at any time
.
.
vita2d_end_drawing();
vita2d_end_shfb();
```

Native 1280x725 and 1920x1088 resolutions are supported for PS TV.
Use Sharpscale to be able to use 1280x720 and 1920x1080 or for normal Vita: https://forum.devchroma.nl/index.php/topic,112.0.html

## Textures

Loading from FIOS2 overlay (for example, loading directly from PSARC archive) is supported. When calling vita2d_load_XXX_file() specify VITA2D_IO_TYPE_FIOS2 for io_type to use FIOS2 or VITA2D_IO_TYPE_NORMAL to use SceIo. When using FIOS2, remember to specify your mounted path to the file instead of the actual file path. 

**- JPEG textures: Codec Engine decoder**

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

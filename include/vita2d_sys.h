#ifndef VITA2D_H
#define VITA2D_H

#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/pgf.h>
#include <psp2/pvf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VITA2D_SYS_VERSION_INTERNAL 0141

#ifndef VITA2D_SYS_VERSION
#define VITA2D_SYS_VERSION VITA2D_SYS_VERSION_INTERNAL
#endif

//#define VITA2D_SYS_PRX

#if _SCE_TARGET_OS_PSP2
#ifdef VITA2D_SYS_PRX
#define PRX_INTERFACE __declspec (dllexport)
#endif
#else
#define PRX_INTERFACE
#endif

#define RGBA8(r,g,b,a) ((((a)&0xFF)<<24) | (((b)&0xFF)<<16) | (((g)&0xFF)<<8) | (((r)&0xFF)<<0))

typedef enum vita2d_io_type {
	VITA2D_IO_TYPE_NORMAL,
	VITA2D_IO_TYPE_FIOS2
} vita2d_io_type;

typedef struct vita2d_clear_vertex {
	float x;
	float y;
} vita2d_clear_vertex;

typedef struct vita2d_color_vertex {
	float x;
	float y;
	float z;
	unsigned int color;
} vita2d_color_vertex;

typedef struct vita2d_texture_vertex {
	float x;
	float y;
	float z;
	float u;
	float v;
} vita2d_texture_vertex;

typedef struct vita2d_texture {
	SceGxmTexture gxm_tex;
	SceUID data_UID;
	SceUID palette_UID;
	SceGxmRenderTarget *gxm_rtgt;
	SceGxmColorSurface gxm_sfc;
	SceGxmDepthStencilSurface gxm_sfd;
	SceUID depth_UID;
	void *gxt_data;
} vita2d_texture;

typedef struct vita2d_system_pgf_config {
	SceFontLanguageCode code;
	int (*in_font_group)(unsigned int c);
} vita2d_system_pgf_config;

typedef struct vita2d_system_pvf_config {
	ScePvfLanguageCode language;
	ScePvfFamilyCode family;
	ScePvfStyleCode style;
	int (*in_font_group)(unsigned int c);
} vita2d_system_pvf_config;

typedef struct vita2d_pgf vita2d_pgf;
typedef struct vita2d_pvf vita2d_pvf;

/*----------------------------------- general functions -----------------------------------*/

/**
 * Check version of vita2d_sys library.
 *
 * @return SCE_OK if versions match, -1 otherwise.
 */
#if VITA2D_SYS_VERSION < 0135
#define vita2d_check_version(vita2d_version) (0);
#else
#define vita2d_check_version(vita2d_version) (vita2d_version == VITA2D_SYS_VERSION_INTERNAL ? 0 : -1);
#endif

/**
 * Initialize vita2d_sys.
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_init();

/**
 * Initialize vita2d_sys with user-defined temp memory pool size.
 *
 * @param[in] temp_pool_size - vita2d_sys temp memory pool size (default is 1MB)
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_init_advanced(unsigned int temp_pool_size);

/**
 * Initialize vita2d_sys with user-defined temp memory pool size and MSAA mode.
 *
 * @param[in] temp_pool_size - vita2d_sys temp memory pool size (default is 1MB)
 * @param[in] msaa - one of ::SceGxmMultisampleMode
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_init_advanced_with_msaa(unsigned int temp_pool_size, SceGxmMultisampleMode msaa);

/**
 * Initialize vita2d_sys with user-defined temp memory pool size, MSAA mode and GXM context buffer sizes.
 *
 * @param[in] temp_pool_size - vita2d_sys temp memory pool size (default is 1MB)
 * @param[in] vdmRingBufferMemsize - GXM vdm ring buffer memory size (default is 128KB)
 * @param[in] vertexRingBufferMemsize - GXM vertex ring buffer memory size (default is 2MB)
 * @param[in] fragmentRingBufferMemsize - GXM fragment ring buffer memory size (default is 512KB)
 * @param[in] fragmentUsseRingBufferMemsize - GXM fragment USSE ring buffer memory size (default is 16KB)
 * @param[in] msaa - one of ::SceGxmMultisampleMode
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_init_with_msaa_and_memsize(unsigned int temp_pool_size, unsigned int vdmRingBufferMemsize, unsigned int vertexRingBufferMemsize,
	unsigned int fragmentRingBufferMemsize, unsigned int fragmentUsseRingBufferMemsize, SceGxmMultisampleMode msaa);

/**
 * Block thread execution until rendering is done.
 *
 */
PRX_INTERFACE void vita2d_wait_rendering_done();

/**
 * Finalize vita2d_sys.
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_fini();

/**
 * Clear screen and fill it with clear color. This function must be called on each new frame.
 *
 */
PRX_INTERFACE void vita2d_clear_screen();

/**
 * [SYSTEM MODE ONLY] Get shared framebuffer UID opened by vita2d_sys. 
 *
 * @return valid SceUID, <0 on error.
 */
PRX_INTERFACE SceUID vita2d_get_shfbid();

/**
 * [SYSTEM MODE] Signal to the system that vita2d_sys has finished drawing to shared framebuffer. Must be called after rendering is done on each frame.
 * [GAME MODE] Swap display buffers.
 *
 */
PRX_INTERFACE void vita2d_end_shfb();

/**
 * [GAME MODE ONLY] Set display/rendering resolution. Must be called before vita2d_init().
 *
 * @param[in] hRes - width in pixels
 * @param[in] vRes - height in pixels
 *
 */
PRX_INTERFACE void vita2d_display_set_resolution(int hRes, int vRes);

/**
 * Set internal heap size. Must be called before vita2d_init().
 *
 * @param[in] size - internal heap size (default is 1MB)
 *
 */
PRX_INTERFACE void vita2d_set_heap_size(unsigned int size);

/**
 * [SYSTEM MODE] Start drawing and signal to the system that vita2d_sys has started drawing to shared framebuffer. Must be called on each new frame.
 * [GAME MODE] Start drawing. Must be called on each new frame.
 *
 */
PRX_INTERFACE void vita2d_start_drawing();

/**
 * [SYSTEM MODE] Start drawing only for user-defined rendering target and signal to the system that vita2d_sys has started drawing to shared framebuffer. Must be called on each new frame.
 * [GAME MODE] Start drawing only for user-defined rendering target. Must be called on each new frame.
 *
 * @param[in] target - Drawing rendering target
 * @param[in] flags - flags passed to sceGxmBeginScene()
 *
 */
PRX_INTERFACE void vita2d_start_drawing_advanced(vita2d_texture *target, unsigned int flags);

/**
 * End drawing. Must be called on end of each frame.
 *
 */
PRX_INTERFACE void vita2d_end_drawing();

/**
 * Update common dialog. Must be called after vita2d_end_drawing() but before display buffers are swapped on the end of each frame.
 *
 */
PRX_INTERFACE int vita2d_common_dialog_update();

/**
 * Set clear screen color.
 *
 * @param[in] color - color in RGBA8 format.
 *
 */
PRX_INTERFACE void vita2d_set_clear_color(unsigned int color);

/**
 * Get clear screen color.
 *
 * @return clear screen color in RGBA8 format.
 */
PRX_INTERFACE unsigned int vita2d_get_clear_color();

/**
 * [SYSTEM MODE] Switch vsync mode (30/60 FPS).
 * [GAME MODE] Enable/disable vsync.
 *
 * @param[in] enable - 1 to enable, 0 to disable
 *
 */
PRX_INTERFACE void vita2d_set_vblank_wait(int enable);

/**
 * Get vita2d_sys GXM context.
 *
 * @return pointer to vita2d_sys GXM context.
 */
PRX_INTERFACE SceGxmContext *vita2d_get_context();

/**
 * Get vita2d_sys GXM shader patcher.
 *
 * @return pointer to vita2d_sys GXM shader patcher.
 */
PRX_INTERFACE SceGxmShaderPatcher *vita2d_get_shader_patcher();

/**
 * Get vita2d_sys linear indices.
 *
 * @return pointer to vita2d_sys linear indices.
 */
PRX_INTERFACE const uint16_t *vita2d_get_linear_indices();

/*-----------------------------------  region clipping (region-aligned) -----------------------------------*/

/**
 * Set clipping region size for drawing, region-aligned implementation.
 *
 * @param[in] mode - one of ::SceGxmRegionClipMode
 * @param[in] x_min - minimum x value of clipping region in pixels
 * @param[in] y_min - minimum y value of clipping region in pixels
 * @param[in] x_max - maximum x value of clipping region in pixels
 * @param[in] y_max - maximum y value of clipping region in pixels
 *
 */
PRX_INTERFACE void vita2d_set_region_clip(SceGxmRegionClipMode mode, unsigned int x_min, unsigned int y_min, unsigned int x_max, unsigned int y_max);

/*-----------------------------------  region clipping (pixel-aligned) -----------------------------------*/

/**
 * Enable clipping, pixel-aligned implementation.
 *
 */
PRX_INTERFACE void vita2d_enable_clipping();

/**
 * Disable clipping, pixel-aligned implementation.
 *
 */
PRX_INTERFACE void vita2d_disable_clipping();

/**
 * Get clipping status, pixel-aligned implementation.
 *
 * @return 1 if clipping is enabled, 0 otherwise.
 */
PRX_INTERFACE int vita2d_get_clipping_enabled();

/**
 * Set clipping region size for drawing, pixel-aligned implementation.
 *
 * @param[in] x_min - minimum x value of clipping region in pixels
 * @param[in] y_min - minimum y value of clipping region in pixels
 * @param[in] x_max - maximum x value of clipping region in pixels
 * @param[in] y_max - maximum y value of clipping region in pixels
 *
 */
PRX_INTERFACE void vita2d_set_clip_rectangle(int x_min, int y_min, int x_max, int y_max);

/**
 * Get clipping region size for drawing, pixel-aligned implementation.
 *
 * @param[out] x_min - minimum x value of clipping region in pixels
 * @param[out] y_min - minimum y value of clipping region in pixels
 * @param[out] x_max - maximum x value of clipping region in pixels
 * @param[out] y_max - maximum y value of clipping region in pixels
 *
 */
PRX_INTERFACE void vita2d_get_clip_rectangle(int *x_min, int *y_min, int *x_max, int *y_max);

/**
 * Enable/disable additive blend mode.
 *
 * @param[in] enable - 1 to enable, 0 to disable
 *
 */
PRX_INTERFACE void vita2d_set_blend_mode_add(int enable);

/*-----------------------------------  temp memory pool -----------------------------------*/

/**
 * Allocate memory block from vita2d_sys temp memory pool.
 *
 * @param[in] size - size of the memory block in bytes
 *
 * @return pointer to the allocated memory, NULL on error.
 */
PRX_INTERFACE void *vita2d_pool_malloc(unsigned int size);

/**
 * Allocate aligned memory block from vita2d_sys temp memory pool.
 *
 * @param[in] size - size of the memory block in bytes
 * @param[in] alignment - memory block alignment
 *
 * @return pointer to the allocated memory, NULL on error.
 */
PRX_INTERFACE void *vita2d_pool_memalign(unsigned int size, unsigned int alignment);

/**
 * Get size of free vita2d_sys temp memory pool.
 *
 * @return free temp memory pool size in bytes.
 */
PRX_INTERFACE unsigned int vita2d_pool_free_space();

/**
 * Reset vita2d_sys temp memory pool. All previous allocations will be invalidated.
 *
 */
PRX_INTERFACE void vita2d_pool_reset();

/*----------------------------------- general drawing functions -----------------------------------*/

/**
 * Draw single colored pixel.
 *
 * @param[in] x - x position of the pixel in pixel equivalent
 * @param[in] y - y position of the pixel in pixel equivalent
 * @param[in] color - color of the pixel in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_pixel(float x, float y, unsigned int color);

/**
 * Draw colored pixel line.
 *
 * @param[in] x0 - x beginning position of the line in pixel equivalent
 * @param[in] y0 - y beginning position of the line in pixel equivalent
 * @param[in] x1 - x ending position of the line in pixel equivalent
 * @param[in] y1 - y ending position of the line in pixel equivalent
 * @param[in] color - color of the line in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_line(float x0, float y0, float x1, float y1, unsigned int color);

/**
 * Draw colored rectangle.
 *
 * @param[in] x - x position of upper left corner of the rectangle in pixel equivalent
 * @param[in] y - y position of upper left corner of the rectangle in pixel equivalent
 * @param[in] w - width of the rectangle in pixel equivalent
 * @param[in] h - height of the rectangle in pixel equivalent
 * @param[in] color - color of the rectangle in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_rectangle(float x, float y, float w, float h, unsigned int color);

/**
 * Draw colored circle.
 *
 * @param[in] x - x position of center of the circle in pixel equivalent
 * @param[in] y - y position of center of the circle in pixel equivalent
 * @param[in] radius - radius of the circle in pixel equivalent
 * @param[in] color - color of the circle in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_fill_circle(float x, float y, float radius, unsigned int color);

/**
 * Draw colored polygon.
 *
 * @param[in] mode - one of ::SceGxmPrimitiveType
 * @param[in] vertices - pointer to the vertex stream data
 * @param[in] count - index count
 *
 */
PRX_INTERFACE void vita2d_draw_array(SceGxmPrimitiveType mode, const vita2d_color_vertex *vertices, size_t count);

/*----------------------------------- general texture functions -----------------------------------*/

/**
 * Set memblock type for texture memory.
 *
 * @param[in] type - one of ::SceKernelMemBlockType
 *
 */
PRX_INTERFACE void vita2d_texture_set_alloc_memblock_type(SceKernelMemBlockType type);

/**
 * Get memblock type for texture memory.
 *
 * @return memblock type, one of ::SceKernelMemBlockType.
 */
PRX_INTERFACE SceKernelMemBlockType vita2d_texture_get_alloc_memblock_type();

/**
 * Create empty texture with SCE_GXM_TEXTURE_FORMAT_A8B8G8R8 format.
 *
 * @param[in] w - texture width in pixels
 * @param[in] h - texture height in pixels
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_create_empty_texture(unsigned int w, unsigned int h);

/**
 * Create empty texture.
 *
 * @param[in] w - texture width in pixels
 * @param[in] h - texture height in pixels
 * @param[in] format - one of ::SceGxmTextureFormat
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_create_empty_texture_format(unsigned int w, unsigned int h, SceGxmTextureFormat format);

/**
 * Create empty texture for use as render target.
 *
 * @param[in] w - texture width in pixels
 * @param[in] h - texture height in pixels
 * @param[in] format - one of ::SceGxmTextureFormat
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_create_empty_texture_rendertarget(unsigned int w, unsigned int h, SceGxmTextureFormat format);

/**
 * Free all memory used by vita2d_sys texture and destroy it.
 *
 * @param[in] texture - pointer to ::vita2d_texture to free
 *
 */
PRX_INTERFACE void vita2d_free_texture(vita2d_texture *texture);

/**
 * Get texture width.
 *
 * @param[in] texture - pointer to ::vita2d_texture to get width for
 *
 * @return texture width in pixels, <0 on error.
 */
PRX_INTERFACE unsigned int vita2d_texture_get_width(const vita2d_texture *texture);

/**
 * Get texture height.
 *
 * @param[in] texture - pointer to ::vita2d_texture to get height for
 *
 * @return texture height in pixels, <0 on error.
 */
PRX_INTERFACE unsigned int vita2d_texture_get_height(const vita2d_texture *texture);

/**
 * Get texture stride.
 *
 * @param[in] texture - pointer to ::vita2d_texture to get stride for
 *
 * @return texture stride in pixels, <0 on error.
 */
PRX_INTERFACE unsigned int vita2d_texture_get_stride(const vita2d_texture *texture);

/**
 * Get texture format.
 *
 * @param[in] texture - pointer to ::vita2d_texture to get format for
 *
 * @return texture format, one of ::SceGxmTextureFormat, <0 on error.
 */
PRX_INTERFACE SceGxmTextureFormat vita2d_texture_get_format(const vita2d_texture *texture);

/**
 * Get pointer to the texture data memblock.
 *
 * @param[in] texture - pointer to ::vita2d_texture to get data pointer for
 *
 * @return valid data pointer, NULL on error.
 */
PRX_INTERFACE void *vita2d_texture_get_datap(const vita2d_texture *texture);

/**
 * Get pointer to the texture palette memblock.
 *
 * @param[in] texture - pointer to ::vita2d_texture to get palette pointer for
 *
 * @return valid palette pointer, NULL on error.
 */
PRX_INTERFACE void *vita2d_texture_get_palette(const vita2d_texture *texture);

/**
 * Get texture minification filter type.
 *
 * @param[in] texture - pointer to ::vita2d_texture to get minification filter type for
 *
 * @return texture format, one of ::SceGxmTextureFilter, <0 on error.
 */
PRX_INTERFACE SceGxmTextureFilter vita2d_texture_get_min_filter(const vita2d_texture *texture);

/**
 * Get texture magnification filter type.
 *
 * @param[in] texture - pointer to ::vita2d_texture to get magnification filter type for
 *
 * @return texture format, one of ::SceGxmTextureFilter, <0 on error.
 */
PRX_INTERFACE SceGxmTextureFilter vita2d_texture_get_mag_filter(const vita2d_texture *texture);

/**
 * Set minification and magnification filter types for the texture.
 *
 * @param[in] texture - pointer to ::vita2d_texture to set filter types for
 * @param[in] min_filter - minification filter type, one of ::SceGxmTextureFilter
 * @param[in] mag_filter - magnification filter type, one of ::SceGxmTextureFilter
 *
 */
PRX_INTERFACE void vita2d_texture_set_filters(vita2d_texture *texture, SceGxmTextureFilter min_filter, SceGxmTextureFilter mag_filter);

/*----------------------------------- texture drawing functions -----------------------------------*/

/**
 * Draw texture.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture in pixel equivalent
 *
 */
PRX_INTERFACE void vita2d_draw_texture(const vita2d_texture *texture, float x, float y);

/**
 * Draw rotated texture.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture in pixel equivalent
 * @param[in] rad - rotation of the texture in radians
 *
 */
PRX_INTERFACE void vita2d_draw_texture_rotate(const vita2d_texture *texture, float x, float y, float rad);

/**
 * Draw rotated texture with user-defined rotation hotspot.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] rad - rotation of the texture in radians
 * @param[in] center_x - x position of the hotspot in pixel equivalent
 * @param[in] center_y - y position of the hotspot in pixel equivalent
 *
 */
PRX_INTERFACE void vita2d_draw_texture_rotate_hotspot(const vita2d_texture *texture, float x, float y, float rad, float center_x, float center_y);

/**
 * Draw scaled texture.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture in pixel equivalent
 * @param[in] x_scale - scale of the texture for x axis
 * @param[in] y_scale - scale of the texture for y axis
 *
 */
PRX_INTERFACE void vita2d_draw_texture_scale(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale);

/**
 * Draw part of the texture.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture in pixel equivalent
 * @param[in] tex_x - x position of upper left corner of the drawing area related to x position of the texture in pixel equivalent
 * @param[in] tex_y - y position of upper left corner of the drawing area related to y position of the texture in pixel equivalent
 * @param[in] tex_w - width of the drawing area in pixel equivalent
 * @param[in] tex_h - height of the drawing area in pixel equivalent
 *
 */
PRX_INTERFACE void vita2d_draw_texture_part(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h);

/**
 * Draw part of the texture with scale.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture in pixel equivalent
 * @param[in] tex_x - x position of upper left corner of the drawing area related to x position of the texture in pixel equivalent
 * @param[in] tex_y - y position of upper left corner of the drawing area related to y position of the texture in pixel equivalent
 * @param[in] tex_w - width of the drawing area in pixel equivalent
 * @param[in] tex_h - height of the drawing area in pixel equivalent
 * @param[in] x_scale - scale of the drawing area for x axis
 * @param[in] y_scale - scale of the drawing area for y axis
 *
 */
PRX_INTERFACE void vita2d_draw_texture_part_scale(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale);

/**
 * Draw scaled and rotated texture with user-defined rotation hotspot.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] x_scale - scale of the texture for x axis
 * @param[in] y_scale - scale of the texture for y axis
 * @param[in] rad - rotation of the texture in radians
 * @param[in] center_x - x position of the hotspot in pixel equivalent
 * @param[in] center_y - y position of the hotspot in pixel equivalent
 *
 */
PRX_INTERFACE void vita2d_draw_texture_scale_rotate_hotspot(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, float rad, float center_x, float center_y);

/**
 * Draw scaled and rotated texture.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] x_scale - scale of the texture for x axis
 * @param[in] y_scale - scale of the texture for y axis
 * @param[in] rad - rotation of the texture in radians
 *
 */
PRX_INTERFACE void vita2d_draw_texture_scale_rotate(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, float rad);

/**
 * Draw scaled and rotated part of the texture.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] tex_x - x position of upper left corner of the drawing area related to x position of the texture in pixel equivalent
 * @param[in] tex_y - y position of upper left corner of the drawing area related to y position of the texture in pixel equivalent
 * @param[in] tex_w - width of the drawing area in pixel equivalent
 * @param[in] tex_h - height of the drawing area in pixel equivalent
 * @param[in] x_scale - scale of the texture for x axis
 * @param[in] y_scale - scale of the texture for y axis
 * @param[in] rad - rotation of the texture in radians
 *
 */
PRX_INTERFACE void vita2d_draw_texture_part_scale_rotate(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale, float rad);

/**
 * Draw texture with tint.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture in pixel equivalent
 * @param[in] color - tint color in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_texture_tint(const vita2d_texture *texture, float x, float y, unsigned int color);

/**
 * Draw rotated texture.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture in pixel equivalent
 * @param[in] rad - rotation of the texture in radians
 * @param[in] color - tint color in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_texture_tint_rotate(const vita2d_texture *texture, float x, float y, float rad, unsigned int color);

/**
 * Draw rotated texture with user-defined rotation hotspot.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] rad - rotation of the texture in radians
 * @param[in] center_x - x position of the hotspot in pixel equivalent
 * @param[in] center_y - y position of the hotspot in pixel equivalent
 * @param[in] color - tint color in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_texture_tint_rotate_hotspot(const vita2d_texture *texture, float x, float y, float rad, float center_x, float center_y, unsigned int color);

/**
 * Draw scaled texture.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture in pixel equivalent
 * @param[in] x_scale - scale of the texture for x axis
 * @param[in] y_scale - scale of the texture for y axis
 * @param[in] color - tint color in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_texture_tint_scale(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, unsigned int color);

/**
 * Draw part of the texture.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture in pixel equivalent
 * @param[in] tex_x - x position of upper left corner of the drawing area related to x position of the texture in pixel equivalent
 * @param[in] tex_y - y position of upper left corner of the drawing area related to y position of the texture in pixel equivalent
 * @param[in] tex_w - width of the drawing area in pixel equivalent
 * @param[in] tex_h - height of the drawing area in pixel equivalent
 * @param[in] color - tint color in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_texture_tint_part(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, unsigned int color);

/**
 * Draw part of the texture with scale.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture in pixel equivalent
 * @param[in] tex_x - x position of upper left corner of the drawing area related to x position of the texture in pixel equivalent
 * @param[in] tex_y - y position of upper left corner of the drawing area related to y position of the texture in pixel equivalent
 * @param[in] tex_w - width of the drawing area in pixel equivalent
 * @param[in] tex_h - height of the drawing area in pixel equivalent
 * @param[in] x_scale - scale of the drawing area for x axis
 * @param[in] y_scale - scale of the drawing area for y axis
 * @param[in] color - tint color in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_texture_tint_part_scale(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale, unsigned int color);

/**
 * Draw scaled and rotated texture with user-defined rotation hotspot.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] x_scale - scale of the texture for x axis
 * @param[in] y_scale - scale of the texture for y axis
 * @param[in] rad - rotation of the texture in radians
 * @param[in] center_x - x position of the hotspot in pixel equivalent
 * @param[in] center_y - y position of the hotspot in pixel equivalent
 * @param[in] color - tint color in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_texture_tint_scale_rotate_hotspot(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, float rad, float center_x, float center_y, unsigned int color);

/**
 * Draw scaled and rotated texture.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] x_scale - scale of the texture for x axis
 * @param[in] y_scale - scale of the texture for y axis
 * @param[in] rad - rotation of the texture in radians
 * @param[in] color - tint color in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_texture_tint_scale_rotate(const vita2d_texture *texture, float x, float y, float x_scale, float y_scale, float rad, unsigned int color);

/**
 * Draw scaled and rotated part of the texture.
 *
 * @param[in] texture - pointer to ::vita2d_texture to draw
 * @param[in] x - x position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] y - y position of upper left corner of the texture before rotation in pixel equivalent
 * @param[in] tex_x - x position of upper left corner of the drawing area related to x position of the texture in pixel equivalent
 * @param[in] tex_y - y position of upper left corner of the drawing area related to y position of the texture in pixel equivalent
 * @param[in] tex_w - width of the drawing area in pixel equivalent
 * @param[in] tex_h - height of the drawing area in pixel equivalent
 * @param[in] x_scale - scale of the texture for x axis
 * @param[in] y_scale - scale of the texture for y axis
 * @param[in] rad - rotation of the texture in radians
 * @param[in] color - tint color in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_texture_part_tint_scale_rotate(const vita2d_texture *texture, float x, float y, float tex_x, float tex_y, float tex_w, float tex_h, float x_scale, float y_scale, float rad, unsigned int color);

/**
 * Draw colored and textured polygon.
 *
 * @param[in] texture - pointer to ::vita2d_texture to use
 * @param[in] mode - one of ::SceGxmPrimitiveType
 * @param[in] vertices - pointer to the vertex stream data
 * @param[in] count - index count
 * @param[in] color - tint color in RGBA8 format
 *
 */
PRX_INTERFACE void vita2d_draw_array_textured(const vita2d_texture *texture, SceGxmPrimitiveType mode, const vita2d_texture_vertex *vertices, size_t count, unsigned int color);

/*----------------------------------- PNG functions -----------------------------------*/

/**
 * Create texture from PNG file.
 *
 * @param[in] filename - path to the PNG file
 * @param[in] io_type - one of ::vita2d_io_type
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_load_PNG_file(char *filename, vita2d_io_type io_type);

/**
 * Create texture from PNG buffer.
 *
 * @param[in] buffer - pointer to the buffer that holds PNG data
 * @param[in] buffer_size - size of the input buffer
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_load_PNG_buffer(const void *buffer);

/*----------------------------------- JPEG functions -----------------------------------*/

/**
 * Create texture from JPEG file using hardware decoder.
 *
 * @param[in] filename - path to the JPEG file
 * @param[in] io_type - one of ::vita2d_io_type
 * @param[in] useDownScale - set to 1 to use automatic hardware-accelerated downscaler
 * @param[in] downScalerHeight - downscaler height trigger in pixels. Will be ignored if useDownScale is set to 0
 * @param[in] downScalerWidth - downscaler width trigger in pixels. Will be ignored if useDownScale is set to 0
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_load_JPEG_file(char *filename, vita2d_io_type io_type, int useDownScale, int downScalerHeight, int downScalerWidth);

/**
 * Create texture from JPEG buffer using hardware decoder.
 *
 * @param[in] buffer - pointer to the buffer that holds JPEG data
 * @param[in] buffer_size - size of the input buffer
 * @param[in] useDownScale - set to 1 to use automatic hardware-accelerated downscaler
 * @param[in] downScalerHeight - downscaler height trigger in pixels. Will be ignored if useDownScale is set to 0
 * @param[in] downScalerWidth - downscaler width trigger in pixels. Will be ignored if useDownScale is set to 0
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_load_JPEG_buffer(const void *buffer, unsigned long buffer_size, int useDownScale, int downScalerHeight, int downScalerWidth);

/**
 * Initialize hardware JPEG decoder. Can be called once per application lifetime.
 *
 * @param[in] usePhyContMemory - set to 1 to use phycont memory instead of cdram
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_JPEG_decoder_initialize(int usePhyContMemory);

/**
 * Finalize hardware JPEG decoder.
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_JPEG_decoder_finish(void);

/**
 * Create texture from JPEG file using software decoder.
 *
 * @param[in] filename - path to the JPEG file
 * @param[in] io_type - one of ::vita2d_io_type
 * @param[in] useDownScale - set to 1 to use automatic downscaler
 * @param[in] downScalerHeight - downscaler height trigger in pixels. Will be ignored if useDownScale is set to 0
 * @param[in] downScalerWidth - downscaler width trigger in pixels. Will be ignored if useDownScale is set to 0
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_load_JPEG_ARM_file(char *filename, vita2d_io_type io_type, int useDownScale, int downScalerHeight, int downScalerWidth);

/**
 * Create texture from JPEG buffer using software decoder.
 *
 * @param[in] buffer - pointer to the buffer that holds JPEG data
 * @param[in] buffer_size - size of the input buffer
 * @param[in] useDownScale - set to 1 to use automatic software downscaler
 * @param[in] downScalerHeight - downscaler height trigger in pixels. Will be ignored if useDownScale is set to 0
 * @param[in] downScalerWidth - downscaler width trigger in pixels. Will be ignored if useDownScale is set to 0
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_load_JPEG_ARM_buffer(const void *buffer, unsigned long buffer_size, int useDownScale, int downScalerHeight, int downScalerWidth);

/**
 * Initialize hardware JPEG decoder. Can be called once per application lifetime.
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_JPEG_ARM_decoder_initialize(void);

/**
 * Finalize software JPEG decoder.
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_JPEG_ARM_decoder_finish(void);

/*----------------------------------- BMP functions -----------------------------------*/

/**
 * Create texture from BMP file.
 *
 * @param[in] filename - path to the BMP file
 * @param[in] io_type - one of ::vita2d_io_type
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_load_BMP_file(char *filename, vita2d_io_type io_type);

/**
 * Create texture from BMP buffer.
 *
 * @param[in] buffer - pointer to the buffer that holds BMP data
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_load_BMP_buffer(const void *buffer);

/*----------------------------------- GXT functions -----------------------------------*/

/**
 * Load texture from GXT file.
 *
 * @param[in] filename - path to the GXT file
 * @param[in] texture_index - index of the initial texture in the GXT file (usually 0)
 * @param[in] io_type - one of ::vita2d_io_type
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_load_GXT_file(char *filename, int texture_index, vita2d_io_type io_type);

/**
 * Load additional texture from GXT file.
 *
 * @param[in] initial_tex - pointer to ::vita2d_texture that holds initial (index 0) GXT texture
 * @param[in] texture_index - index of the texture in the GXT file
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_load_additional_GXT(vita2d_texture *initial_tex, int texture_index);

/**
 * Free additional GXT texture. Fast alternative to vita2d_free_texture() for GXT textures.
 *
 * @param[in] tex - pointer to ::vita2d_texture to free
 *
 */
PRX_INTERFACE void vita2d_free_additional_GXT(vita2d_texture *tex);

/*----------------------------------- GIM functions -----------------------------------*/

/**
 * Load texture from GIM file.
 *
 * @param[in] filename - path to the GIM file
 * @param[in] io_type - one of ::vita2d_io_type
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_load_GIM_file(char *filename, vita2d_io_type io_type);

/**
 * Load texture from GIM buffer.
 *
 * @param[in] buffer - pointer to the buffer that holds GIM data
 *
 * @return pointer to ::vita2d_texture, NULL on error.
 */
PRX_INTERFACE vita2d_texture *vita2d_load_GIM_buffer(void *buffer);

/*----------------------------------- PGF font functions -----------------------------------*/

/**
 * Load built-in system PGF font based on user-defined config.
 *
 * @param[in] numFonts - number of the fonts to load. Currently has no effect
 * @param[in] configs - pointer to ::vita2d_system_pgf_config font config
 *
 * @return pointer to ::vita2d_pgf, NULL on error.
 */
PRX_INTERFACE vita2d_pgf *vita2d_load_system_pgf(int numFonts, const vita2d_system_pgf_config *configs);

/**
 * Load default built-in system PGF font.
 *
 * @return pointer to ::vita2d_pgf, NULL on error.
 */
PRX_INTERFACE vita2d_pgf *vita2d_load_default_pgf();

/**
 * Load custom PGF font.
 *
 * @param[in] path - path to the PGF font file
 *
 * @return pointer to ::vita2d_pgf, NULL on error.
 */
PRX_INTERFACE vita2d_pgf *vita2d_load_custom_pgf(const char *path);

/**
 * Load custom PGF font buffer.
 *
 * @param[in] buf - pointer to the buffer that holds PGF font data
 * @param[in] bufSize - input buffer size
 *
 * @return pointer to ::vita2d_pgf, NULL on error.
 */
PRX_INTERFACE vita2d_pgf *vita2d_load_custom_pgf_buffer(void* buf, SceSize bufSize);

/**
 * Free all memory used by PGF font and destroy it.
 *
 * @param[in] font - pointer to ::vita2d_pgf to free
 *
 */
PRX_INTERFACE void vita2d_free_pgf(vita2d_pgf *font);

/**
 * Draw PGF text.
 *
 * @param[in] font - pointer to ::vita2d_pgf to use
 * @param[in] x - x position of upper left corner of the text texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the text texture in pixel equivalent
 * @param[in] color - color of the text in RGBA8 format
 * @param[in] scale - scale of the text
 * @param[in] text - text string to draw
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_pgf_draw_text(vita2d_pgf *font, int x, int y, unsigned int color, float scale, const char *text);

/**
 * Draw PGF text with user-defined linespace.
 *
 * @param[in] font - pointer to ::vita2d_pgf to use
 * @param[in] x - x position of upper left corner of the text texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the text texture in pixel equivalent
 * @param[in] linespace - linespace in pixel equivalent
 * @param[in] color - color of the text in RGBA8 format
 * @param[in] scale - scale of the text
 * @param[in] text - text string to draw
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_pgf_draw_text_ls(vita2d_pgf *font, int x, int y, float linespace, unsigned int color, float scale, const char *text);

/**
 * Draw PGF text in printf format string.
 *
 * @param[in] font - pointer to ::vita2d_pgf to use
 * @param[in] x - x position of upper left corner of the text texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the text texture in pixel equivalent
 * @param[in] color - color of the text in RGBA8 format
 * @param[in] scale - scale of the text
 * @param[in] text - text string to draw
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_pgf_draw_textf(vita2d_pgf *font, int x, int y, unsigned int color, float scale, const char *text, ...);

/**
 * Draw PGF text with user-defined linespace printf format string.
 *
 * @param[in] font - pointer to ::vita2d_pgf to use
 * @param[in] x - x position of upper left corner of the text texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the text texture in pixel equivalent
 * @param[in] linespace - linespace in pixel equivalent
 * @param[in] color - color of the text in RGBA8 format
 * @param[in] scale - scale of the text
 * @param[in] text - text string to draw
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_pgf_draw_textf_ls(vita2d_pgf *font, int x, int y, float linespace, unsigned int color, float scale, const char *text, ...);

/**
 * Get PGF text drawing dimensions.
 *
 * @param[in] font - pointer to ::vita2d_pgf to use
 * @param[in] scale - scale of the text
 * @param[in] text - text string to get dimensions for
 * @param[out] width - width of the resulting text in pixels
 * @param[out] height - height of the resulting text in pixels
 *
 */
PRX_INTERFACE void vita2d_pgf_text_dimensions(vita2d_pgf *font, float scale, const char *text, int *width, int *height);

/**
 * Get PGF text drawing width.
 *
 * @param[in] font - pointer to ::vita2d_pgf to use
 * @param[in] scale - scale of the text
 * @param[in] text - text string to get dimensions for
 *
 * @return width of the resulting text in pixels, <0 on error.
 */
PRX_INTERFACE int vita2d_pgf_text_width(vita2d_pgf *font, float scale, const char *text);

/**
 * Get PGF text drawing height.
 *
 * @param[in] font - pointer to ::vita2d_pgf to use
 * @param[in] scale - scale of the text
 * @param[in] text - text string to get dimensions for
 *
 * @return height of the resulting text in pixels, <0 on error.
 */
PRX_INTERFACE int vita2d_pgf_text_height(vita2d_pgf *font, float scale, const char *text);

/*----------------------------------- PVF/generic font functions -----------------------------------*/

/**
 * Load built-in system PVF font based on user-defined config.
 *
 * @param[in] numFonts - number of the fonts to load. Currently has no effect
 * @param[in] configs - pointer to ::vita2d_system_pvf_config font config
 * @param[in] hSize - glyph rendering width
 * @param[in] vSize - glyph rendering height
 *
 * @return pointer to ::vita2d_pvf, NULL on error.
 */
PRX_INTERFACE vita2d_pvf *vita2d_load_system_pvf(int numFonts, const vita2d_system_pvf_config *configs, float hSize, float vSize);

/**
 * Load built-in system PVF font to shared memory based on user-defined config.
 *
 * @param[in] numFonts - number of the fonts to load. Currently has no effect
 * @param[in] configs - pointer to ::vita2d_system_pvf_config font config
 * @param[in] hSize - glyph rendering width
 * @param[in] vSize - glyph rendering height
 *
 * @return pointer to ::vita2d_pvf, NULL on error.
 */
PRX_INTERFACE vita2d_pvf *vita2d_load_system_shared_pvf(int numFonts, const vita2d_system_pvf_config *configs, float hSize, float vSize);

/**
 * Load default built-in system PVF font to shared memory.
 *
 * @return pointer to ::vita2d_pvf, NULL on error.
 */
vita2d_pvf *vita2d_load_default_pvf();

/**
 * Load custom PVF, TTF or OTF font.
 *
 * @param[in] path - path to the PVF, TTF or OTF font file
 * @param[in] hSize - glyph rendering width
 * @param[in] vSize - glyph rendering height
 *
 * @return pointer to ::vita2d_pvf, NULL on error.
 */
PRX_INTERFACE vita2d_pvf *vita2d_load_custom_pvf(const char *path, float hSize, float vSize);

/**
 * Load custom PVF, TTF or OTF font.
 *
 * @param[in] buf - pointer to the buffer that holds PVF, TTF or OTF font data
 * @param[in] bufSize - input buffer size
 * @param[in] hSize - glyph rendering width
 * @param[in] vSize - glyph rendering height
 *
 * @return pointer to ::vita2d_pvf, NULL on error.
 */
PRX_INTERFACE vita2d_pvf *vita2d_load_custom_pvf_buffer(void* buf, SceSize bufSize, float hSize, float vSize);

/**
 * Free all memory used by PVF font and destroy it.
 *
 * @param[in] font - pointer to ::vita2d_pvf to free
 *
 */
PRX_INTERFACE void vita2d_free_pvf(vita2d_pvf *font);

/**
 * Draw PVF text.
 *
 * @param[in] font - pointer to ::vita2d_pvf to use
 * @param[in] x - x position of upper left corner of the text texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the text texture in pixel equivalent
 * @param[in] color - color of the text in RGBA8 format
 * @param[in] scale - scale of the text
 * @param[in] text - text string to draw
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_pvf_draw_text(vita2d_pvf *font, int x, int y, unsigned int color, float scale, const char *text);

/**
 * Draw PVF text in printf format string.
 *
 * @param[in] font - pointer to ::vita2d_pvf to use
 * @param[in] x - x position of upper left corner of the text texture in pixel equivalent
 * @param[in] y - y position of upper left corner of the text texture in pixel equivalent
 * @param[in] color - color of the text in RGBA8 format
 * @param[in] scale - scale of the text
 * @param[in] text - text string to draw
 *
 * @return SCE_OK, <0 on error.
 */
PRX_INTERFACE int vita2d_pvf_draw_textf(vita2d_pvf *font, int x, int y, unsigned int color, float scale, const char *text, ...);

/**
 * Get PVF text drawing dimensions.
 *
 * @param[in] font - pointer to ::vita2d_pvf to use
 * @param[in] scale - scale of the text
 * @param[in] text - text string to get dimensions for
 * @param[out] width - width of the resulting text in pixels
 * @param[out] height - height of the resulting text in pixels
 *
 */
PRX_INTERFACE void vita2d_pvf_text_dimensions(vita2d_pvf *font, float scale, const char *text, int *width, int *height);

/**
 * Get PVF text drawing width.
 *
 * @param[in] font - pointer to ::vita2d_pvf to use
 * @param[in] scale - scale of the text
 * @param[in] text - text string to get dimensions for
 *
 * @return width of the resulting text in pixels, <0 on error.
 */
PRX_INTERFACE int vita2d_pvf_text_width(vita2d_pvf *font, float scale, const char *text);

/**
 * Get PVF text drawing height.
 *
 * @param[in] font - pointer to ::vita2d_pvf to use
 * @param[in] scale - scale of the text
 * @param[in] text - text string to get dimensions for
 *
 * @return height of the resulting text in pixels, <0 on error.
 */
PRX_INTERFACE int vita2d_pvf_text_height(vita2d_pvf *font, float scale, const char *text);

/**
 * Get maximum estimated height of PVF font glyph.
 *
 * @param[in] font - pointer to ::vita2d_pvf to use
 *
 * @return height in pixels, <0 on error.
 */
PRX_INTERFACE int vita2d_pvf_irect_maxheight(vita2d_pvf *font);

/**
 * Get maximum estimated width of PVF font glyph.
 *
 * @param[in] font - pointer to ::vita2d_pvf to use
 *
 * @return width in pixels, <0 on error.
 */
PRX_INTERFACE int vita2d_pvf_irect_maxwidth(vita2d_pvf *font);

/**
 * Set embolden (or unembolden) rate of PVF font glyph.
 *
 * @param[in] font - pointer to ::vita2d_pvf to use
 * @param[in] em - embolden rate value
 *
 */
PRX_INTERFACE void vita2d_pvf_embolden_rate(vita2d_pvf *font, float em);

/**
 * Set skew value of PVF font glyph.
 *
 * @param[in] font - pointer to ::vita2d_pvf to use
 * @param[in] ax - angle of x-axis defined by horizontal line in degrees
 * @param[in] ay - angle of y-axis defined by vertical line in degrees
 *
 */
PRX_INTERFACE void vita2d_pvf_skew_rate(vita2d_pvf *font, float ax, float ay);

/**
 * Set rendering size of PVF font glyph.
 *
 * @param[in] font - pointer to ::vita2d_pvf to use
 * @param[in] hs - glyph rendering width
 * @param[in] vs - glyph rendering height
 *
 */
PRX_INTERFACE void vita2d_pvf_char_size(vita2d_pvf *font, float hs, float vs);

/**
 * Set PVF font linespace.
 *
 * @param[in] font - pointer to ::vita2d_pvf to use
 * @param[in] ls - linespace in pixel equivalent
 *
 */
PRX_INTERFACE void vita2d_pvf_linespace(vita2d_pvf *font, float ls);

/**
 * Set PVF font character space.
 *
 * @param[in] font - pointer to ::vita2d_pvf to use
 * @param[in] cs - character space in pixel equivalent
 *
 */
PRX_INTERFACE void vita2d_pvf_charspace(vita2d_pvf *font, float cs);


#if VITA2D_SYS_VERSION < 0134
/* DEPRECATED, do not use in new projects */

/**
 * [DEPRECATED, DO NOT USE].
 *
 */
PRX_INTERFACE void vita2d_clib_pass_mspace(void* space);

/**
 * [DEPRECATED, DO NOT USE].
 *
 */
PRX_INTERFACE int vita2d_pvf_draw_text_ls(vita2d_pvf *font, int x, int y, float linespace, unsigned int color, float scale, const char *text);

/**
 * [DEPRECATED, DO NOT USE].
 *
 */
PRX_INTERFACE int vita2d_pvf_draw_textf_ls(vita2d_pvf *font, int x, int y, float linespace, unsigned int color, float scale, const char *text, ...);
#endif

#ifdef __cplusplus
}
#endif

#endif

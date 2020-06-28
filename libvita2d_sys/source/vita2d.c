#include <psp2/display.h>
#include <psp2/gxm.h>
#include <psp2/types.h>
#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/clib.h>
#include <psp2/kernel/threadmgr.h> 
#include <psp2/message_dialog.h>
#include <psp2/sysmodule.h>
#include <psp2/appmgr.h>
#include <psp2/scebase.h>
#include "vita2d_sys.h"
#include "utils.h"

#ifdef DEBUG_BUILD
#  define DEBUG(...) sceClibPrintf(__VA_ARGS__)
#else
#  define DEBUG(...)
#endif

/* Defines */

#define DISPLAY_COLOR_FORMAT		SCE_GXM_COLOR_FORMAT_A8B8G8R8
#define DISPLAY_PIXEL_FORMAT		SCE_DISPLAY_PIXELFORMAT_A8B8G8R8
#define DISPLAY_BUFFER_COUNT		2
#define DISPLAY_MAX_PENDING_SWAPS	1
#define DEFAULT_TEMP_POOL_SIZE		(1 * 1024 * 1024)

typedef struct SceSharedFbInfo { // size is 0x58
	void* base1;		// cdram base
	int memsize;
	void* base2;		// cdram base
	int unk_0C;
	void *unk_10;
	int unk_14;
	int unk_18;
	int unk_1C;
	int unk_20;
	int unk_24;		// 960
	int unk_28;		// 960
	int unk_2C;		// 544
	int unk_30;
	int curbuf;
	int unk_38;
	int unk_3C;
	int unk_40;
	int unk_44;
	int owner;
	int unk_4C;
	int unk_50;
	int unk_54;
} SceSharedFbInfo;

typedef struct vita2d_display_data {
	void *address;
} vita2d_display_data;

/* SharedFb */

SceUID _sceSharedFbOpen(int a1, int sysver);
int sceSharedFbClose(SceUID shared_fb_id);
int sceSharedFbBegin(SceUID shared_fb_id, SceSharedFbInfo *info);
int sceSharedFbEnd(SceUID shared_fb_id);
int sceSharedFbGetInfo(SceUID shared_fb_id, SceSharedFbInfo *info);

/* Extern */

extern const SceGxmProgram clear_v_gxp_start;
extern const SceGxmProgram clear_f_gxp_start;
extern const SceGxmProgram color_v_gxp_start;
extern const SceGxmProgram color_f_gxp_start;
extern const SceGxmProgram texture_v_gxp_start;
extern const SceGxmProgram texture_f_gxp_start;
extern const SceGxmProgram texture_tint_f_gxp_start;

/* Static variables */

static SceSharedFbInfo info;

static const SceGxmProgram *const clearVertexProgramGxp         = &clear_v_gxp_start;
static const SceGxmProgram *const clearFragmentProgramGxp       = &clear_f_gxp_start;
static const SceGxmProgram *const colorVertexProgramGxp         = &color_v_gxp_start;
static const SceGxmProgram *const colorFragmentProgramGxp       = &color_f_gxp_start;
static const SceGxmProgram *const textureVertexProgramGxp       = &texture_v_gxp_start;
static const SceGxmProgram *const textureFragmentProgramGxp     = &texture_f_gxp_start;
static const SceGxmProgram *const textureTintFragmentProgramGxp = &texture_tint_f_gxp_start;

static int display_hres = 960;
static int display_vres = 544;
static int display_stride = 960;

static int vita2d_initialized = 0;
static float clear_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
static unsigned int clear_color_u = 0xFF000000;
static int clip_rect_x_min = 0;
static int clip_rect_y_min = 0;
static int clip_rect_x_max = 960; //<- change this
static int clip_rect_y_max = 544; //<- change this
static int vblank_wait = 1;
static int drawing = 0;
static int clipping_enabled = 0;

static SceUID renderTargetMemUid;
static SceUID vdmRingBufferUid;
static SceUID vertexRingBufferUid;
static SceUID fragmentRingBufferUid;
static SceUID fragmentUsseRingBufferUid;
static SceUID shfb_id;

static SceGxmContextParams contextParams;
static SceGxmRenderTarget *renderTarget = NULL;
static void *displayBufferData[DISPLAY_BUFFER_COUNT];
static SceGxmColorSurface displaySurface[DISPLAY_BUFFER_COUNT];
static SceGxmSyncObject *displayBufferSync[DISPLAY_BUFFER_COUNT];

static SceUID displayBufferUid[DISPLAY_BUFFER_COUNT];
static void *displayBufferData[DISPLAY_BUFFER_COUNT];

static int bufferIndex = 1;

static SceUID depthBufferUid;
static SceUID stencilBufferUid;
static SceGxmDepthStencilSurface depthSurface;
static void *depthBufferData = NULL;
static void *stencilBufferData = NULL;

static SceGxmShaderPatcher *shaderPatcher = NULL;
static SceGxmVertexProgram *clearVertexProgram = NULL;
static SceGxmFragmentProgram *clearFragmentProgram = NULL;

static SceGxmShaderPatcherId clearVertexProgramId;
static SceGxmShaderPatcherId clearFragmentProgramId;
static SceGxmShaderPatcherId colorVertexProgramId;
static SceGxmShaderPatcherId colorFragmentProgramId;
static SceGxmShaderPatcherId textureVertexProgramId;
static SceGxmShaderPatcherId textureFragmentProgramId;
static SceGxmShaderPatcherId textureTintFragmentProgramId;

static SceUID patcherBufferUid;
static SceUID patcherVertexUsseUid;
static SceUID patcherFragmentUsseUid;

static SceUID clearVerticesUid;
static SceUID linearIndicesUid;
static vita2d_clear_vertex *clearVertices = NULL;
static uint16_t *linearIndices = NULL;

/* Shared with other .c */
void* mspace_internal;
int system_mode_flag = 1;
int pgf_module_was_loaded = 10;
float _vita2d_ortho_matrix[4*4];
SceGxmContext *_vita2d_context = NULL;
SceGxmVertexProgram *_vita2d_colorVertexProgram = NULL;
SceGxmFragmentProgram *_vita2d_colorFragmentProgram = NULL;
SceGxmVertexProgram *_vita2d_textureVertexProgram = NULL;
SceGxmFragmentProgram *_vita2d_textureFragmentProgram = NULL;
SceGxmFragmentProgram *_vita2d_textureTintFragmentProgram = NULL;
const SceGxmProgramParameter *_vita2d_clearClearColorParam = NULL;
const SceGxmProgramParameter *_vita2d_colorWvpParam = NULL;
const SceGxmProgramParameter *_vita2d_textureWvpParam = NULL;
const SceGxmProgramParameter *_vita2d_textureTintColorParam = NULL;

typedef struct vita2d_fragment_programs {
	SceGxmFragmentProgram *color;
	SceGxmFragmentProgram *texture;
	SceGxmFragmentProgram *textureTint;
} vita2d_fragment_programs;

struct {
	vita2d_fragment_programs blend_mode_normal;
	vita2d_fragment_programs blend_mode_add;
} _vita2d_fragmentPrograms;

// Temporary memory pool
static void *pool_addr = NULL;
static SceUID poolUid;
static unsigned int pool_index = 0;
static unsigned int pool_size = 0;

/* Static functions */

static void *patcher_host_alloc(void *user_data, uint32_t size)
{
	return sceClibMspaceMalloc(mspace_internal, size);
}

static void patcher_host_free(void *user_data, void *mem)
{
	sceClibMspaceFree(mspace_internal, mem);
}

static void _vita2d_free_fragment_programs(vita2d_fragment_programs *out)
{
	sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, out->color);
	sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, out->texture);
	sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, out->textureTint);
}

static void _vita2d_make_fragment_programs(vita2d_fragment_programs *out,
	const SceGxmBlendInfo *blend_info, SceGxmMultisampleMode msaa)
{
	int err;
	(void)err;

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		colorFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa,
		blend_info,
		colorVertexProgramGxp,
		&out->color);

	DEBUG("color sceGxmShaderPatcherCreateFragmentProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		textureFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa,
		blend_info,
		textureVertexProgramGxp,
		&out->texture);

	DEBUG("texture sceGxmShaderPatcherCreateFragmentProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		textureTintFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa,
		blend_info,
		textureVertexProgramGxp,
		&out->textureTint);

	DEBUG("texture_tint sceGxmShaderPatcherCreateFragmentProgram(): 0x%08X\n", err);
}

static void display_callback(const void *callback_data)
{
	SceDisplayFrameBuf framebuf;
	const vita2d_display_data *display_data = (const vita2d_display_data *)callback_data;

	sceClibMemset(&framebuf, 0x00, sizeof(SceDisplayFrameBuf));
	framebuf.size = sizeof(SceDisplayFrameBuf);
	framebuf.base = display_data->address;
	framebuf.pitch = display_stride;
	framebuf.pixelformat = DISPLAY_PIXEL_FORMAT;
	framebuf.width = display_hres;
	framebuf.height = display_vres;
	sceDisplaySetFrameBuf(&framebuf, SCE_DISPLAY_SETBUF_NEXTFRAME);

	if (vblank_wait) {
		sceDisplayWaitVblankStart();
	}
}

static int vita2d_init_internal_for_system(unsigned int temp_pool_size, unsigned int vdmRingBufferMemsize, unsigned int vertexRingBufferMemsize,
	unsigned int fragmentRingBufferMemsize, unsigned int fragmentUsseRingBufferMemsize, SceGxmMultisampleMode msaa)
{
	int err;
	unsigned int i;
	UNUSED(err);

	SceGxmInitializeParams gxm_init_params_internal;
	sceClibMemset(&gxm_init_params_internal, 0, sizeof(SceGxmInitializeParams));
	gxm_init_params_internal.flags = 0x0A;
	gxm_init_params_internal.displayQueueMaxPendingCount = DISPLAY_MAX_PENDING_SWAPS;
	gxm_init_params_internal.parameterBufferSize = 0x200000;

	err = sceGxmVshInitialize(&gxm_init_params_internal);
	DEBUG("sceGxmVshInitialize(): 0x%08X\n", err);

	while (1) {
		shfb_id = _sceSharedFbOpen(1, SCE_PSP2_SDK_VERSION);
		sceSharedFbGetInfo(shfb_id, &info);
		if (info.curbuf == 1)
			sceSharedFbClose(shfb_id);
		else
			break;
	}

	vita2d_texture_set_alloc_memblock_type(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE);

	err = sceGxmMapMemory(info.base1, info.memsize, SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE);
	DEBUG("SHAREDFB: sceGxmMapMemory(): 0x%08X\n", err);

	displayBufferData[0] = info.base1;
	displayBufferData[1] = info.base2;

	// allocate ring buffer memory using default sizes
	void *vdmRingBuffer = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		vdmRingBufferMemsize,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&vdmRingBufferUid);

	void *vertexRingBuffer = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		vertexRingBufferMemsize,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&vertexRingBufferUid);

	void *fragmentRingBuffer = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		fragmentRingBufferMemsize,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&fragmentRingBufferUid);

	uint32_t fragmentUsseRingBufferOffset;
	void *fragmentUsseRingBuffer = fragment_usse_alloc(
		fragmentUsseRingBufferMemsize,
		&fragmentUsseRingBufferUid,
		&fragmentUsseRingBufferOffset);

	sceClibMemset(&contextParams, 0, sizeof(SceGxmContextParams));
	contextParams.hostMem = sceClibMspaceMalloc(mspace_internal, SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
	contextParams.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	contextParams.vdmRingBufferMem = vdmRingBuffer;
	contextParams.vdmRingBufferMemSize = vdmRingBufferMemsize;
	contextParams.vertexRingBufferMem = vertexRingBuffer;
	contextParams.vertexRingBufferMemSize = vertexRingBufferMemsize;
	contextParams.fragmentRingBufferMem = fragmentRingBuffer;
	contextParams.fragmentRingBufferMemSize = fragmentRingBufferMemsize;
	contextParams.fragmentUsseRingBufferMem = fragmentUsseRingBuffer;
	contextParams.fragmentUsseRingBufferMemSize = fragmentUsseRingBufferMemsize;
	contextParams.fragmentUsseRingBufferOffset = fragmentUsseRingBufferOffset;

	err = sceGxmCreateContext(&contextParams, &_vita2d_context);
	DEBUG("sceGxmCreateContext(): 0x%08X\n", err);

	// set up parameters
	SceGxmRenderTargetParams renderTargetParams;
	sceClibMemset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
	renderTargetParams.flags = 0;
	renderTargetParams.width = display_hres;
	renderTargetParams.height = display_vres;
	renderTargetParams.scenesPerFrame = 1;
	renderTargetParams.multisampleMode = msaa;
	renderTargetParams.multisampleLocations = 0;
	renderTargetParams.driverMemBlock = -1; // Invalid UID

	// allocate target memblock
	uint32_t targetMemsize;
	sceGxmGetRenderTargetMemSize(&renderTargetParams, &targetMemsize);
	renderTargetMemUid = sceKernelAllocMemBlock("render_target_mem", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, targetMemsize, NULL);
	renderTargetParams.driverMemBlock = renderTargetMemUid;

	// create the render target
	err = sceGxmCreateRenderTarget(&renderTargetParams, &renderTarget);
	DEBUG("sceGxmCreateRenderTarget(): 0x%08X\n", err);

	// allocate memory and sync objects for display buffers
	for (i = 0; i < DISPLAY_BUFFER_COUNT; i++) {

		// initialize a color surface for this display buffer
		err = sceGxmColorSurfaceInit(
			&displaySurface[i],
			DISPLAY_COLOR_FORMAT,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			(msaa == SCE_GXM_MULTISAMPLE_NONE) ? SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			display_hres,
			display_vres,
			display_stride,
			displayBufferData[i]);

		// create a sync object that we will associate with this buffer
		err = sceGxmSyncObjectCreate(&displayBufferSync[i]);
	}

	// compute the memory footprint of the depth buffer
	const unsigned int alignedWidth = ALIGN(display_hres, SCE_GXM_TILE_SIZEX);
	const unsigned int alignedHeight = ALIGN(display_vres, SCE_GXM_TILE_SIZEY);
	unsigned int sampleCount = alignedWidth * alignedHeight;
	unsigned int depthStrideInSamples = alignedWidth;
	if (msaa == SCE_GXM_MULTISAMPLE_4X) {
		// samples increase in X and Y
		sampleCount *= 4;
		depthStrideInSamples *= 2;
	}
	else if (msaa == SCE_GXM_MULTISAMPLE_2X) {
		// samples increase in Y only
		sampleCount *= 2;
	}

	// allocate the depth buffer
	depthBufferData = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		4 * sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&depthBufferUid);

	// allocate the stencil buffer
	stencilBufferData = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		4 * sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&stencilBufferUid);

	// create the SceGxmDepthStencilSurface structure
	err = sceGxmDepthStencilSurfaceInit(
		&depthSurface,
		SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
		SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
		depthStrideInSamples,
		depthBufferData,
		stencilBufferData);

	// set the stencil test reference (this is currently assumed to always remain 1 after here for region clipping)
	sceGxmSetFrontStencilRef(_vita2d_context, 1);
	// set the stencil function (this wouldn't actually be needed, as the set clip rectangle function has to call this at the begginning of every scene)
	sceGxmSetFrontStencilFunc(
		_vita2d_context,
		SCE_GXM_STENCIL_FUNC_ALWAYS,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		0xFF,
		0xFF);

	// set buffer sizes for this sample
	const unsigned int patcherBufferSize = 64 * 1024;
	const unsigned int patcherVertexUsseSize = 64 * 1024;
	const unsigned int patcherFragmentUsseSize = 64 * 1024;

	// allocate memory for buffers and USSE code
	void *patcherBuffer = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		patcherBufferSize,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&patcherBufferUid);

	uint32_t patcherVertexUsseOffset;
	void *patcherVertexUsse = vertex_usse_alloc(
		patcherVertexUsseSize,
		&patcherVertexUsseUid,
		&patcherVertexUsseOffset);

	uint32_t patcherFragmentUsseOffset;
	void *patcherFragmentUsse = fragment_usse_alloc(
		patcherFragmentUsseSize,
		&patcherFragmentUsseUid,
		&patcherFragmentUsseOffset);

	// create a shader patcher
	SceGxmShaderPatcherParams patcherParams;
	sceClibMemset(&patcherParams, 0, sizeof(SceGxmShaderPatcherParams));
	patcherParams.userData = NULL;
	patcherParams.hostAllocCallback = &patcher_host_alloc;
	patcherParams.hostFreeCallback = &patcher_host_free;
	patcherParams.bufferAllocCallback = NULL;
	patcherParams.bufferFreeCallback = NULL;
	patcherParams.bufferMem = patcherBuffer;
	patcherParams.bufferMemSize = patcherBufferSize;
	patcherParams.vertexUsseAllocCallback = NULL;
	patcherParams.vertexUsseFreeCallback = NULL;
	patcherParams.vertexUsseMem = patcherVertexUsse;
	patcherParams.vertexUsseMemSize = patcherVertexUsseSize;
	patcherParams.vertexUsseOffset = patcherVertexUsseOffset;
	patcherParams.fragmentUsseAllocCallback = NULL;
	patcherParams.fragmentUsseFreeCallback = NULL;
	patcherParams.fragmentUsseMem = patcherFragmentUsse;
	patcherParams.fragmentUsseMemSize = patcherFragmentUsseSize;
	patcherParams.fragmentUsseOffset = patcherFragmentUsseOffset;

	err = sceGxmShaderPatcherCreate(&patcherParams, &shaderPatcher);
	DEBUG("sceGxmShaderPatcherCreate(): 0x%08X\n", err);

	// check the shaders
	err = sceGxmProgramCheck(clearVertexProgramGxp);
	DEBUG("clear_v sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(clearFragmentProgramGxp);
	DEBUG("clear_f sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(colorVertexProgramGxp);
	DEBUG("color_v sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(colorFragmentProgramGxp);
	DEBUG("color_f sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(textureVertexProgramGxp);
	DEBUG("texture_v sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(textureFragmentProgramGxp);
	DEBUG("texture_f sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(textureTintFragmentProgramGxp);
	DEBUG("texture_tint_f sceGxmProgramCheck(): 0x%08X\n", err);

	// register programs with the patcher
	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, clearVertexProgramGxp, &clearVertexProgramId);
	DEBUG("clear_v sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, clearFragmentProgramGxp, &clearFragmentProgramId);
	DEBUG("clear_f sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, colorVertexProgramGxp, &colorVertexProgramId);
	DEBUG("color_v sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, colorFragmentProgramGxp, &colorFragmentProgramId);
	DEBUG("color_f sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureVertexProgramGxp, &textureVertexProgramId);
	DEBUG("texture_v sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureFragmentProgramGxp, &textureFragmentProgramId);
	DEBUG("texture_f sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureTintFragmentProgramGxp, &textureTintFragmentProgramId);
	DEBUG("texture_tint_f sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	// Fill SceGxmBlendInfo
	static const SceGxmBlendInfo blend_info = {
		.colorFunc = SCE_GXM_BLEND_FUNC_ADD,
		.alphaFunc = SCE_GXM_BLEND_FUNC_ADD,
		.colorSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
		.colorDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.alphaSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
		.alphaDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorMask = SCE_GXM_COLOR_MASK_ALL
	};

	static const SceGxmBlendInfo blend_info_add = {
		.colorFunc = SCE_GXM_BLEND_FUNC_ADD,
		.alphaFunc = SCE_GXM_BLEND_FUNC_ADD,
		.colorSrc = SCE_GXM_BLEND_FACTOR_ONE,
		.colorDst = SCE_GXM_BLEND_FACTOR_ONE,
		.alphaSrc = SCE_GXM_BLEND_FACTOR_ONE,
		.alphaDst = SCE_GXM_BLEND_FACTOR_ONE,
		.colorMask = SCE_GXM_COLOR_MASK_ALL
	};

	// get attributes by name to create vertex format bindings
	const SceGxmProgramParameter *paramClearPositionAttribute = sceGxmProgramFindParameterByName(clearVertexProgramGxp, "aPosition");

	// create clear vertex format
	SceGxmVertexAttribute clearVertexAttributes[1];
	SceGxmVertexStream clearVertexStreams[1];
	clearVertexAttributes[0].streamIndex = 0;
	clearVertexAttributes[0].offset = 0;
	clearVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	clearVertexAttributes[0].componentCount = 2;
	clearVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramClearPositionAttribute);
	clearVertexStreams[0].stride = sizeof(vita2d_clear_vertex);
	clearVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create clear programs
	err = sceGxmShaderPatcherCreateVertexProgram(
		shaderPatcher,
		clearVertexProgramId,
		clearVertexAttributes,
		1,
		clearVertexStreams,
		1,
		&clearVertexProgram);

	DEBUG("clear sceGxmShaderPatcherCreateVertexProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		clearFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa,
		NULL,
		clearVertexProgramGxp,
		&clearFragmentProgram);

	DEBUG("clear sceGxmShaderPatcherCreateFragmentProgram(): 0x%08X\n", err);

	// create the clear triangle vertex/index data
	clearVertices = (vita2d_clear_vertex *)gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		3 * sizeof(vita2d_clear_vertex),
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&clearVerticesUid);

	// Allocate a 64k * 2 bytes = 128 KiB buffer and store all possible
	// 16-bit indices in linear ascending order, so we can use this for
	// all drawing operations where we don't want to use indexing.
	linearIndices = (uint16_t *)gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		UINT16_MAX * sizeof(uint16_t),
		sizeof(uint16_t),
		SCE_GXM_MEMORY_ATTRIB_READ,
		&linearIndicesUid);

	// Range of i must be greater than uint16_t, this doesn't endless-loop
	for (uint32_t i = 0; i <= UINT16_MAX; ++i) {
		linearIndices[i] = i;
	}

	clearVertices[0].x = -1.0f;
	clearVertices[0].y = -1.0f;
	clearVertices[1].x = 3.0f;
	clearVertices[1].y = -1.0f;
	clearVertices[2].x = -1.0f;
	clearVertices[2].y = 3.0f;

	const SceGxmProgramParameter *paramColorPositionAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aPosition");
	DEBUG("aPosition sceGxmProgramFindParameterByName(): %p\n", paramColorPositionAttribute);

	const SceGxmProgramParameter *paramColorColorAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aColor");
	DEBUG("aColor sceGxmProgramFindParameterByName(): %p\n", paramColorColorAttribute);

	// create color vertex format
	SceGxmVertexAttribute colorVertexAttributes[2];
	SceGxmVertexStream colorVertexStreams[1];
	/* x,y,z: 3 float 32 bits */
	colorVertexAttributes[0].streamIndex = 0;
	colorVertexAttributes[0].offset = 0;
	colorVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	colorVertexAttributes[0].componentCount = 3; // (x, y, z)
	colorVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorPositionAttribute);
	/* color: 4 unsigned char  = 32 bits */
	colorVertexAttributes[1].streamIndex = 0;
	colorVertexAttributes[1].offset = 12; // (x, y, z) * 4 = 12 bytes
	colorVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	colorVertexAttributes[1].componentCount = 4; // (color)
	colorVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorColorAttribute);
	// 16 bit (short) indices
	colorVertexStreams[0].stride = sizeof(vita2d_color_vertex);
	colorVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create color shaders
	err = sceGxmShaderPatcherCreateVertexProgram(
		shaderPatcher,
		colorVertexProgramId,
		colorVertexAttributes,
		2,
		colorVertexStreams,
		1,
		&_vita2d_colorVertexProgram);

	DEBUG("color sceGxmShaderPatcherCreateVertexProgram(): 0x%08X\n", err);


	const SceGxmProgramParameter *paramTexturePositionAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aPosition");
	DEBUG("aPosition sceGxmProgramFindParameterByName(): %p\n", paramTexturePositionAttribute);

	const SceGxmProgramParameter *paramTextureTexcoordAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aTexcoord");
	DEBUG("aTexcoord sceGxmProgramFindParameterByName(): %p\n", paramTextureTexcoordAttribute);

	// create texture vertex format
	SceGxmVertexAttribute textureVertexAttributes[2];
	SceGxmVertexStream textureVertexStreams[1];
	/* x,y,z: 3 float 32 bits */
	textureVertexAttributes[0].streamIndex = 0;
	textureVertexAttributes[0].offset = 0;
	textureVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	textureVertexAttributes[0].componentCount = 3; // (x, y, z)
	textureVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramTexturePositionAttribute);
	/* u,v: 2 floats 32 bits */
	textureVertexAttributes[1].streamIndex = 0;
	textureVertexAttributes[1].offset = 12; // (x, y, z) * 4 = 12 bytes
	textureVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	textureVertexAttributes[1].componentCount = 2; // (u, v)
	textureVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramTextureTexcoordAttribute);
	// 16 bit (short) indices
	textureVertexStreams[0].stride = sizeof(vita2d_texture_vertex);
	textureVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create texture shaders
	err = sceGxmShaderPatcherCreateVertexProgram(
		shaderPatcher,
		textureVertexProgramId,
		textureVertexAttributes,
		2,
		textureVertexStreams,
		1,
		&_vita2d_textureVertexProgram);

	DEBUG("texture sceGxmShaderPatcherCreateVertexProgram(): 0x%08X\n", err);

	// Create variations of the fragment program based on blending mode
	_vita2d_make_fragment_programs(&_vita2d_fragmentPrograms.blend_mode_normal, &blend_info, msaa);
	_vita2d_make_fragment_programs(&_vita2d_fragmentPrograms.blend_mode_add, &blend_info_add, msaa);

	// Default to "normal" blending mode (non-additive)
	vita2d_set_blend_mode_add(0);

	// find vertex uniforms by name and cache parameter information
	_vita2d_clearClearColorParam = sceGxmProgramFindParameterByName(clearFragmentProgramGxp, "uClearColor");
	DEBUG("_vita2d_clearClearColorParam sceGxmProgramFindParameterByName(): %p\n", _vita2d_clearClearColorParam);

	_vita2d_colorWvpParam = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "wvp");
	DEBUG("color wvp sceGxmProgramFindParameterByName(): %p\n", _vita2d_colorWvpParam);

	_vita2d_textureWvpParam = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "wvp");
	DEBUG("texture wvp sceGxmProgramFindParameterByName(): %p\n", _vita2d_textureWvpParam);

	_vita2d_textureTintColorParam = sceGxmProgramFindParameterByName(textureTintFragmentProgramGxp, "uTintColor");
	DEBUG("texture wvp sceGxmProgramFindParameterByName(): %p\n", _vita2d_textureWvpParam);

	// Allocate memory for the memory pool
	pool_size = temp_pool_size;
	pool_addr = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
		pool_size,
		sizeof(void *),
		SCE_GXM_MEMORY_ATTRIB_READ,
		&poolUid);


	matrix_init_orthographic(_vita2d_ortho_matrix, 0.0f, display_hres, display_vres, 0.0f, 0.0f, 1.0f);

	vita2d_initialized = 1;
	return 1;
}

static int vita2d_init_internal_for_game(unsigned int temp_pool_size, unsigned int vdmRingBufferMemsize, unsigned int vertexRingBufferMemsize,
	unsigned int fragmentRingBufferMemsize, unsigned int fragmentUsseRingBufferMemsize, SceGxmMultisampleMode msaa)
{
	int err;
	unsigned int i, x, y;
	UNUSED(err);

	SceGxmInitializeParams initializeParams;
	sceClibMemset(&initializeParams, 0, sizeof(SceGxmInitializeParams));
	initializeParams.flags = 0;
	initializeParams.displayQueueMaxPendingCount = 2;
	initializeParams.displayQueueCallback = display_callback;
	initializeParams.displayQueueCallbackDataSize = sizeof(vita2d_display_data);
	initializeParams.parameterBufferSize = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;

	err = sceGxmInitialize(&initializeParams);
	DEBUG("sceGxmInitialize(): 0x%08X\n", err);

	// allocate ring buffer memory using default sizes
	void *vdmRingBuffer = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		vdmRingBufferMemsize,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&vdmRingBufferUid);

	void *vertexRingBuffer = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		vertexRingBufferMemsize,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&vertexRingBufferUid);

	void *fragmentRingBuffer = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		fragmentRingBufferMemsize,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&fragmentRingBufferUid);

	uint32_t fragmentUsseRingBufferOffset;
	void *fragmentUsseRingBuffer = fragment_usse_alloc(
		fragmentUsseRingBufferMemsize,
		&fragmentUsseRingBufferUid,
		&fragmentUsseRingBufferOffset);

	sceClibMemset(&contextParams, 0, sizeof(SceGxmContextParams));
	contextParams.hostMem = sceClibMspaceMalloc(mspace_internal, SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
	contextParams.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	contextParams.vdmRingBufferMem = vdmRingBuffer;
	contextParams.vdmRingBufferMemSize = vdmRingBufferMemsize;
	contextParams.vertexRingBufferMem = vertexRingBuffer;
	contextParams.vertexRingBufferMemSize = vertexRingBufferMemsize;
	contextParams.fragmentRingBufferMem = fragmentRingBuffer;
	contextParams.fragmentRingBufferMemSize = fragmentRingBufferMemsize;
	contextParams.fragmentUsseRingBufferMem = fragmentUsseRingBuffer;
	contextParams.fragmentUsseRingBufferMemSize = fragmentUsseRingBufferMemsize;
	contextParams.fragmentUsseRingBufferOffset = fragmentUsseRingBufferOffset;

	err = sceGxmCreateContext(&contextParams, &_vita2d_context);
	DEBUG("sceGxmCreateContext(): 0x%08X\n", err);

	// set up parameters
	SceGxmRenderTargetParams renderTargetParams;
	sceClibMemset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
	renderTargetParams.flags = 0;
	renderTargetParams.width = display_hres;
	renderTargetParams.height = display_vres;
	renderTargetParams.scenesPerFrame = 1;
	renderTargetParams.multisampleMode = msaa;
	renderTargetParams.multisampleLocations = 0;
	renderTargetParams.driverMemBlock = -1; // Invalid UID

	// allocate target memblock
	uint32_t targetMemsize;
	sceGxmGetRenderTargetMemSize(&renderTargetParams, &targetMemsize);
	renderTargetMemUid = sceKernelAllocMemBlock("render_target_mem", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, targetMemsize, NULL);
	renderTargetParams.driverMemBlock = renderTargetMemUid;

	// create the render target
	err = sceGxmCreateRenderTarget(&renderTargetParams, &renderTarget);
	DEBUG("sceGxmCreateRenderTarget(): 0x%08X\n", err);

	// allocate memory and sync objects for display buffers
	for (i = 0; i < DISPLAY_BUFFER_COUNT; i++) {
		// allocate memory for display
		displayBufferData[i] = gpu_alloc(
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			4 * display_stride*display_vres,
			SCE_GXM_COLOR_SURFACE_ALIGNMENT,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
			&displayBufferUid[i]);

		// memset the buffer to black
		for (y = 0; y < display_vres; y++) {
			unsigned int *row = (unsigned int *)displayBufferData[i] + y * display_stride;
			for (x = 0; x < display_hres; x++) {
				row[x] = 0xff000000;
			}
		}

		// initialize a color surface for this display buffer
		err = sceGxmColorSurfaceInit(
			&displaySurface[i],
			DISPLAY_COLOR_FORMAT,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			(msaa == SCE_GXM_MULTISAMPLE_NONE) ? SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			display_hres,
			display_vres,
			display_stride,
			displayBufferData[i]);

		// create a sync object that we will associate with this buffer
		err = sceGxmSyncObjectCreate(&displayBufferSync[i]);
	}

	// compute the memory footprint of the depth buffer
	const unsigned int alignedWidth = ALIGN(display_hres, SCE_GXM_TILE_SIZEX);
	const unsigned int alignedHeight = ALIGN(display_vres, SCE_GXM_TILE_SIZEY);
	unsigned int sampleCount = alignedWidth * alignedHeight;
	unsigned int depthStrideInSamples = alignedWidth;
	if (msaa == SCE_GXM_MULTISAMPLE_4X) {
		// samples increase in X and Y
		sampleCount *= 4;
		depthStrideInSamples *= 2;
	}
	else if (msaa == SCE_GXM_MULTISAMPLE_2X) {
		// samples increase in Y only
		sampleCount *= 2;
	}

	// allocate the depth buffer
	depthBufferData = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		4 * sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&depthBufferUid);

	// allocate the stencil buffer
	stencilBufferData = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		4 * sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&stencilBufferUid);

	// create the SceGxmDepthStencilSurface structure
	err = sceGxmDepthStencilSurfaceInit(
		&depthSurface,
		SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
		SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
		depthStrideInSamples,
		depthBufferData,
		stencilBufferData);

	// set the stencil test reference (this is currently assumed to always remain 1 after here for region clipping)
	sceGxmSetFrontStencilRef(_vita2d_context, 1);
	// set the stencil function (this wouldn't actually be needed, as the set clip rectangle function has to call this at the begginning of every scene)
	sceGxmSetFrontStencilFunc(
		_vita2d_context,
		SCE_GXM_STENCIL_FUNC_ALWAYS,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		SCE_GXM_STENCIL_OP_KEEP,
		0xFF,
		0xFF);

	// set buffer sizes for this sample
	const unsigned int patcherBufferSize = 64 * 1024;
	const unsigned int patcherVertexUsseSize = 64 * 1024;
	const unsigned int patcherFragmentUsseSize = 64 * 1024;

	// allocate memory for buffers and USSE code
	void *patcherBuffer = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		patcherBufferSize,
		4,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		&patcherBufferUid);

	uint32_t patcherVertexUsseOffset;
	void *patcherVertexUsse = vertex_usse_alloc(
		patcherVertexUsseSize,
		&patcherVertexUsseUid,
		&patcherVertexUsseOffset);

	uint32_t patcherFragmentUsseOffset;
	void *patcherFragmentUsse = fragment_usse_alloc(
		patcherFragmentUsseSize,
		&patcherFragmentUsseUid,
		&patcherFragmentUsseOffset);

	// create a shader patcher
	SceGxmShaderPatcherParams patcherParams;
	sceClibMemset(&patcherParams, 0, sizeof(SceGxmShaderPatcherParams));
	patcherParams.userData = NULL;
	patcherParams.hostAllocCallback = &patcher_host_alloc;
	patcherParams.hostFreeCallback = &patcher_host_free;
	patcherParams.bufferAllocCallback = NULL;
	patcherParams.bufferFreeCallback = NULL;
	patcherParams.bufferMem = patcherBuffer;
	patcherParams.bufferMemSize = patcherBufferSize;
	patcherParams.vertexUsseAllocCallback = NULL;
	patcherParams.vertexUsseFreeCallback = NULL;
	patcherParams.vertexUsseMem = patcherVertexUsse;
	patcherParams.vertexUsseMemSize = patcherVertexUsseSize;
	patcherParams.vertexUsseOffset = patcherVertexUsseOffset;
	patcherParams.fragmentUsseAllocCallback = NULL;
	patcherParams.fragmentUsseFreeCallback = NULL;
	patcherParams.fragmentUsseMem = patcherFragmentUsse;
	patcherParams.fragmentUsseMemSize = patcherFragmentUsseSize;
	patcherParams.fragmentUsseOffset = patcherFragmentUsseOffset;

	err = sceGxmShaderPatcherCreate(&patcherParams, &shaderPatcher);
	DEBUG("sceGxmShaderPatcherCreate(): 0x%08X\n", err);

	// check the shaders
	err = sceGxmProgramCheck(clearVertexProgramGxp);
	DEBUG("clear_v sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(clearFragmentProgramGxp);
	DEBUG("clear_f sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(colorVertexProgramGxp);
	DEBUG("color_v sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(colorFragmentProgramGxp);
	DEBUG("color_f sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(textureVertexProgramGxp);
	DEBUG("texture_v sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(textureFragmentProgramGxp);
	DEBUG("texture_f sceGxmProgramCheck(): 0x%08X\n", err);
	err = sceGxmProgramCheck(textureTintFragmentProgramGxp);
	DEBUG("texture_tint_f sceGxmProgramCheck(): 0x%08X\n", err);

	// register programs with the patcher
	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, clearVertexProgramGxp, &clearVertexProgramId);
	DEBUG("clear_v sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, clearFragmentProgramGxp, &clearFragmentProgramId);
	DEBUG("clear_f sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, colorVertexProgramGxp, &colorVertexProgramId);
	DEBUG("color_v sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, colorFragmentProgramGxp, &colorFragmentProgramId);
	DEBUG("color_f sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureVertexProgramGxp, &textureVertexProgramId);
	DEBUG("texture_v sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureFragmentProgramGxp, &textureFragmentProgramId);
	DEBUG("texture_f sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureTintFragmentProgramGxp, &textureTintFragmentProgramId);
	DEBUG("texture_tint_f sceGxmShaderPatcherRegisterProgram(): 0x%08X\n", err);

	// Fill SceGxmBlendInfo
	static const SceGxmBlendInfo blend_info = {
		.colorFunc = SCE_GXM_BLEND_FUNC_ADD,
		.alphaFunc = SCE_GXM_BLEND_FUNC_ADD,
		.colorSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
		.colorDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.alphaSrc = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
		.alphaDst = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
		.colorMask = SCE_GXM_COLOR_MASK_ALL
	};

	static const SceGxmBlendInfo blend_info_add = {
		.colorFunc = SCE_GXM_BLEND_FUNC_ADD,
		.alphaFunc = SCE_GXM_BLEND_FUNC_ADD,
		.colorSrc = SCE_GXM_BLEND_FACTOR_ONE,
		.colorDst = SCE_GXM_BLEND_FACTOR_ONE,
		.alphaSrc = SCE_GXM_BLEND_FACTOR_ONE,
		.alphaDst = SCE_GXM_BLEND_FACTOR_ONE,
		.colorMask = SCE_GXM_COLOR_MASK_ALL
	};

	// get attributes by name to create vertex format bindings
	const SceGxmProgramParameter *paramClearPositionAttribute = sceGxmProgramFindParameterByName(clearVertexProgramGxp, "aPosition");

	// create clear vertex format
	SceGxmVertexAttribute clearVertexAttributes[1];
	SceGxmVertexStream clearVertexStreams[1];
	clearVertexAttributes[0].streamIndex = 0;
	clearVertexAttributes[0].offset = 0;
	clearVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	clearVertexAttributes[0].componentCount = 2;
	clearVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramClearPositionAttribute);
	clearVertexStreams[0].stride = sizeof(vita2d_clear_vertex);
	clearVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create clear programs
	err = sceGxmShaderPatcherCreateVertexProgram(
		shaderPatcher,
		clearVertexProgramId,
		clearVertexAttributes,
		1,
		clearVertexStreams,
		1,
		&clearVertexProgram);

	DEBUG("clear sceGxmShaderPatcherCreateVertexProgram(): 0x%08X\n", err);

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		clearFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa,
		NULL,
		clearVertexProgramGxp,
		&clearFragmentProgram);

	DEBUG("clear sceGxmShaderPatcherCreateFragmentProgram(): 0x%08X\n", err);

	// create the clear triangle vertex/index data
	clearVertices = (vita2d_clear_vertex *)gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		3 * sizeof(vita2d_clear_vertex),
		4,
		SCE_GXM_MEMORY_ATTRIB_READ,
		&clearVerticesUid);

	// Allocate a 64k * 2 bytes = 128 KiB buffer and store all possible
	// 16-bit indices in linear ascending order, so we can use this for
	// all drawing operations where we don't want to use indexing.
	linearIndices = (uint16_t *)gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE,
		UINT16_MAX * sizeof(uint16_t),
		sizeof(uint16_t),
		SCE_GXM_MEMORY_ATTRIB_READ,
		&linearIndicesUid);

	// Range of i must be greater than uint16_t, this doesn't endless-loop
	for (uint32_t i = 0; i <= UINT16_MAX; ++i) {
		linearIndices[i] = i;
	}

	clearVertices[0].x = -1.0f;
	clearVertices[0].y = -1.0f;
	clearVertices[1].x = 3.0f;
	clearVertices[1].y = -1.0f;
	clearVertices[2].x = -1.0f;
	clearVertices[2].y = 3.0f;

	const SceGxmProgramParameter *paramColorPositionAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aPosition");
	DEBUG("aPosition sceGxmProgramFindParameterByName(): %p\n", paramColorPositionAttribute);

	const SceGxmProgramParameter *paramColorColorAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aColor");
	DEBUG("aColor sceGxmProgramFindParameterByName(): %p\n", paramColorColorAttribute);

	// create color vertex format
	SceGxmVertexAttribute colorVertexAttributes[2];
	SceGxmVertexStream colorVertexStreams[1];
	/* x,y,z: 3 float 32 bits */
	colorVertexAttributes[0].streamIndex = 0;
	colorVertexAttributes[0].offset = 0;
	colorVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	colorVertexAttributes[0].componentCount = 3; // (x, y, z)
	colorVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorPositionAttribute);
	/* color: 4 unsigned char  = 32 bits */
	colorVertexAttributes[1].streamIndex = 0;
	colorVertexAttributes[1].offset = 12; // (x, y, z) * 4 = 12 bytes
	colorVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_U8N;
	colorVertexAttributes[1].componentCount = 4; // (color)
	colorVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramColorColorAttribute);
	// 16 bit (short) indices
	colorVertexStreams[0].stride = sizeof(vita2d_color_vertex);
	colorVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create color shaders
	err = sceGxmShaderPatcherCreateVertexProgram(
		shaderPatcher,
		colorVertexProgramId,
		colorVertexAttributes,
		2,
		colorVertexStreams,
		1,
		&_vita2d_colorVertexProgram);

	DEBUG("color sceGxmShaderPatcherCreateVertexProgram(): 0x%08X\n", err);


	const SceGxmProgramParameter *paramTexturePositionAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aPosition");
	DEBUG("aPosition sceGxmProgramFindParameterByName(): %p\n", paramTexturePositionAttribute);

	const SceGxmProgramParameter *paramTextureTexcoordAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aTexcoord");
	DEBUG("aTexcoord sceGxmProgramFindParameterByName(): %p\n", paramTextureTexcoordAttribute);

	// create texture vertex format
	SceGxmVertexAttribute textureVertexAttributes[2];
	SceGxmVertexStream textureVertexStreams[1];
	/* x,y,z: 3 float 32 bits */
	textureVertexAttributes[0].streamIndex = 0;
	textureVertexAttributes[0].offset = 0;
	textureVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	textureVertexAttributes[0].componentCount = 3; // (x, y, z)
	textureVertexAttributes[0].regIndex = sceGxmProgramParameterGetResourceIndex(paramTexturePositionAttribute);
	/* u,v: 2 floats 32 bits */
	textureVertexAttributes[1].streamIndex = 0;
	textureVertexAttributes[1].offset = 12; // (x, y, z) * 4 = 12 bytes
	textureVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
	textureVertexAttributes[1].componentCount = 2; // (u, v)
	textureVertexAttributes[1].regIndex = sceGxmProgramParameterGetResourceIndex(paramTextureTexcoordAttribute);
	// 16 bit (short) indices
	textureVertexStreams[0].stride = sizeof(vita2d_texture_vertex);
	textureVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

	// create texture shaders
	err = sceGxmShaderPatcherCreateVertexProgram(
		shaderPatcher,
		textureVertexProgramId,
		textureVertexAttributes,
		2,
		textureVertexStreams,
		1,
		&_vita2d_textureVertexProgram);

	DEBUG("texture sceGxmShaderPatcherCreateVertexProgram(): 0x%08X\n", err);

	// Create variations of the fragment program based on blending mode
	_vita2d_make_fragment_programs(&_vita2d_fragmentPrograms.blend_mode_normal, &blend_info, msaa);
	_vita2d_make_fragment_programs(&_vita2d_fragmentPrograms.blend_mode_add, &blend_info_add, msaa);

	// Default to "normal" blending mode (non-additive)
	vita2d_set_blend_mode_add(0);

	// find vertex uniforms by name and cache parameter information
	_vita2d_clearClearColorParam = sceGxmProgramFindParameterByName(clearFragmentProgramGxp, "uClearColor");
	DEBUG("_vita2d_clearClearColorParam sceGxmProgramFindParameterByName(): %p\n", _vita2d_clearClearColorParam);

	_vita2d_colorWvpParam = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "wvp");
	DEBUG("color wvp sceGxmProgramFindParameterByName(): %p\n", _vita2d_colorWvpParam);

	_vita2d_textureWvpParam = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "wvp");
	DEBUG("texture wvp sceGxmProgramFindParameterByName(): %p\n", _vita2d_textureWvpParam);

	_vita2d_textureTintColorParam = sceGxmProgramFindParameterByName(textureTintFragmentProgramGxp, "uTintColor");
	DEBUG("texture wvp sceGxmProgramFindParameterByName(): %p\n", _vita2d_textureWvpParam);

	// Allocate memory for the memory pool
	pool_size = temp_pool_size;
	pool_addr = gpu_alloc(
		SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
		pool_size,
		sizeof(void *),
		SCE_GXM_MEMORY_ATTRIB_READ,
		&poolUid);


	matrix_init_orthographic(_vita2d_ortho_matrix, 0.0f, display_hres, display_vres, 0.0f, 0.0f, 1.0f);

	vita2d_initialized = 1;
	return 1;
}

static int vita2d_init_internal(unsigned int temp_pool_size, unsigned int vdmRingBufferMemsize, unsigned int vertexRingBufferMemsize,
	unsigned int fragmentRingBufferMemsize, unsigned int fragmentUsseRingBufferMemsize, SceGxmMultisampleMode msaa)
{
	int err;

	if (vita2d_initialized) {
		DEBUG("libvita2d is already initialized!\n");
		return 1;
	}

	//Check application mode (this call returns error if app is in "game mode")
	void* dummy_struct = sceClibMspaceMalloc(mspace_internal, 0x88);
	*(SceSize*)dummy_struct = 0x88;
	err = sceAppMgrGetBudgetInfo(dummy_struct);
	if (err < 0)
		system_mode_flag = 0;
	sceClibMspaceFree(mspace_internal, dummy_struct);
	DEBUG("system_mode_flag: %d\n", system_mode_flag);

	if (system_mode_flag)
		return vita2d_init_internal_for_system(temp_pool_size, vdmRingBufferMemsize, vertexRingBufferMemsize, fragmentRingBufferMemsize, 
			fragmentUsseRingBufferMemsize, msaa);
	else
		return vita2d_init_internal_for_game(temp_pool_size, vdmRingBufferMemsize, vertexRingBufferMemsize, fragmentRingBufferMemsize, 
			fragmentUsseRingBufferMemsize, msaa);
}

void vita2d_display_set_resolution(int hRes, int vRes) 
{
	display_hres = hRes;
	display_vres = vRes;
	display_stride = hRes;
}

int vita2d_init()
{
	return vita2d_init_internal(DEFAULT_TEMP_POOL_SIZE, SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE, SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
		SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE, SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE, SCE_GXM_MULTISAMPLE_NONE);
}

int vita2d_init_advanced(unsigned int temp_pool_size)
{
	return vita2d_init_internal(temp_pool_size, SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE, SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE,
		SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE, SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE, SCE_GXM_MULTISAMPLE_NONE);
}

int vita2d_init_advanced_with_msaa(unsigned int temp_pool_size, SceGxmMultisampleMode msaa)
{
	return vita2d_init_internal(temp_pool_size, SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE, SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE, 
		SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE, SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE, msaa);
}

int vita2d_init_with_msaa_and_memsize(unsigned int temp_pool_size, unsigned int vdmRingBufferMemsize, unsigned int vertexRingBufferMemsize, 
	unsigned int fragmentRingBufferMemsize, unsigned int fragmentUsseRingBufferMemsize, SceGxmMultisampleMode msaa)
{
	if (!temp_pool_size)
		return vita2d_init_internal(DEFAULT_TEMP_POOL_SIZE, vdmRingBufferMemsize, vertexRingBufferMemsize, fragmentRingBufferMemsize, fragmentUsseRingBufferMemsize, msaa);
	else
		return vita2d_init_internal(temp_pool_size, vdmRingBufferMemsize, vertexRingBufferMemsize, fragmentRingBufferMemsize, fragmentUsseRingBufferMemsize, msaa);
}

void vita2d_wait_rendering_done()
{
	sceGxmFinish(_vita2d_context);
}

int vita2d_fini()
{
	if (system_mode_flag)
		sceSharedFbBegin(shfb_id, &info);

	unsigned int i;

	if (!vita2d_initialized) {
		DEBUG("libvita2d is not initialized!\n");
		return 1;
	}

	// wait until rendering is done
	sceGxmFinish(_vita2d_context);

	// clean up allocations
	sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, clearFragmentProgram);
	sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, clearVertexProgram);
	sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, _vita2d_colorVertexProgram);
	sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, _vita2d_textureVertexProgram);

	_vita2d_free_fragment_programs(&_vita2d_fragmentPrograms.blend_mode_normal);
	_vita2d_free_fragment_programs(&_vita2d_fragmentPrograms.blend_mode_add);

	gpu_free(linearIndicesUid);
	gpu_free(clearVerticesUid);

	// wait until display queue is finished before deallocating display buffers
	sceGxmDisplayQueueFinish();

	// clean up display queue
	gpu_free(depthBufferUid);

	if (system_mode_flag) {
		for (i = 0; i < DISPLAY_BUFFER_COUNT; i++)
			sceGxmSyncObjectDestroy(displayBufferSync[i]);
	}
	else {
		for (i = 0; i < DISPLAY_BUFFER_COUNT; i++) {
			// clear the buffer then deallocate
			sceClibMemset(displayBufferData[i], 0, display_vres*display_stride * 4);
			gpu_free(displayBufferUid[i]);

			// destroy the sync object
			sceGxmSyncObjectDestroy(displayBufferSync[i]);
		}
	}

	// free the depth and stencil buffer
	gpu_free(depthBufferUid);
	gpu_free(stencilBufferUid);

	// unregister programs and destroy shader patcher
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, clearFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, clearVertexProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, colorFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, colorVertexProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, textureFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, textureTintFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, textureVertexProgramId);

	sceGxmShaderPatcherDestroy(shaderPatcher);
	fragment_usse_free(patcherFragmentUsseUid);
	vertex_usse_free(patcherVertexUsseUid);
	gpu_free(patcherBufferUid);

	// destroy the render target
	sceGxmDestroyRenderTarget(renderTarget);

	// destroy the _vita2d_context
	sceGxmDestroyContext(_vita2d_context);
	fragment_usse_free(fragmentUsseRingBufferUid);
	gpu_free(fragmentRingBufferUid);
	gpu_free(vertexRingBufferUid);
	gpu_free(vdmRingBufferUid);
	sceClibMspaceFree(mspace_internal, contextParams.hostMem);

	gpu_free(poolUid);

	// terminate libgxm
	sceGxmTerminate();

	if (pgf_module_was_loaded == SCE_SYSMODULE_LOADED)
		sceSysmoduleUnloadModule(SCE_SYSMODULE_PGF);

	if (system_mode_flag) {
		sceGxmUnmapMemory(info.base1);
		sceSharedFbEnd(shfb_id);
		sceSharedFbClose(shfb_id);
	}

	vita2d_initialized = 0;

	return 1;
}

void vita2d_clib_pass_mspace(void* space) 
{
	mspace_internal = space;
}

void vita2d_clear_screen()
{
	// set clear shaders
	sceGxmSetVertexProgram(_vita2d_context, clearVertexProgram);
	sceGxmSetFragmentProgram(_vita2d_context, clearFragmentProgram);

	// set the clear color
	void *color_buffer;
	sceGxmReserveFragmentDefaultUniformBuffer(_vita2d_context, &color_buffer);
	sceGxmSetUniformDataF(color_buffer, _vita2d_clearClearColorParam, 0, 4, clear_color);

	// draw the clear triangle
	sceGxmSetVertexStream(_vita2d_context, 0, clearVertices);
	sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, linearIndices, 3);
}

void vita2d_start_drawing()
{
	vita2d_pool_reset();
	vita2d_start_drawing_advanced(NULL, 0);
}

void vita2d_start_drawing_advanced(vita2d_texture *target, unsigned int flags)
{
	if (system_mode_flag) {
		sceSharedFbBegin(shfb_id, &info);
		info.owner = 1;
		if (info.curbuf == 1)
			bufferIndex = 0;
		else
			bufferIndex = 1;
	}

	if (target == NULL) {
		sceGxmBeginScene(
		_vita2d_context,
		flags,
		renderTarget,
		NULL,
		NULL,
		displayBufferSync[bufferIndex],
		&displaySurface[bufferIndex],
		&depthSurface);
	} else {
		sceGxmBeginScene(
		_vita2d_context,
		flags,
		target->gxm_rtgt,
		NULL,
		NULL,
		NULL,
		&target->gxm_sfc,
		&target->gxm_sfd);
	}

	drawing = 1;
	// in the current way, the library keeps the region clip across scenes
	if (clipping_enabled) {
		vita2d_set_clip_rectangle(clip_rect_x_min, clip_rect_y_min, clip_rect_x_max, clip_rect_y_max);
	}
}

void vita2d_end_drawing()
{
	sceGxmEndScene(_vita2d_context, NULL, NULL);
	if (system_mode_flag && vblank_wait)
		sceDisplayWaitVblankStart();
	drawing = 0;
}

void vita2d_end_shfb()
{
	if (system_mode_flag)
		sceSharedFbEnd(shfb_id);
	else {

		int oldFb = (bufferIndex + 1) % DISPLAY_BUFFER_COUNT;

		// queue the display swap for this frame
		vita2d_display_data displayData;
		displayData.address = displayBufferData[bufferIndex];
		sceGxmDisplayQueueAddEntry(
			displayBufferSync[oldFb],	// OLD fb
			displayBufferSync[bufferIndex],	// NEW fb
			&displayData);

		// update buffer indices
		bufferIndex = (bufferIndex + 1) % DISPLAY_BUFFER_COUNT;
	}
}

void vita2d_enable_clipping()
{
	clipping_enabled = 1;
	vita2d_set_clip_rectangle(clip_rect_x_min, clip_rect_y_min, clip_rect_x_max, clip_rect_y_max);
}

void vita2d_disable_clipping()
{
	clipping_enabled = 0;
	sceGxmSetFrontStencilFunc(
			_vita2d_context,
			SCE_GXM_STENCIL_FUNC_ALWAYS,
			SCE_GXM_STENCIL_OP_KEEP,
			SCE_GXM_STENCIL_OP_KEEP,
			SCE_GXM_STENCIL_OP_KEEP,
			0xFF,
			0xFF);
}

int vita2d_get_clipping_enabled()
{
	return clipping_enabled;
}

void vita2d_set_clip_rectangle(int x_min, int y_min, int x_max, int y_max)
{
	clip_rect_x_min = x_min;
	clip_rect_y_min = y_min;
	clip_rect_x_max = x_max;
	clip_rect_y_max = y_max;
	// we can only draw during a scene, but we can cache the values since they're not going to have any visible effect till the scene starts anyways
	if(drawing) {
		// clear the stencil buffer to 0
		sceGxmSetFrontStencilFunc(
			_vita2d_context,
			SCE_GXM_STENCIL_FUNC_NEVER,
			SCE_GXM_STENCIL_OP_ZERO,
			SCE_GXM_STENCIL_OP_ZERO,
			SCE_GXM_STENCIL_OP_ZERO,
			0xFF,
			0xFF);
		vita2d_draw_rectangle(0, 0, display_hres, display_vres, 0);
		// set the stencil to 1 in the desired region
		sceGxmSetFrontStencilFunc(
			_vita2d_context,
			SCE_GXM_STENCIL_FUNC_NEVER,
			SCE_GXM_STENCIL_OP_REPLACE,
			SCE_GXM_STENCIL_OP_REPLACE,
			SCE_GXM_STENCIL_OP_REPLACE,
			0xFF,
			0xFF);
		vita2d_draw_rectangle(x_min, y_min, x_max - x_min, y_max - y_min, 0);
		if(clipping_enabled) {
			// set the stencil function to only accept pixels where the stencil is 1
			sceGxmSetFrontStencilFunc(
				_vita2d_context,
				SCE_GXM_STENCIL_FUNC_EQUAL,
				SCE_GXM_STENCIL_OP_KEEP,
				SCE_GXM_STENCIL_OP_KEEP,
				SCE_GXM_STENCIL_OP_KEEP,
				0xFF,
				0xFF);
		} else {
			sceGxmSetFrontStencilFunc(
				_vita2d_context,
				SCE_GXM_STENCIL_FUNC_ALWAYS,
				SCE_GXM_STENCIL_OP_KEEP,
				SCE_GXM_STENCIL_OP_KEEP,
				SCE_GXM_STENCIL_OP_KEEP,
				0xFF,
				0xFF);
		}
	}
}

void vita2d_get_clip_rectangle(int *x_min, int *y_min, int *x_max, int *y_max)
{
	*x_min = clip_rect_x_min;
	*y_min = clip_rect_y_min;
	*x_max = clip_rect_x_max;
	*y_max = clip_rect_y_max;
}

int vita2d_common_dialog_update()
{
	SceCommonDialogUpdateParam updateParam;
	sceClibMemset(&updateParam, 0, sizeof(updateParam));

	updateParam.renderTarget.colorFormat    = DISPLAY_COLOR_FORMAT;
	updateParam.renderTarget.surfaceType    = SCE_GXM_COLOR_SURFACE_LINEAR;
	updateParam.renderTarget.width          = display_hres;
	updateParam.renderTarget.height         = display_vres;
	updateParam.renderTarget.strideInPixels = display_stride;

	updateParam.renderTarget.colorSurfaceData = displayBufferData[bufferIndex];
	updateParam.renderTarget.depthSurfaceData = depthBufferData;
	updateParam.displaySyncObject = displayBufferSync[bufferIndex];

	return sceCommonDialogUpdate(&updateParam);
}

void vita2d_set_clear_color(unsigned int color)
{
	clear_color[0] = ((color >> 8*0) & 0xFF)/255.0f;
	clear_color[1] = ((color >> 8*1) & 0xFF)/255.0f;
	clear_color[2] = ((color >> 8*2) & 0xFF)/255.0f;
	clear_color[3] = ((color >> 8*3) & 0xFF)/255.0f;
	clear_color_u = color;
}

unsigned int vita2d_get_clear_color()
{
	return clear_color_u;
}

void vita2d_set_vblank_wait(int enable)
{
	vblank_wait = enable;
}

SceUID vita2d_get_shfbid()
{
	return shfb_id;
}

SceGxmContext *vita2d_get_context()
{
	return _vita2d_context;
}

SceGxmShaderPatcher *vita2d_get_shader_patcher()
{
	return shaderPatcher;
}

const uint16_t *vita2d_get_linear_indices()
{
	return linearIndices;
}

void vita2d_set_region_clip(SceGxmRegionClipMode mode, unsigned int x_min, unsigned int y_min, unsigned int x_max, unsigned int y_max)
{
	sceGxmSetRegionClip(_vita2d_context, mode, x_min, y_min, x_max, y_max);
}

void *vita2d_pool_malloc(unsigned int size)
{
	if ((pool_index + size) < pool_size) {
		void *addr = (void *)((unsigned int)pool_addr + pool_index);
		pool_index += size;
		return addr;
	}
	return NULL;
}

void *vita2d_pool_memalign(unsigned int size, unsigned int alignment)
{
	unsigned int new_index = (pool_index + alignment - 1) & ~(alignment - 1);
	if ((new_index + size) < pool_size) {
		void *addr = (void *)((unsigned int)pool_addr + new_index);
		pool_index = new_index + size;
		return addr;
	}
	return NULL;
}

unsigned int vita2d_pool_free_space()
{
	return pool_size - pool_index;
}

void vita2d_pool_reset()
{
	pool_index = 0;
}

void vita2d_set_blend_mode_add(int enable)
{
	vita2d_fragment_programs *in = enable ? &_vita2d_fragmentPrograms.blend_mode_add
	    : &_vita2d_fragmentPrograms.blend_mode_normal;

	_vita2d_colorFragmentProgram = in->color;
	_vita2d_textureFragmentProgram = in->texture;
	_vita2d_textureTintFragmentProgram = in->textureTint;
}

#include <display.h>
#include <gxm.h>
#include <kernel.h>
#include <kernel/dmacmgr.h>
#include <message_dialog.h>
#include <libsysmodule.h>
#include <appmgr.h>
#include <libdbg.h>
#include "vita2d_sys.h"


#include "utils.h"
#include "heap.h"
#include "pvr.h"

/* Shader binaries */

#include "shader/compiled/clear_v_gxp.h"
#include "shader/compiled/clear_f_gxp.h"
#include "shader/compiled/color_v_gxp.h"
#include "shader/compiled/color_f_gxp.h"
#include "shader/compiled/texture_v_gxp.h"
#include "shader/compiled/texture_f_gxp.h"
#include "shader/compiled/texture_tint_f_gxp.h"

/* Defines */

#define DEFAULT_HEAP_SIZE			1 * 1024 * 1024;
#define DISPLAY_COLOR_FORMAT		SCE_GXM_COLOR_FORMAT_A8B8G8R8
#define DISPLAY_PIXEL_FORMAT		SCE_DISPLAY_PIXELFORMAT_A8B8G8R8
#define DISPLAY_BUFFER_COUNT		2
#define DEFAULT_TEMP_POOL_SIZE		(1 * 1024 * 1024)

typedef struct vita2d_display_data {
	void *address;
} vita2d_display_data;

/* Extern */

extern int sceKernelIsGameBudget(void);

/* Static variables */

static SceSharedFbInfo info;

static const SceGxmProgram *const clearVertexProgramGxp = (const SceGxmProgram*)clear_v_gxp;
static const SceGxmProgram *const clearFragmentProgramGxp = (const SceGxmProgram*)clear_f_gxp;
static const SceGxmProgram *const colorVertexProgramGxp = (const SceGxmProgram*)color_v_gxp;
static const SceGxmProgram *const colorFragmentProgramGxp = (const SceGxmProgram*)color_f_gxp;
static const SceGxmProgram *const textureVertexProgramGxp = (const SceGxmProgram*)texture_v_gxp;
static const SceGxmProgram *const textureFragmentProgramGxp = (const SceGxmProgram*)texture_f_gxp;
static const SceGxmProgram *const textureTintFragmentProgramGxp = (const SceGxmProgram*)texture_tint_f_gxp;

static int display_hres = 960;
static int display_vres = 544;
static int display_stride = 960;
static int max_display_hres = 960;
static int max_display_vres = 544;
static int max_display_stride = 960;

static int vita2d_initialized = 0;
static float clear_color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
static unsigned int clear_color_u = 0xFF000000;
static int clip_rect_x_min = 0;
static int clip_rect_y_min = 0;
static int clip_rect_x_max = 960;
static int clip_rect_y_max = 544;
static int vblank_wait = 1;
static int drawing = 0;
static int clipping_enabled = 0;

static vita2d_init_param init_param_s;
static SceUID renderTargetMemUid;
static vita2d_shared_mem_info *vdmRingBufferMem;
static vita2d_shared_mem_info *vertexRingBufferMem;
static vita2d_shared_mem_info *fragmentRingBufferMem;
static vita2d_shared_mem_info *fragmentUsseRingBufferMem;

static SceUID shfb_id;

static SceGxmValidRegion validRegion;
static SceGxmMultisampleMode msaa_s;
static SceGxmContextParams contextParams;
static SceGxmRenderTarget *renderTarget = NULL;
static void *displayBufferData[DISPLAY_BUFFER_COUNT];
static SceGxmColorSurface displaySurface[DISPLAY_BUFFER_COUNT];
static SceGxmSyncObject *displayBufferSync[DISPLAY_BUFFER_COUNT];

static SceGxmDeviceMemInfo *displayBufferMem[DISPLAY_BUFFER_COUNT];
static void *displayBufferData[DISPLAY_BUFFER_COUNT];

static int bufferIndex = 1;

static SceGxmDeviceMemInfo *depthBufferMem;
static SceGxmDeviceMemInfo *stencilBufferMem;
static SceGxmDepthStencilSurface depthSurface;

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

static SceGxmDeviceMemInfo *patcherBufferMem;
static SceGxmDeviceMemInfo *patcherVertexUsseMem;
static SceGxmDeviceMemInfo *patcherFragmentUsseMem;

static SceGxmDeviceMemInfo *clearVerticesMem;
static SceGxmDeviceMemInfo *clearIndicesMem;
static SceGxmDeviceMemInfo *linearIndicesMem;
static vita2d_clear_vertex *clearVertices = NULL;
static uint16_t *clearIndices = NULL;
static uint16_t *linearIndices = NULL;

/* PVR */

void *psDevData;
void *phTransferContext;
PVRSRVHeapInfoVita pvrsrvHeapInfo;
int pvrsrvContextAttribs;

/* Shared with other .c */
void *vita2d_heap_internal;
int system_mode_flag = 1;
int pgf_module_was_loaded = 10;
float _vita2d_ortho_matrix[4 * 4];
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
static SceGxmDeviceMemInfo *poolMem;
static unsigned int pool_index = 0;

/* Static functions */

static void *patcher_host_alloc(void *user_data, uint32_t size)
{
	void* pMem = heap_alloc_heap_memory(vita2d_heap_internal, size);
	if (pMem == NULL)
		SCE_DBG_LOG_ERROR("patcher_host_alloc(): heap_alloc_heap_memory() returned NULL");
	return pMem;
}

static void patcher_host_free(void *user_data, void *mem)
{
	heap_free_heap_memory(vita2d_heap_internal, mem);
}

static int _vita2d_free_fragment_programs(vita2d_fragment_programs *out)
{
	int ret;
	ret = sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, out->color);
	if (ret != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmShaderPatcherReleaseFragmentProgram(): 0x%X", ret);
		goto _free_fragment_programs_error;
	}
	ret = sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, out->texture);
	if (ret != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmShaderPatcherReleaseFragmentProgram(): 0x%X", ret);
		goto _free_fragment_programs_error;
	}
	ret = sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, out->textureTint);
	if (ret != SCE_OK)
		SCE_DBG_LOG_ERROR("sceGxmShaderPatcherReleaseFragmentProgram(): 0x%X", ret);

_free_fragment_programs_error:

	return ret;
}

static int _vita2d_make_fragment_programs(vita2d_fragment_programs *out,
	const SceGxmBlendInfo *blend_info, SceGxmMultisampleMode msaa)
{
	int err;
	UNUSED(err);

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		colorFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa,
		blend_info,
		colorVertexProgramGxp,
		&out->color);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("color sceGxmShaderPatcherCreateFragmentProgram(): 0x%X", err);
		goto _make_fragment_programs_error;
	}

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		textureFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa,
		blend_info,
		textureVertexProgramGxp,
		&out->texture);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("texture sceGxmShaderPatcherCreateFragmentProgram(): 0x%X", err);
		goto _make_fragment_programs_error;
	}

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		textureTintFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa,
		blend_info,
		textureVertexProgramGxp,
		&out->textureTint);

	if (err != SCE_OK)
		SCE_DBG_LOG_ERROR("texture_tint sceGxmShaderPatcherCreateFragmentProgram(): 0x%X", err);

_make_fragment_programs_error:

	return err;
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
	sceDisplaySetFrameBuf(&framebuf, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);

	if (vblank_wait) {
		sceDisplayWaitVblankStart();
	}
}

static void driver_bridge_init(void)
{
	psDevData = sceGxmGetDeviceData();

	sceClibMemset(&pvrsrvHeapInfo, 0, sizeof(PVRSRVHeapInfoVita));

	pvrsrvHeapInfo.generalHeapId = *(int *)(psDevData + 0xC0);

	pvrsrvHeapInfo.vertexshaderHeapId = *(int *)(psDevData + 0xC4);
	pvrsrvHeapInfo.pixelshaderHeapId = *(int *)(psDevData + 0xC8);
	pvrsrvHeapInfo.vertexshaderHeapSize = *(uint32_t *)(psDevData + 0xCC);
	pvrsrvHeapInfo.pixelshaderHeapSize = *(uint32_t *)(psDevData + 0xD0);

	pvrsrvHeapInfo.vertexshaderSharedHeapId = *(int *)(psDevData + 0xD4);
	pvrsrvHeapInfo.pixelshaderSharedHeapId = *(int *)(psDevData + 0xD8);
	pvrsrvHeapInfo.vertexshaderSharedHeapSize = *(uint32_t *)(psDevData + 0xDC);
	pvrsrvHeapInfo.pixelshaderSharedHeapSize = *(uint32_t *)(psDevData + 0xE0);

	pvrsrvHeapInfo.syncinfoHeapId = *(int *)(psDevData + 0xE4);
	pvrsrvHeapInfo.vpbTiledHeapId = *(int *)(psDevData + 0xE8);

	pvrsrvContextAttribs = *(int *)(psDevData + 0xF0);

	phTransferContext = (psDevData + 0x100);
}

static int vita2d_setup_shaders(void)
{
	int err;
	UNUSED(err);

	// register programs with the patcher
	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, clearVertexProgramGxp, &clearVertexProgramId);
	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("clear_v sceGxmShaderPatcherRegisterProgram(): 0x%X", err);
		goto _init_internal_common_error;
	}

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, clearFragmentProgramGxp, &clearFragmentProgramId);
	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("clear_f sceGxmShaderPatcherRegisterProgram(): 0x%X", err);
		goto _init_internal_common_error;
	}

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, colorVertexProgramGxp, &colorVertexProgramId);
	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("color_v sceGxmShaderPatcherRegisterProgram(): 0x%X", err);
		goto _init_internal_common_error;
	}

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, colorFragmentProgramGxp, &colorFragmentProgramId);
	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("color_f sceGxmShaderPatcherRegisterProgram(): 0x%X", err);
		goto _init_internal_common_error;
	}

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureVertexProgramGxp, &textureVertexProgramId);
	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("texture_v sceGxmShaderPatcherRegisterProgram(): 0x%X", err);
		goto _init_internal_common_error;
	}

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureFragmentProgramGxp, &textureFragmentProgramId);
	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("texture_f sceGxmShaderPatcherRegisterProgram(): 0x%X", err);
		goto _init_internal_common_error;
	}

	err = sceGxmShaderPatcherRegisterProgram(shaderPatcher, textureTintFragmentProgramGxp, &textureTintFragmentProgramId);
	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("texture_tint_f sceGxmShaderPatcherRegisterProgram(): 0x%X", err);
		goto _init_internal_common_error;
	}

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

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("clear sceGxmShaderPatcherCreateVertexProgram(): 0x%X", err);
		goto _init_internal_common_error;
	}

	err = sceGxmShaderPatcherCreateFragmentProgram(
		shaderPatcher,
		clearFragmentProgramId,
		SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
		msaa_s,
		NULL,
		clearVertexProgramGxp,
		&clearFragmentProgram);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("clear sceGxmShaderPatcherCreateFragmentProgram(): 0x%X", err);
		goto _init_internal_common_error;
	}

	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_USER_NC,
		SCE_GXM_MEMORY_ATTRIB_READ,
		UINT16_MAX * sizeof(uint16_t),
		sizeof(uint16_t),
		&linearIndicesMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmAllocDeviceMemLinux(): 0x%X", err);
		goto _init_internal_common_error;
	}

	linearIndices = (uint16_t *)(linearIndicesMem->mappedBase);

	// Range of i must be greater than uint16_t, this doesn't endless-loop
	for (uint32_t i = 0; i <= UINT16_MAX; ++i) {
		linearIndices[i] = i;
	}

	// create the clear rectangle vertex/index data

	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_USER_NC,
		SCE_GXM_MEMORY_ATTRIB_READ,
		4 * sizeof(vita2d_clear_vertex),
		4,
		&clearVerticesMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmAllocDeviceMemLinux(): 0x%X", err);
		goto _init_internal_common_error;
	}

	clearVertices = (vita2d_clear_vertex *)(clearVerticesMem->mappedBase);

	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_USER_NC,
		SCE_GXM_MEMORY_ATTRIB_READ,
		6 * sizeof(uint16_t),
		2,
		&clearIndicesMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmAllocDeviceMemLinux(): 0x%X", err);
		goto _init_internal_common_error;
	}

	clearIndices = (uint16_t *)(clearIndicesMem->mappedBase);

	clearVertices[0].x = -1.0f;
	clearVertices[0].y = -1.0f;
	clearVertices[1].x = 1.0f;
	clearVertices[1].y = -1.0f;
	clearVertices[2].x = -1.0f;
	clearVertices[2].y = 1.0f;
	clearVertices[3].x = 1.0f;
	clearVertices[3].y = 1.0f;

	clearIndices[0] = 0;
	clearIndices[1] = 1;
	clearIndices[2] = 2;
	clearIndices[3] = 1;
	clearIndices[4] = 2;
	clearIndices[5] = 3;

	const SceGxmProgramParameter *paramColorPositionAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aPosition");
	const SceGxmProgramParameter *paramColorColorAttribute = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "aColor");

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

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("color sceGxmShaderPatcherCreateVertexProgram(): 0x%X", err);
		goto _init_internal_common_error;;
	}

	const SceGxmProgramParameter *paramTexturePositionAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aPosition");
	const SceGxmProgramParameter *paramTextureTexcoordAttribute = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "aTexcoord");

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

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("texture sceGxmShaderPatcherCreateVertexProgram(): 0x%X", err);
		goto _init_internal_common_error;
	}

	// Create variations of the fragment program based on blending mode
	_vita2d_make_fragment_programs(&_vita2d_fragmentPrograms.blend_mode_normal, &blend_info, msaa_s);
	_vita2d_make_fragment_programs(&_vita2d_fragmentPrograms.blend_mode_add, &blend_info_add, msaa_s);

	// Default to "normal" blending mode (non-additive)
	vita2d_set_blend_mode_add(0);

	// find vertex uniforms by name and cache parameter information
	_vita2d_clearClearColorParam = sceGxmProgramFindParameterByName(clearFragmentProgramGxp, "uClearColor");
	_vita2d_colorWvpParam = sceGxmProgramFindParameterByName(colorVertexProgramGxp, "wvp");
	_vita2d_textureWvpParam = sceGxmProgramFindParameterByName(textureVertexProgramGxp, "wvp");
	_vita2d_textureTintColorParam = sceGxmProgramFindParameterByName(textureTintFragmentProgramGxp, "uTintColor");

	// Allocate memory for the memory pool
	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_USER_NC,
		SCE_GXM_MEMORY_ATTRIB_READ,
		init_param_s.temp_pool_size,
		sizeof(void *),
		&poolMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmAllocDeviceMemLinux(): 0x%X", err);
		goto _init_internal_common_error;
	}

	matrix_init_orthographic(_vita2d_ortho_matrix, 0.0f, display_hres, display_vres, 0.0f, 0.0f, 1.0f);

	/* Wait if there are unfinished PTLA operations */
	SGXWaitTransfer(psDevData, phTransferContext);

	vita2d_initialized = 1;

_init_internal_common_error:

	return err;
}

static int vita2d_init_internal_common()
{
	int err;
	unsigned int i;
	UNUSED(err);

	validRegion.xMax = display_hres - 1;
	validRegion.yMax = display_vres - 1;

	msaa_s = init_param_s.msaa;
	SceGxmDeviceHeapId mem_type = vita2d_texture_get_heap_type();

	// allocate ring buffer memory

	if (init_param_s.vdm_ring_buffer_attrib & VITA2D_MEM_ATTRIB_SHARED) {
		err = PVRSRVAllocDeviceMem(
			psDevData,
			pvrsrvHeapInfo.generalHeapId,
			PVRSRV_VITA_GENERIC_MEMORY_ATTRIB,
			init_param_s.vdm_ring_buffer_size,
			4,
			pvrsrvContextAttribs,
			(PVRSRVMemInfoVita **)&vdmRingBufferMem);
		err = PVRSRVMapMemoryToGpu(
			psDevData,
			2,
			0,
			init_param_s.vdm_ring_buffer_size,
			0,
			vdmRingBufferMem->mappedBase,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE | 0x100,
			NULL);
	}
	else {
		err = sceGxmAllocDeviceMemLinux(
			SCE_GXM_DEVICE_HEAP_ID_USER_NC,
			SCE_GXM_MEMORY_ATTRIB_READ,
			init_param_s.vdm_ring_buffer_size,
			4,
			(SceGxmDeviceMemInfo **)&vdmRingBufferMem);
	}

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("VDM ring buffer allocation failed: 0x%X", err);
		goto _init_internal_common_error;
	}

	if (init_param_s.vertex_ring_buffer_attrib & VITA2D_MEM_ATTRIB_SHARED) {
		err = PVRSRVAllocDeviceMem(
			psDevData,
			pvrsrvHeapInfo.generalHeapId,
			PVRSRV_VITA_GENERIC_MEMORY_ATTRIB,
			init_param_s.vertex_ring_buffer_size,
			4,
			pvrsrvContextAttribs,
			(PVRSRVMemInfoVita **)&vertexRingBufferMem);
		err = PVRSRVMapMemoryToGpu(
			psDevData,
			2,
			0,
			init_param_s.vertex_ring_buffer_size,
			0,
			vertexRingBufferMem->mappedBase,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE | 0x100,
			NULL);
	}
	else {
		err = sceGxmAllocDeviceMemLinux(
			SCE_GXM_DEVICE_HEAP_ID_USER_NC,
			SCE_GXM_MEMORY_ATTRIB_READ,
			init_param_s.vertex_ring_buffer_size,
			4,
			(SceGxmDeviceMemInfo **)&vertexRingBufferMem);
	}

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("Vertex ring buffer allocation failed: 0x%X", err);
		goto _init_internal_common_error;
	}

	if (init_param_s.fragment_ring_buffer_attrib & VITA2D_MEM_ATTRIB_SHARED) {
		err = PVRSRVAllocDeviceMem(
			psDevData,
			pvrsrvHeapInfo.generalHeapId,
			PVRSRV_VITA_GENERIC_MEMORY_ATTRIB,
			init_param_s.fragment_ring_buffer_size,
			4,
			pvrsrvContextAttribs,
			(PVRSRVMemInfoVita **)&fragmentRingBufferMem);
		err = PVRSRVMapMemoryToGpu(
			psDevData,
			2,
			0,
			init_param_s.fragment_ring_buffer_size,
			0,
			fragmentRingBufferMem->mappedBase,
			SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE | 0x100,
			NULL);
	}
	else {
		err = sceGxmAllocDeviceMemLinux(
			SCE_GXM_DEVICE_HEAP_ID_USER_NC,
			SCE_GXM_MEMORY_ATTRIB_READ,
			init_param_s.fragment_ring_buffer_size,
			4,
			(SceGxmDeviceMemInfo **)&fragmentRingBufferMem);
	}

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("Fragment ring buffer allocation failed: 0x%X", err);
		goto _init_internal_common_error;
	}

	if (init_param_s.fragment_usse_ring_buffer_attrib & VITA2D_MEM_ATTRIB_SHARED) {
		err = PVRSRVAllocDeviceMem(
			psDevData,
			pvrsrvHeapInfo.pixelshaderHeapId,
			PVRSRV_VITA_GENERIC_MEMORY_ATTRIB,
			init_param_s.fragment_usse_ring_buffer_size,
			4,
			pvrsrvContextAttribs,
			(PVRSRVMemInfoVita **)&fragmentUsseRingBufferMem);
		err = sceGxmMapFragmentUsseMemory(
			fragmentUsseRingBufferMem->mappedBase,
			init_param_s.fragment_usse_ring_buffer_size,
			&fragmentUsseRingBufferMem->offset);
	}
	else {
		err = sceGxmAllocDeviceMemLinux(
			SCE_GXM_DEVICE_HEAP_ID_FRAGMENT_USSE,
			SCE_GXM_MEMORY_ATTRIB_READ,
			init_param_s.fragment_usse_ring_buffer_size,
			4096,
			(SceGxmDeviceMemInfo **)&fragmentUsseRingBufferMem);
	}

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("Fragment USSE ring buffer allocation failed: 0x%X", err);
		goto _init_internal_common_error;
	}

	sceClibMemset(&contextParams, 0, sizeof(SceGxmContextParams));
	contextParams.hostMem = PVRSRVAllocUserModeMem(SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE);
	contextParams.hostMemSize = SCE_GXM_MINIMUM_CONTEXT_HOST_MEM_SIZE;
	contextParams.vdmRingBufferMem = vdmRingBufferMem->mappedBase;
	contextParams.vdmRingBufferMemSize = init_param_s.vdm_ring_buffer_size;
	contextParams.vertexRingBufferMem = vertexRingBufferMem->mappedBase;
	contextParams.vertexRingBufferMemSize = init_param_s.vertex_ring_buffer_size;
	contextParams.fragmentRingBufferMem = fragmentRingBufferMem->mappedBase;
	contextParams.fragmentRingBufferMemSize = init_param_s.fragment_ring_buffer_size;
	contextParams.fragmentUsseRingBufferMem = fragmentUsseRingBufferMem->mappedBase;
	contextParams.fragmentUsseRingBufferMemSize = init_param_s.fragment_usse_ring_buffer_size;
	contextParams.fragmentUsseRingBufferOffset = fragmentUsseRingBufferMem->offset;

	err = sceGxmCreateContext(&contextParams, &_vita2d_context);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmCreateContext(): 0x%X", err);
		goto _init_internal_common_error;
	}

	// set up parameters
	SceGxmRenderTargetParams renderTargetParams;
	sceClibMemset(&renderTargetParams, 0, sizeof(SceGxmRenderTargetParams));
	renderTargetParams.flags = 0;
	renderTargetParams.width = max_display_hres;
	renderTargetParams.height = max_display_vres;
	renderTargetParams.scenesPerFrame = 1;
	renderTargetParams.multisampleMode = msaa_s;
	renderTargetParams.multisampleLocations = 0;
	renderTargetParams.driverMemBlock = -1; // Invalid UID

	// allocate target memblock
	uint32_t targetMemsize;
	sceGxmGetRenderTargetMemSize(&renderTargetParams, &targetMemsize);
	renderTargetMemUid = sceKernelAllocMemBlock("render_target_mem", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, targetMemsize, NULL);
	renderTargetParams.driverMemBlock = renderTargetMemUid;

	// create the render target
	err = sceGxmCreateRenderTarget(&renderTargetParams, &renderTarget);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmCreateRenderTarget(): 0x%X", err);
		goto _init_internal_common_error;
	}

	// allocate memory and sync objects for display buffers
	for (i = 0; i < DISPLAY_BUFFER_COUNT; i++) {

		if (!system_mode_flag) {
			// allocate memory for display
			sceGxmAllocDeviceMemLinux(
				SCE_GXM_DEVICE_HEAP_ID_CDRAM,
				SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
				4 * max_display_stride*max_display_vres,
				SCE_GXM_COLOR_SURFACE_ALIGNMENT,
				&displayBufferMem[i]);

			displayBufferData[i] = displayBufferMem[i]->mappedBase;

			// memset the buffer to black
			sceGxmTransferFill(
				0xff000000,
				SCE_GXM_TRANSFER_FORMAT_U8U8U8U8_ABGR,
				displayBufferData[i],
				0,
				0,
				max_display_hres,
				max_display_vres,
				max_display_stride,
				NULL,
				0,
				NULL);

			// create a sync object that we will associate with this buffer
			err = PVRSRVAllocSyncInfo(psDevData, &displayBufferSync[i]);

			if (err != SCE_OK) {
				SCE_DBG_LOG_ERROR("PVRSRVAllocSyncInfo(): 0x%X", err);
				goto _init_internal_common_error;
			}
		}
		else {
			// open shared sync object that we will associate with this buffer
			err = PVRSRVOpenSharedSyncInfo(psDevData, i + 1, &displayBufferSync[i]);

			if (err != SCE_OK) {
				SCE_DBG_LOG_ERROR("PVRSRVOpenSharedSyncInfo(): 0x%X", err);
				goto _init_internal_common_error;
			}
		}

		// initialize a color surface for this display buffer
		err = sceGxmColorSurfaceInit(
			&displaySurface[i],
			DISPLAY_COLOR_FORMAT,
			SCE_GXM_COLOR_SURFACE_LINEAR,
			(msaa_s == SCE_GXM_MULTISAMPLE_NONE) ? SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
			SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
			display_hres,
			display_vres,
			display_stride,
			displayBufferData[i]);

		if (err != SCE_OK) {
			SCE_DBG_LOG_ERROR("sceGxmColorSurfaceInit(): 0x%X", err);
			goto _init_internal_common_error;
		}
	}

	// compute the memory footprint of the depth buffer
	const unsigned int alignedWidth = ALIGN(max_display_hres, SCE_GXM_TILE_SIZEX);
	const unsigned int alignedHeight = ALIGN(max_display_vres, SCE_GXM_TILE_SIZEY);
	unsigned int sampleCount = alignedWidth * alignedHeight;
	unsigned int depthStrideInSamples = alignedWidth;
	if (msaa_s == SCE_GXM_MULTISAMPLE_4X) {
		// samples increase in X and Y
		sampleCount *= 4;
		depthStrideInSamples *= 2;
	}
	else if (msaa_s == SCE_GXM_MULTISAMPLE_2X) {
		// samples increase in Y only
		sampleCount *= 2;
	}

	// allocate the depth buffer
	err = sceGxmAllocDeviceMemLinux(
		mem_type,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		4 * sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		&depthBufferMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmAllocDeviceMemLinux(): 0x%X", err);
		goto _init_internal_common_error;
	}

	// allocate the stencil buffer
	err = sceGxmAllocDeviceMemLinux(
		mem_type,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		4 * sampleCount,
		SCE_GXM_DEPTHSTENCIL_SURFACE_ALIGNMENT,
		&stencilBufferMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmAllocDeviceMemLinux(): 0x%X", err);
		goto _init_internal_common_error;
	}

	// create the SceGxmDepthStencilSurface structure
	err = sceGxmDepthStencilSurfaceInit(
		&depthSurface,
		SCE_GXM_DEPTH_STENCIL_FORMAT_S8D24,
		SCE_GXM_DEPTH_STENCIL_SURFACE_TILED,
		depthStrideInSamples,
		depthBufferMem->mappedBase,
		stencilBufferMem->mappedBase);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmDepthStencilSurfaceInit(): 0x%X", err);
		goto _init_internal_common_error;
	}

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
	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_USER_NC,
		SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE,
		patcherBufferSize,
		4,
		&patcherBufferMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmAllocDeviceMemLinux(): 0x%X", err);
		goto _init_internal_common_error;
	}

	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_VERTEX_USSE,
		SCE_GXM_MEMORY_ATTRIB_READ,
		patcherVertexUsseSize,
		4096,
		&patcherVertexUsseMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmAllocDeviceMemLinux(): 0x%X", err);
		goto _init_internal_common_error;
	}

	err = sceGxmAllocDeviceMemLinux(
		SCE_GXM_DEVICE_HEAP_ID_FRAGMENT_USSE,
		SCE_GXM_MEMORY_ATTRIB_READ,
		patcherFragmentUsseSize,
		4096,
		&patcherFragmentUsseMem);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmAllocDeviceMemLinux(): 0x%X", err);
		goto _init_internal_common_error;
	}

	// create a shader patcher
	SceGxmShaderPatcherParams patcherParams;
	sceClibMemset(&patcherParams, 0, sizeof(SceGxmShaderPatcherParams));
	patcherParams.userData = NULL;
	patcherParams.hostAllocCallback = &patcher_host_alloc;
	patcherParams.hostFreeCallback = &patcher_host_free;
	patcherParams.bufferAllocCallback = NULL;
	patcherParams.bufferFreeCallback = NULL;
	patcherParams.bufferMem = patcherBufferMem->mappedBase;
	patcherParams.bufferMemSize = patcherBufferMem->size;
	patcherParams.vertexUsseAllocCallback = NULL;
	patcherParams.vertexUsseFreeCallback = NULL;
	patcherParams.vertexUsseMem = patcherVertexUsseMem->mappedBase;
	patcherParams.vertexUsseMemSize = patcherVertexUsseMem->size;
	patcherParams.vertexUsseOffset = patcherVertexUsseMem->offset;
	patcherParams.fragmentUsseAllocCallback = NULL;
	patcherParams.fragmentUsseFreeCallback = NULL;
	patcherParams.fragmentUsseMem = patcherFragmentUsseMem->mappedBase;
	patcherParams.fragmentUsseMemSize = patcherFragmentUsseMem->size;
	patcherParams.fragmentUsseOffset = patcherFragmentUsseMem->offset;

	err = sceGxmShaderPatcherCreate(&patcherParams, &shaderPatcher);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmShaderPatcherCreate(): 0x%X", err);
		goto _init_internal_common_error;
	}

	return vita2d_setup_shaders();

_init_internal_common_error:

	return err;
}

static int vita2d_init_internal_common_external(vita2d_init_param_external *init_param)
{
	validRegion.xMax = display_hres - 1;
	validRegion.yMax = display_vres - 1;
	display_stride = init_param->display_stride;
	msaa_s = init_param->msaa;

	_vita2d_context = init_param->imm_context;
	renderTarget = init_param->render_target;
	displayBufferData[0] = init_param->display_buffer_data[0];
	displayBufferData[1] = init_param->display_buffer_data[1];
	displayBufferSync[0] = init_param->display_buffer_sync[0];
	displayBufferSync[1] = init_param->display_buffer_sync[1];
	displaySurface[0] = *init_param->display_color_surface[0];
	displaySurface[1] = *init_param->display_color_surface[1];
	depthSurface = *init_param->depth_stencil_surface;
	shaderPatcher = init_param->shader_patcher;

	return vita2d_setup_shaders();
}

int vita2d_init(vita2d_init_param *init_param)
{
	int err;
	UNUSED(err);

	if (vita2d_initialized) {
		SCE_DBG_LOG_WARNING("libvita2d is already initialized!");
		return VITA2D_SYS_ERROR_ALREADY_INITIALIZED;
	}

	if (init_param == NULL) {
		return VITA2D_SYS_ERROR_INVALID_POINTER;
	}

	sceClibMemcpy(&init_param_s, init_param, sizeof(vita2d_init_param));

	if (!init_param_s.temp_pool_size)
		init_param_s.temp_pool_size = DEFAULT_TEMP_POOL_SIZE;
	if (!init_param_s.heap_size)
		init_param_s.heap_size = DEFAULT_HEAP_SIZE;
	if (!init_param_s.param_buffer_size)
		init_param_s.param_buffer_size = SCE_GXM_DEFAULT_PARAMETER_BUFFER_SIZE;
	if (!init_param_s.vdm_ring_buffer_size)
		init_param_s.vdm_ring_buffer_size = SCE_GXM_DEFAULT_VDM_RING_BUFFER_SIZE;
	if (!init_param_s.vertex_ring_buffer_size)
		init_param_s.vertex_ring_buffer_size = SCE_GXM_DEFAULT_VERTEX_RING_BUFFER_SIZE;
	if (!init_param_s.fragment_ring_buffer_size)
		init_param_s.fragment_ring_buffer_size = SCE_GXM_DEFAULT_FRAGMENT_RING_BUFFER_SIZE;
	if (!init_param_s.fragment_usse_ring_buffer_size)
		init_param_s.fragment_usse_ring_buffer_size = SCE_GXM_DEFAULT_FRAGMENT_USSE_RING_BUFFER_SIZE;

	vita2d_heap_internal = heap_create_heap("vita2d_heap", init_param_s.heap_size, HEAP_AUTO_EXTEND, NULL);

	err = sceKernelIsGameBudget();
	if (err == 1)
		system_mode_flag = 0;

	SCE_DBG_LOG_DEBUG("system_mode_flag: %d\n", system_mode_flag);

	if (system_mode_flag) {

		SceGxmInitializeParams gxm_init_params_internal;
		sceClibMemset(&gxm_init_params_internal, 0, sizeof(SceGxmInitializeParams));
		gxm_init_params_internal.flags = SCE_GXM_INITIALIZE_FLAG_PBDESCFLAGS_ZLS_OVERRIDE | SCE_GXM_INITIALIZE_FLAG_DRIVER_MEM_SHARE;
		gxm_init_params_internal.displayQueueMaxPendingCount = 2;

		err = sceGxmInitializeInternal(&gxm_init_params_internal);

		if (err != SCE_OK) {
			SCE_DBG_LOG_ERROR("sceGxmInitializeInternal(): 0x%X", err);
			return err;
		}

		driver_bridge_init();

		while (1) {
			shfb_id = sceSharedFbOpen(1);
			sceSharedFbGetInfo(shfb_id, &info);
			sceKernelDelayThread(40);
			if (info.curbuf == 1)
				sceSharedFbClose(shfb_id);
			else
				break;
		}

		vita2d_texture_set_heap_type(SCE_GXM_DEVICE_HEAP_ID_USER_NC);

		err = PVRSRVMapMemoryToGpu(psDevData, 2, 0, info.memsize, 0, info.frontBuffer, SCE_GXM_MEMORY_ATTRIB_READ | SCE_GXM_MEMORY_ATTRIB_WRITE | 0x100, NULL);

		if (err != SCE_OK) {
			SCE_DBG_LOG_ERROR("sharedfb PVRSRVMapMemoryToGpu(): 0x%X", err);
			return err;
		}

		displayBufferData[0] = info.frontBuffer;
		displayBufferData[1] = info.backBuffer;

		return vita2d_init_internal_common();
	}
	else {

		SceGxmInitializeParams initializeParams;
		sceClibMemset(&initializeParams, 0, sizeof(SceGxmInitializeParams));
		initializeParams.flags = 0;
		initializeParams.displayQueueMaxPendingCount = 2;
		initializeParams.displayQueueCallback = display_callback;
		initializeParams.displayQueueCallbackDataSize = sizeof(vita2d_display_data);
		initializeParams.parameterBufferSize = init_param_s.param_buffer_size;

		err = sceGxmInitialize(&initializeParams);

		if (err != SCE_OK) {
			SCE_DBG_LOG_ERROR("sceGxmInitialize(): 0x%X", err);
			return err;
		}

		driver_bridge_init();

		return vita2d_init_internal_common();
	}
}

int vita2d_init_external(vita2d_init_param_external *init_param)
{
	int err;
	UNUSED(err);

	if (vita2d_initialized) {
		SCE_DBG_LOG_WARNING("libvita2d is already initialized!");
		return VITA2D_SYS_ERROR_ALREADY_INITIALIZED;
	}

	if (init_param == NULL) {
		return VITA2D_SYS_ERROR_INVALID_POINTER;
	}

	init_param_s.temp_pool_size = init_param->temp_pool_size;
	init_param_s.heap_size = init_param->heap_size;

	if (!init_param_s.temp_pool_size)
		init_param_s.temp_pool_size = DEFAULT_TEMP_POOL_SIZE;
	if (!init_param_s.heap_size)
		init_param_s.heap_size = DEFAULT_HEAP_SIZE;

	vita2d_heap_internal = heap_create_heap("vita2d_heap", init_param_s.heap_size, HEAP_AUTO_EXTEND, NULL);

	err = sceKernelIsGameBudget();
	if (err == 1)
		system_mode_flag = 0;

	SCE_DBG_LOG_DEBUG("system_mode_flag: %d\n", system_mode_flag);

	if (system_mode_flag) {

		driver_bridge_init();
		vita2d_texture_set_heap_type(SCE_GXM_DEVICE_HEAP_ID_USER_NC);

		return vita2d_init_internal_common_external(init_param);
	}
	else {

		driver_bridge_init();

		return vita2d_init_internal_common_external(init_param);
	}
}

int vita2d_display_set_resolution(int hRes, int vRes)
{
	if (hRes > 1920 || vRes > 1088)
		return VITA2D_SYS_ERROR_INVALID_ARGUMENT;

	display_hres = hRes;
	display_vres = vRes;

	switch (hRes) {
	case 1920:
		display_stride = 1920;
		break;
	case 1280:
		display_stride = 1280;
		break;
	case 960:
		display_stride = 960;
		break;
	case 720:
		display_stride = 768;
		break;
	case 640:
		display_stride = 640;
		break;
	case 480:
		display_stride = 512;
		break;
	}

	matrix_init_orthographic(_vita2d_ortho_matrix, 0.0f, display_hres, display_vres, 0.0f, 0.0f, 1.0f);

	validRegion.xMax = hRes - 1;
	validRegion.yMax = vRes - 1;

	return SCE_OK;
}

int vita2d_display_set_max_resolution(int hRes, int vRes)
{
	if (hRes > 1920 || vRes > 1088)
		return VITA2D_SYS_ERROR_INVALID_ARGUMENT;

	max_display_hres = hRes;
	max_display_vres = vRes;

	switch (hRes) {
	case 1920:
		max_display_stride = 1920;
		break;
	case 1280:
		max_display_stride = 1280;
		break;
	case 960:
		max_display_stride = 960;
		break;
	case 720:
		max_display_stride = 768;
		break;
	case 640:
		max_display_stride = 640;
		break;
	case 480:
		max_display_stride = 512;
		break;
	}

	validRegion.xMax = hRes - 1;
	validRegion.yMax = vRes - 1;

	return SCE_OK;
}

int vita2d_wait_rendering_done()
{
	return sceGxmFinish(_vita2d_context);
}

int vita2d_fini()
{
	unsigned int i;
	int err;
	UNUSED(err);

	if (!vita2d_initialized) {
		SCE_DBG_LOG_WARNING("libvita2d is not initialized!");
		return VITA2D_SYS_ERROR_NOT_INITIALIZED;
	}

	// wait until rendering is done
	err = sceGxmFinish(_vita2d_context);

	if (err != SCE_OK) {
		SCE_DBG_LOG_WARNING("sceGxmFinish(): 0x%X", err);
		goto _fini_error;
	}

	if (system_mode_flag)
		sceSharedFbBegin(shfb_id, &info);

	// clean up allocations
	err = sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, clearFragmentProgram);
	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("clearFragmentProgram sceGxmShaderPatcherReleaseFragmentProgram(): 0x%X", err);
		goto _fini_error;
	}

	err = sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, clearVertexProgram);
	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("clearVertexProgram sceGxmShaderPatcherReleaseVertexProgram(): 0x%X", err);
		goto _fini_error;
	}

	err = sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, _vita2d_colorVertexProgram);
	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("_vita2d_colorVertexProgram sceGxmShaderPatcherReleaseVertexProgram(): 0x%X", err);
		goto _fini_error;
	}

	err = sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, _vita2d_textureVertexProgram);
	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("_vita2d_textureVertexProgram sceGxmShaderPatcherReleaseVertexProgram(): 0x%X", err);
		goto _fini_error;
	}

	_vita2d_free_fragment_programs(&_vita2d_fragmentPrograms.blend_mode_normal);
	_vita2d_free_fragment_programs(&_vita2d_fragmentPrograms.blend_mode_add);

	sceGxmFreeDeviceMemLinux(linearIndicesMem);
	sceGxmFreeDeviceMemLinux(clearIndicesMem);
	sceGxmFreeDeviceMemLinux(clearVerticesMem);

	// wait until display queue is finished before deallocating display buffers
	if (!system_mode_flag) {
		err = sceGxmDisplayQueueFinish();

		if (err != SCE_OK) {
			SCE_DBG_LOG_ERROR("sceGxmDisplayQueueFinish(): 0x%X, Check if sharedfb is running!", err);
			goto _fini_error;
		}
	}

	for (i = 0; i < DISPLAY_BUFFER_COUNT; i++) {
		if (!system_mode_flag) {
			sceGxmFreeDeviceMemLinux(displayBufferMem[i]);

			// destroy the sync object
			err = PVRSRVFreeSyncInfo(psDevData, displayBufferSync[i]);

			if (err != SCE_OK) {
				SCE_DBG_LOG_ERROR("PVRSRVFreeSyncInfo(): 0x%X", err);
				goto _fini_error;
			}
		}
		else {
			// close shared sync object
			err = PVRSRVCloseSharedSyncInfo(psDevData, i + 1, displayBufferSync[i]);

			if (err != SCE_OK) {
				SCE_DBG_LOG_ERROR("PVRSRVCloseSharedSyncInfo(): 0x%X", err);
				goto _fini_error;
			}
		}
	}

	// free the depth and stencil buffer
	sceGxmFreeDeviceMemLinux(depthBufferMem);
	sceGxmFreeDeviceMemLinux(stencilBufferMem);

	// unregister programs and destroy shader patcher
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, clearFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, clearVertexProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, colorFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, colorVertexProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, textureFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, textureTintFragmentProgramId);
	sceGxmShaderPatcherUnregisterProgram(shaderPatcher, textureVertexProgramId);

	err = sceGxmShaderPatcherDestroy(shaderPatcher);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmShaderPatcherDestroy(): 0x%X", err);
		goto _fini_error;
	}

	sceGxmFreeDeviceMemLinux(patcherFragmentUsseMem);
	sceGxmFreeDeviceMemLinux(patcherVertexUsseMem);
	sceGxmFreeDeviceMemLinux(patcherBufferMem);

	// destroy the render target
	err = sceGxmDestroyRenderTarget(renderTarget);

	sceKernelFreeMemBlock(renderTargetMemUid);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmDestroyRenderTarget(): 0x%X", err);
		goto _fini_error;
	}

	// destroy the _vita2d_context
	err = sceGxmDestroyContext(_vita2d_context);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmDestroyContext(): 0x%X", err);
		goto _fini_error;
	}

	if (init_param_s.fragment_usse_ring_buffer_attrib & VITA2D_MEM_ATTRIB_SHARED) {
		sceGxmUnmapFragmentUsseMemory(fragmentUsseRingBufferMem->mappedBase);
		PVRSRVFreeDeviceMem(psDevData, (PVRSRVMemInfoVita *)fragmentUsseRingBufferMem);
	}
	else
		sceGxmFreeDeviceMemLinux((SceGxmDeviceMemInfo *)fragmentUsseRingBufferMem);

	if (init_param_s.fragment_ring_buffer_attrib & VITA2D_MEM_ATTRIB_SHARED) {
		PVRSRVUnmapMemoryFromGpu(psDevData, fragmentRingBufferMem->mappedBase, 0, 0);
		PVRSRVFreeDeviceMem(psDevData, (PVRSRVMemInfoVita *)fragmentRingBufferMem);
	}
	else
		sceGxmFreeDeviceMemLinux((SceGxmDeviceMemInfo *)fragmentRingBufferMem);

	if (init_param_s.vertex_ring_buffer_attrib & VITA2D_MEM_ATTRIB_SHARED) {
		PVRSRVUnmapMemoryFromGpu(psDevData, vertexRingBufferMem->mappedBase, 0, 0);
		PVRSRVFreeDeviceMem(psDevData, (PVRSRVMemInfoVita *)vertexRingBufferMem);
	}
	else
		sceGxmFreeDeviceMemLinux((SceGxmDeviceMemInfo *)vertexRingBufferMem);

	if (init_param_s.vdm_ring_buffer_attrib & VITA2D_MEM_ATTRIB_SHARED) {
		PVRSRVUnmapMemoryFromGpu(psDevData, vdmRingBufferMem->mappedBase, 0, 0);
		PVRSRVFreeDeviceMem(psDevData, (PVRSRVMemInfoVita *)vdmRingBufferMem);
	}
	else
		sceGxmFreeDeviceMemLinux((SceGxmDeviceMemInfo *)vdmRingBufferMem);

	PVRSRVFreeUserModeMem(contextParams.hostMem);
	sceGxmFreeDeviceMemLinux(poolMem);

	if (system_mode_flag) {

		SCE_DBG_LOG_DEBUG("System mode finalize");

		err = PVRSRVUnmapMemoryFromGpu(psDevData, info.frontBuffer, 0, 0);

		if (err != SCE_OK) {
			SCE_DBG_LOG_ERROR("PVRSRVUnmapMemoryFromGpu(): 0x%X", err);
			goto _fini_error;
		}

		sceSharedFbEnd(shfb_id);
		err = sceSharedFbClose(shfb_id);

		if (err != SCE_OK) {
			SCE_DBG_LOG_ERROR("sceSharedFbClose(): 0x%X", err);
			goto _fini_error;
		}
	}

	// terminate libgxm
	err = sceGxmTerminate();

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("sceGxmTerminate(): 0x%X", err);
		goto _fini_error;
	}

	if (pgf_module_was_loaded == SCE_SYSMODULE_LOADED) {

		SCE_DBG_LOG_DEBUG("PGF module was loaded, unloading");

		err = sceSysmoduleUnloadModule(SCE_SYSMODULE_PGF);

		if (err != SCE_OK) {
			SCE_DBG_LOG_ERROR("PGF sceSysmoduleUnloadModule(): 0x%X", err);
			goto _fini_error;
		}
	}

	err = heap_delete_heap(vita2d_heap_internal);

	if (err != SCE_OK) {
		SCE_DBG_LOG_ERROR("heap_delete_heap(): 0x%X", err);
		goto _fini_error;
	}

	vita2d_initialized = 0;

_fini_error:

	return SCE_OK;
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
	sceGxmDraw(_vita2d_context, SCE_GXM_PRIMITIVE_TRIANGLES, SCE_GXM_INDEX_FORMAT_U16, clearIndices, 6);
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

		if (system_mode_flag) {
			sceGxmColorSurfaceInit(
				&displaySurface[bufferIndex],
				info.colorFormat,
				SCE_GXM_COLOR_SURFACE_LINEAR,
				(msaa_s == SCE_GXM_MULTISAMPLE_NONE) ? SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
				SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
				info.width,
				info.height,
				info.stride,
				info.backBuffer);
		}
		else {
			sceGxmColorSurfaceInit(
				&displaySurface[bufferIndex],
				DISPLAY_COLOR_FORMAT,
				SCE_GXM_COLOR_SURFACE_LINEAR,
				(msaa_s == SCE_GXM_MULTISAMPLE_NONE) ? SCE_GXM_COLOR_SURFACE_SCALE_NONE : SCE_GXM_COLOR_SURFACE_SCALE_MSAA_DOWNSCALE,
				SCE_GXM_OUTPUT_REGISTER_SIZE_32BIT,
				display_hres,
				display_vres,
				display_stride,
				displayBufferData[bufferIndex]);
		}

		sceGxmBeginScene(
			_vita2d_context,
			flags,
			renderTarget,
			&validRegion,
			NULL,
			displayBufferSync[bufferIndex],
			&displaySurface[bufferIndex],
			&depthSurface);
	}
	else {
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
	sceGxmPadHeartbeat(&displaySurface[bufferIndex], displayBufferSync[bufferIndex]);

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
	if (drawing) {
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
		if (clipping_enabled) {
			// set the stencil function to only accept pixels where the stencil is 1
			sceGxmSetFrontStencilFunc(
				_vita2d_context,
				SCE_GXM_STENCIL_FUNC_EQUAL,
				SCE_GXM_STENCIL_OP_KEEP,
				SCE_GXM_STENCIL_OP_KEEP,
				SCE_GXM_STENCIL_OP_KEEP,
				0xFF,
				0xFF);
		}
		else {
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

	updateParam.renderTarget.colorFormat = DISPLAY_COLOR_FORMAT;
	updateParam.renderTarget.surfaceType = SCE_GXM_COLOR_SURFACE_LINEAR;
	updateParam.renderTarget.width = display_hres;
	updateParam.renderTarget.height = display_vres;
	updateParam.renderTarget.strideInPixels = display_stride;

	updateParam.renderTarget.colorSurfaceData = displayBufferData[bufferIndex];
	updateParam.renderTarget.depthSurfaceData = depthBufferMem->mappedBase;
	updateParam.displaySyncObject = displayBufferSync[bufferIndex];

	return sceCommonDialogUpdate(&updateParam);
}

void vita2d_set_clear_color(unsigned int color)
{
	clear_color[0] = ((color >> 8 * 0) & 0xFF) / 255.0f;
	clear_color[1] = ((color >> 8 * 1) & 0xFF) / 255.0f;
	clear_color[2] = ((color >> 8 * 2) & 0xFF) / 255.0f;
	clear_color[3] = ((color >> 8 * 3) & 0xFF) / 255.0f;
	clear_color_u = color;
}

void vita2d_set_clear_vertices(vita2d_clear_vertex v0, vita2d_clear_vertex v1, vita2d_clear_vertex v2, vita2d_clear_vertex v3)
{
	clearVertices[0].x = v0.x;
	clearVertices[0].y = v0.y;
	clearVertices[1].x = v1.x;
	clearVertices[1].y = v1.y;
	clearVertices[2].x = v2.x;
	clearVertices[2].y = v2.y;
	clearVertices[3].x = v3.x;
	clearVertices[3].y = v3.y;
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
	if ((pool_index + size) < init_param_s.temp_pool_size) {
		void *addr = (void *)((unsigned int)poolMem->mappedBase + pool_index);
		pool_index += size;
		return addr;
	}
	return NULL;
}

void *vita2d_pool_memalign(unsigned int size, unsigned int alignment)
{
	unsigned int new_index = (pool_index + alignment - 1) & ~(alignment - 1);
	if ((new_index + size) < init_param_s.temp_pool_size) {
		void *addr = (void *)((unsigned int)poolMem->mappedBase + new_index);
		pool_index = new_index + size;
		return addr;
	}
	return NULL;
}

unsigned int vita2d_pool_free_space()
{
	return init_param_s.temp_pool_size - pool_index;
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

int vita2d_check_version(int vita2d_version)
{
	if (vita2d_version == VITA2D_SYS_VERSION_INTERNAL)
		return SCE_OK;
	else
		return VITA2D_SYS_ERROR_VERSION_MISMATCH;
}

#ifdef VITA2D_SYS_PRX

int __module_stop(SceSize argc, const void *args) {
	sceClibPrintf("vita2d_sys module stop\n");
	return SCE_KERNEL_STOP_SUCCESS;
}

int __module_exit() {
	sceClibPrintf("vita2d_sys module exit\n");
	return SCE_KERNEL_STOP_SUCCESS;
}

int __module_start(SceSize argc, void *args) {
	sceClibPrintf("vita2d_sys module start, ver. %d\n", VITA2D_SYS_VERSION_INTERNAL);
	return SCE_KERNEL_START_SUCCESS;
}

#endif

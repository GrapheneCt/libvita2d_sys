#ifndef GIM_H_
#define GIM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <psp2/gxm.h>
#include <psp2/scebase.h>

#include <psp2/kernel/clib.h>
#include <psp2/kernel/sysmem.h>

/** GIM data block types */
#define GIM_BLOCK_TYPE_ROOT 0x02
#define GIM_BLOCK_TYPE_PICTURE 0x03
#define GIM_BLOCK_TYPE_IMAGE 0x04
#define GIM_BLOCK_TYPE_PALETTE 0x05
#define GIM_BLOCK_TYPE_FILEINFO 0xFF

/** GIM texture data formats */
#define GIM_TYPE_INDEX8 0x0005
#define GIM_TYPE_INDEX4 0x0004
#define GIM_TYPE_RGBA8888 0x0003
#define GIM_TYPE_RGBA4444 0x0002
#define GIM_TYPE_RGBA5551 0x0001
#define GIM_TYPE_RGBA5650 0x0000

/** GIM error codes */
typedef int SceGimErrorCode;
#define SCE_GIM_ERROR_INVALID_VALUE        -1
#define SCE_GIM_ERROR_INVALID_POINTER      -2
#define SCE_GIM_ERROR_INVALID_ALIGNMENT    -3
#define SCE_GIM_ERROR_NOT_FOUND            -3

#define SCE_GIM_PALETTE_SIZE_P4    64U
#define SCE_GIM_PALETTE_SIZE_P8    1024U

#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))

/**	A header for a GIM file.
	This structure should be located right at the start of the GIM file.
*/
typedef struct SceGimHeader {
	uint32_t tag;           //!< GIM Identifier
	uint32_t version;       //!< Version number
	uint32_t formatStyle;   //!< GIM file format
	uint32_t pad;           //!< Padding
} SceGimHeader;

/*	Contains information about data block in the GIM file. */
typedef struct SceGimBlockInfo {
	uint16_t blockId;           //!< Block id (level)
	uint16_t unk;               //!< Always 0 on PSP2
	uint32_t blockSize;         //!< Size of this block + children blocks	
	uint32_t blockHeaderNext;   //!< Offset to the next block header
	uint32_t blockDataOffset;   //!< Offset to the data of this block
} SceGimBlockInfo;

/*	Contains information about texture block in the GIM file. */
typedef struct SceGimTextureInfo {
	uint16_t blockHeaderSize;   //!< Size of this header (0x30)
	uint16_t unk0;              //!< Always 0 on PSP2
	uint16_t format;            //!< Texture format
	uint16_t pixelOrder;        //!< Always 0 (normal order) on PSP2
	uint16_t width;             //!< Texture width
	uint16_t height;            //!< Texture height
	uint16_t bppAlign;          //!< Image/palette alignment BPP
	uint16_t pitchAlign;        //!< Image/palette alignment X
	uint16_t heightAlign;       //!< Image/palette alignment Y
	uint16_t unk1;              //!< Always 2
	uint32_t unk3;              //!< Always 0
	uint32_t indexStart;        //!< Index relative start offset
	uint32_t pixelsStart;       //!< First plane/level/frame relative start offset
	uint32_t pixelsEnd;         //!< Last plane/level/frame relative end offset
	uint32_t planeMask;         //!< Always 0 on PSP2
	uint16_t levelType;
	uint16_t levelCount;        //!< Number of mipmaps
	uint16_t frameType;         //!< Always 3
	uint16_t frameCount;        //!< Always 1 on PSP2 (static images)
	uint32_t levelOffset;       //!< Always 0x40 on PSP2
	uint8_t pad[12];            //!< Padding
} SceGimTextureInfo;

#define SCE_GIM_TAG        0x2E47494DUL    /* '.MIG' */
#define SCE_GIM_VERSION    0x312E3030UL

#define SCE_GIM_PALETTE_SIZE_P4    64U
#define SCE_GIM_PALETTE_SIZE_P8    1024U

#ifndef SCE_GIM_ERROR_RETURN
#define SCE_GIM_ERROR_RETURN(COND, ERROR_CODE, FMT, ...) \
	do {                                                 \
		if (!(COND)) { return (ERROR_CODE); }            \
	} while (false)
#endif

#ifndef SCE_GIM_ASSERT
#define SCE_GIM_ASSERT(x) ((void)(x))
#endif

/// Check if a value is aligned to the given power of 2 alignment.
#define SCE_GIM_IS_ALIGNED(VALUE, ALIGNMENT) (((uint32_t)(VALUE) & ((ALIGNMENT) - 1)) == 0)

/** Gets the header size of a GIM file.
	@return					The size of the header in bytes.
*/
uint32_t sceGimGetHeaderSize(void) {
	return sizeof(SceGimHeader);
}

/** Gets the start address of the texture data within a GIM file.
	@param gim				A pointer to the GIM data.
	@return					A pointer to the start of the texture data.
*/
void* sceGimGetDataAddress(void *gim) {
	return (void*)(gim + sceGimGetHeaderSize());
}

/** Gets the size of the texture data within a GIM file.
	@param gim				A pointer to the GIM data.
	@return					The size of the texture data in bytes.
*/
uint32_t sceGimGetDataSize(void *gim) {
	void* data_hdr = sceGimGetDataAddress(gim);
	SceGimBlockInfo *hdr = (SceGimBlockInfo*)data_hdr;
	return hdr->blockSize;
}

/** Gets the size of the whole GIM file.
	@param gim				A pointer to the GIM data.
	@return					The size of the file in bytes.
*/
uint32_t sceGimGetSize(void *gim) {
	return sceGimGetDataSize(gim) + sceGimGetHeaderSize();
}

/** Gets address of the image block.
	@param gim				A pointer to the GIM data.
	@return					A pointer to the start of the image block.
*/
void* sceGimFindImageAddress(void *gim) {

	uint32_t nextHdrOffs = sceGimGetHeaderSize();
	uint32_t gimSize = sceGimGetSize(gim);
	SceGimBlockInfo *hdr;

	while (nextHdrOffs < gimSize) {
		hdr = (SceGimBlockInfo*)(gim + nextHdrOffs);

		if (hdr->blockId == GIM_BLOCK_TYPE_IMAGE)
			return (void *)hdr;

		nextHdrOffs = hdr->blockHeaderNext + nextHdrOffs;
	}

	return NULL;
}

/** Gets address of the palette block.
	@param gim				A pointer to the GIM data.
	@return					A pointer to the start of the palette block.
*/
void* sceGimFindPaletteAddress(void *gim) {

	uint32_t nextHdrOffs = sceGimGetHeaderSize();
	uint32_t gimSize = sceGimGetSize(gim);
	SceGimBlockInfo *hdr;

	while (nextHdrOffs < gimSize) {
		hdr = (SceGimBlockInfo*)(gim + nextHdrOffs);

		if (hdr->blockId == GIM_BLOCK_TYPE_PALETTE)
			return (void *)hdr;

		nextHdrOffs = hdr->blockHeaderNext + nextHdrOffs;
	}

	return NULL;
}

/** Gets the number of textures in a GIM file.
	@return					The number of textures.
*/
inline uint32_t sceGimGetTextureCount(void *gim) {
	return 1;
}

/** Initializes the texture control words for a given GIM texture.

	@param texture			A pointer to texture to be initialized.
	@param gim				A pointer to the GIM data.

	@retval
	SCE_OK The operation was successful.
	@retval
	SCE_GIM_ERROR_INVALID_ALIGNMENT The operation failed due to an invalid buffer alignment.
	@retval
	SCE_GIM_ERROR_INVALID_VALUE The operation failed due to an invalid input parameter.
	@retval
	SCE_GIM_ERROR_INVALID_POINTER The operation failed due to an invalid input pointer.
*/
SceGimErrorCode sceGimInitTexture(SceGxmTexture *texture, void *gim) {

	SceGimTextureInfo *imgHdr = NULL;
	SceGimHeader *hdr = (SceGimHeader*)gim;

	// check parameters
	SCE_GIM_ERROR_RETURN(
		texture,
		SCE_GIM_ERROR_INVALID_POINTER,
		"");
	SCE_GIM_ERROR_RETURN(
		gim,
		SCE_GIM_ERROR_INVALID_POINTER,
		"");

	void* img = sceGimFindImageAddress(gim);

	SCE_GIM_ERROR_RETURN(
		img,
		SCE_GIM_ERROR_NOT_FOUND,
		"");

	void* pal = sceGimFindPaletteAddress(gim);

	if (pal)
		pal = pal + 0x50;

	imgHdr = img + 0x10;
	img = img + 0x50;

	SceGxmTextureFormat texFormat = SCE_GXM_TEXTURE_FORMAT_P8_ABGR;
	uint32_t strideAlign = 16;

	if(hdr->version == SCE_GIM_VERSION) {
		switch(imgHdr->format)
		{
		case GIM_TYPE_INDEX8:
			break;
		case GIM_TYPE_RGBA8888:
			texFormat = SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ABGR;
			strideAlign = 4;
			break;
		case GIM_TYPE_RGBA4444:
			texFormat = SCE_GXM_TEXTURE_FORMAT_U4U4U4U4_ABGR;
			break;
		case GIM_TYPE_RGBA5551:
			texFormat = SCE_GXM_TEXTURE_FORMAT_U1U5U5U5_ABGR;
			break;
		case GIM_TYPE_RGBA5650:
			texFormat = SCE_GXM_TEXTURE_FORMAT_U5U6U5_BGR;
			break;
		default:
			// Unsupported type
			SCE_GIM_ASSERT(0);
		}
	}
	else {
		SCE_GIM_ERROR_RETURN(
			false,
			SCE_GIM_ERROR_INVALID_VALUE,
			"");
	}

	SceGxmErrorCode res = SCE_OK;

	res = sceGxmTextureInitLinear(
		texture,
		img,
		texFormat,
		ALIGN(imgHdr->width, strideAlign),
		imgHdr->height,
		imgHdr->levelCount);

	if (res < 0) {
		return res;
	}

	if (pal) {
		uint32_t pal_size = 0;

		if (GIM_TYPE_INDEX8) {
			pal_size = SCE_GIM_PALETTE_SIZE_P8;
			pal_size = ALIGN(pal_size, 4 * 1024);
		}

		SceUID id = sceKernelAllocMemBlock("gpu_mem", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, pal_size, NULL);

		if (id < 0) {
			return id;
		}

		void* pal_al;

		sceKernelGetMemBlockBase(id, &pal_al);

		res = sceGxmMapMemory(pal_al, pal_size, SCE_GXM_MEMORY_ATTRIB_READ);

		if (res < 0) {
			return res;
		}

		sceClibMemcpy(pal_al, pal, SCE_GIM_PALETTE_SIZE_P8);

		res = sceGxmTextureSetPalette(texture, pal_al);
	}

	return res;
}

/** Checks if a pointer looks like a GIM file.

	@param gim			A pointer to the GIM data.

	@retval
	SCE_OK The data structure passes GIM header validation.
	@retval
	SCE_GIM_ERROR_INVALID_VALUE The operation failed because the header magic number was invalid
	or the version numbers are not compatible.
	@retval
	SCE_GIM_ERROR_INVALID_POINTER The operation failed due to an invalid input pointer.
*/
SceGimErrorCode sceGimCheckData(void *gim) {
	// check parameter
	SCE_GIM_ERROR_RETURN(
		gim,
		SCE_GIM_ERROR_INVALID_POINTER,
		"");

	// check the magic sequence
	SCE_GIM_ERROR_RETURN(
		((SceGimHeader*)gim)->tag == SCE_GIM_TAG,
		SCE_GIM_ERROR_INVALID_VALUE,
		"");

	// check the header version
	SCE_GIM_ERROR_RETURN(
		((SceGimHeader*)gim)->version == SCE_GIM_VERSION,
		SCE_GIM_ERROR_INVALID_VALUE,
		"");

	// all ok
	return SCE_OK;
}

#ifdef __cplusplus
}
#endif

#endif /* GIM_H_ */


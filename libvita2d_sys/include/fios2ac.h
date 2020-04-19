#ifndef FIOS2AC_H
#define FIOS2AC_H

#include <psp2/io/stat.h>
#include <psp2/types.h>

#define MAX_PATH_LENGTH 1024
#define MAX_NAME_LENGTH 256
#define MAX_SHORT_NAME_LENGTH 64

#define SCE_FIOS_FH_SIZE 80
#define SCE_FIOS_DH_SIZE 80
#define SCE_FIOS_OP_SIZE 168
#define SCE_FIOS_CHUNK_SIZE 64

#define SCE_FIOS_ALIGN_UP(val, align) (((val) + ((align) - 1)) & ~((align) - 1))
#define SCE_FIOS_STORAGE_SIZE(num, size) (((num) * (size)) + SCE_FIOS_ALIGN_UP(SCE_FIOS_ALIGN_UP((num), 8) / 8, 8))

#define SCE_FIOS_DH_STORAGE_SIZE(numDHs, pathMax) SCE_FIOS_STORAGE_SIZE(numDHs, SCE_FIOS_DH_SIZE + pathMax)
#define SCE_FIOS_FH_STORAGE_SIZE(numFHs, pathMax) SCE_FIOS_STORAGE_SIZE(numFHs, SCE_FIOS_FH_SIZE + pathMax)
#define SCE_FIOS_OP_STORAGE_SIZE(numOps, pathMax) SCE_FIOS_STORAGE_SIZE(numOps, SCE_FIOS_OP_SIZE + pathMax)
#define SCE_FIOS_CHUNK_STORAGE_SIZE(numChunks) SCE_FIOS_STORAGE_SIZE(numChunks, SCE_FIOS_CHUNK_SIZE)

#define SCE_FIOS_BUFFER_INITIALIZER  { 0, 0 }
#define SCE_FIOS_PSARC_DEARCHIVER_CONTEXT_INITIALIZER { sizeof(SceFiosPsarcDearchiverContext), 0, 0, 0, {0, 0, 0} }
#define SCE_FIOS_PARAMS_INITIALIZER { 0, sizeof(SceFiosParams), 0, 0, 2, 1, 0, 0, 256 * 1024, 2, 0, 0, 0, 0, 0, SCE_FIOS_BUFFER_INITIALIZER, SCE_FIOS_BUFFER_INITIALIZER, SCE_FIOS_BUFFER_INITIALIZER, SCE_FIOS_BUFFER_INITIALIZER, NULL, NULL, NULL, { 66, 189, 66 }, { 0, 0, 0}, { 8 * 1024, 16 * 1024, 8 * 1024}}

typedef SceInt32 SceFiosFH;
typedef SceInt32 SceFiosDH;
typedef SceUInt64 SceFiosDate;
typedef SceInt64 SceFiosOffset;
typedef SceInt64 SceFiosSize;

typedef struct SceFiosPsarcDearchiverContext {
	SceSize sizeOfContext;
	SceSize workBufferSize;
	ScePVoid pWorkBuffer;
	SceIntPtr flags;
	SceIntPtr reserved[3];
} SceFiosPsarcDearchiverContext;

typedef struct SceFiosBuffer {
	ScePVoid pPtr;
	SceSize length;
} SceFiosBuffer;

typedef struct SceFiosParams {
	SceUInt32 initialized : 1;
	SceUInt32 paramsSize : 15;
	SceUInt32 pathMax : 16;
	SceUInt32 profiling;
	SceUInt32 ioThreadCount;
	SceUInt32 threadsPerScheduler;
	SceUInt32 extraFlag1 : 1;
	SceUInt32 extraFlags : 31;
	SceUInt32 maxChunk;
	SceUInt8 maxDecompressorThreadCount;
	SceUInt8 reserved1;
	SceUInt8 reserved2;
	SceUInt8 reserved3;
	SceIntPtr reserved4;
	SceIntPtr reserved5;
	SceFiosBuffer opStorage;
	SceFiosBuffer fhStorage;
	SceFiosBuffer dhStorage;
	SceFiosBuffer chunkStorage;
	ScePVoid pVprintf;
	ScePVoid pMemcpy;
	ScePVoid pProfileCallback;
	SceInt32 threadPriority[3];
	SceInt32 threadAffinity[3];
	SceInt32 threadStackSize[3];
} SceFiosParams;

typedef struct SceFiosDirEntry {
	SceFiosOffset fileSize;
	SceUInt32 statFlags;
	SceUInt16 nameLength;
	SceUInt16 fullPathLength;
	SceUInt16 offsetToName;
	SceUInt16 reserved[3];
	SceByte fullPath[1024];
} SceFiosDirEntry;

typedef struct SceFiosStat {
	SceFiosOffset fileSize;
	SceFiosDate accessDate;
	SceFiosDate modificationDate;
	SceFiosDate creationDate;
	SceUInt32 statFlags;
	SceUInt32 reserved;
	SceInt64 uid;
	SceInt64 gid;
	SceInt64 dev;
	SceInt64 ino;
	SceInt64 mode;
} SceFiosStat;

typedef enum SceFiosWhence {
	SCE_FIOS_SEEK_SET = 0,
	SCE_FIOS_SEEK_CUR = 1,
	SCE_FIOS_SEEK_END = 2
} SceFiosWhence;

SceInt32 sceFiosInitialize(const SceFiosParams* params);
void sceFiosTerminate();

SceFiosSize sceFiosArchiveGetMountBufferSizeSync(const ScePVoid attr, const SceName path, ScePVoid params);
SceInt32 sceFiosArchiveMountSync(const ScePVoid attr, SceFiosFH* fh, const SceName path, const SceName mount_point, SceFiosBuffer mount_buffer, ScePVoid params);
SceInt32 sceFiosArchiveUnmountSync(const ScePVoid attr, SceFiosFH fh);

SceInt32 sceFiosStatSync(const ScePVoid attr, const SceName path, SceFiosStat* stat);

SceInt32 sceFiosFHOpenSync(const ScePVoid attr, SceFiosFH* fh, const SceName path, const ScePVoid params);
SceFiosSize sceFiosFHReadSync(const ScePVoid attr, SceFiosFH fh, ScePVoid data, SceFiosSize size);
SceInt32 sceFiosFHCloseSync(const ScePVoid attr, SceFiosFH fh);
SceFiosOffset sceFiosFHSeek(SceFiosFH fh, SceFiosOffset offset, SceFiosWhence whence);

SceInt32 sceFiosDHOpenSync(const ScePVoid attr, SceFiosDH* dh, const SceName path, SceFiosBuffer buf);
SceInt32 sceFiosDHReadSync(const ScePVoid attr, SceFiosDH dh, SceFiosDirEntry* dir);
SceInt32 sceFiosDHCloseSync(const ScePVoid attr, SceFiosDH dh);

SceDateTime* sceFiosDateToSceDateTime(SceFiosDate date, SceDateTime* sce_date);

SceInt32 sceFiosIOFilterAdd(SceInt32 index, SceVoid(*callback)(), ScePVoid context);
SceInt32 sceFiosIOFilterRemove(SceInt32 index);

void sceFiosIOFilterPsarcDearchiver();

#endif

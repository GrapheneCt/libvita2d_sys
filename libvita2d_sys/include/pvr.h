#ifndef PVR_H
#define PVR_H

/*
	Flags associated with memory allocation
	(bits 0-11)
*/
#define PVRSRV_MEM_READ						(1U<<0)
#define PVRSRV_MEM_WRITE					(1U<<1)
#define PVRSRV_MEM_CACHE_CONSISTENT			(1U<<2)
#define PVRSRV_MEM_NO_SYNCOBJ				(1U<<3)
#define PVRSRV_MEM_INTERLEAVED				(1U<<4)
#define PVRSRV_MEM_DUMMY					(1U<<5)
#define PVRSRV_MEM_EDM_PROTECT				(1U<<6)
#define PVRSRV_MEM_ZERO						(1U<<7)
#define PVRSRV_MEM_USER_SUPPLIED_DEVVADDR	(1U<<8)
#define PVRSRV_MEM_RAM_BACKED_ALLOCATION	(1U<<9)
#define PVRSRV_MEM_NO_RESMAN				(1U<<10)
#define PVRSRV_MEM_EXPORTED					(1U<<11)

/*
	Heap Attribute flags
	(bits 12-23)
*/
#define PVRSRV_HAP_CACHED					(1U<<12)
#define PVRSRV_HAP_UNCACHED					(1U<<13)
#define PVRSRV_HAP_WRITECOMBINE				(1U<<14)
#define PVRSRV_HAP_CACHETYPE_MASK			(PVRSRV_HAP_CACHED|PVRSRV_HAP_UNCACHED|PVRSRV_HAP_WRITECOMBINE)
#define PVRSRV_HAP_KERNEL_ONLY				(1U<<15)
#define PVRSRV_HAP_SINGLE_PROCESS			(1U<<16)
#define PVRSRV_HAP_MULTI_PROCESS			(1U<<17)
#define PVRSRV_HAP_FROM_EXISTING_PROCESS	(1U<<18)
#define PVRSRV_HAP_NO_CPU_VIRTUAL			(1U<<19)
#define PVRSRV_MAP_GC_MMU				(1UL<<20)
#define PVRSRV_HAP_GPU_PAGEABLE				(1U<<21)
#define PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC	(1U<<22)
#define PVRSRV_HAP_MAPTYPE_MASK				(PVRSRV_HAP_KERNEL_ONLY \
                                            |PVRSRV_HAP_SINGLE_PROCESS \
                                            |PVRSRV_HAP_MULTI_PROCESS \
                                            |PVRSRV_HAP_FROM_EXISTING_PROCESS \
                                            |PVRSRV_HAP_NO_CPU_VIRTUAL\
                                            |PVRSRV_HAP_GPU_PAGEABLE \
                                            |PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC)

#define PVRSRV_VITA_GENERIC_MEMORY_ATTRIB (PVRSRV_MEM_READ \
                                          | PVRSRV_MEM_WRITE \
                                          | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC \
                                          | PVRSRV_MEM_CACHE_CONSISTENT \
                                          | PVRSRV_MEM_NO_SYNCOBJ)

typedef struct PVRSRVHeapInfoVita {
	int generalHeapId;
	uint32_t generalHeapSize;
	int pixelshaderHeapId;
	uint32_t pixelshaderHeapSize;
	int vertexshaderHeapId;
	uint32_t vertexshaderHeapSize;
	int syncinfoHeapId;
	uint32_t syncinfoHeapSize;
	int vpbTiledHeapId;
	uint32_t vpbTiledHeapSize;
	int pixelshaderSharedHeapId;
	uint32_t pixelshaderSharedHeapSize;
	int vertexshaderSharedHeapId;
	uint32_t vertexshaderSharedHeapSize;
} PVRSRVHeapInfoVita;

typedef struct PVRSRVMemInfoVita {
	int unk_00;
	void *pMem;
	void *pMem2;
	uint32_t attribs;
	int unk_10;
	uint32_t size;
} PVRSRVMemInfoVita;

void *PVRSRVAllocUserModeMem(uint32_t size);
int PVRSRVAllocSyncInfo(void *psDevData, SceGxmSyncObject **ppsSyncInfo);
int PVRSRVOpenSharedSyncInfo(void *psDevData, int key, SceGxmSyncObject **ppsSyncInfo);

int PVRSRVMapMemoryToGpu(
	void *psDevData,
	int memIndex,
	int heapHandle,
	uint32_t size,
	int sizeLimit,
	void* memBase,
	uint32_t flags,
	uint32_t *internalOffset);

int PVRSRVUnmapMemoryFromGpu(
	void *psDevData,
	void* memBase,
	int heapHandle,
	int a4);

int PVRSRVFreeSyncInfo(void *psDevData, SceGxmSyncObject *psSyncInfo);
int PVRSRVCloseSharedSyncInfo(void *psDevData, int key, SceGxmSyncObject *psSyncInfo);
void PVRSRVFreeUserModeMem(void *memBase);

int PVRSRVAllocDeviceMem(
	void *psDevData,
	int hDevMemHeap,
	uint32_t ui32Attribs,
	uint32_t ui32Size,
	uint32_t ui32Alignment,
	uint32_t ui32ContextFlags,
	PVRSRVMemInfoVita **ppsMemInfo);

int PVRSRVFreeDeviceMem(void *psDevData, PVRSRVMemInfoVita *psMemInfo);

#endif

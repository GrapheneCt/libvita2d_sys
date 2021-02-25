#ifndef HEAP_H
#define HEAP_H

#include <kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* mspace;

typedef struct heap_mspace_link {
	struct heap_mspace_link *next;
	struct heap_mspace_link *prev;

	SceUID	uid;
	SceSize	size;
	mspace	msp;
} heap_mspace_link;

#define HEAP_ERROR_INVALID_ID				-2142306304	/* 0x804F0000 */
#define HEAP_ERROR_INVALID_POINTER			-2142306303	/* 0x804F0001 */

#define HEAP_AUTO_EXTEND		0x0001U

typedef struct heap_opt_param {
	unsigned int size;
} heap_opt_param;

typedef struct heap_alloc_opt_param {
	unsigned int size;
	unsigned int alignment;
} heap_alloc_opt_param;

static __inline__ int _heap_is_pointer_in_bound(const heap_mspace_link *mp, const void *ptr)
{
	if (((const void *)(mp + 1) <= ptr) && (ptr < (const void *)(((const char *)(mp + 1)) + mp->size))) {
		return (1);
	}
	return (0);
}

typedef struct heap_work_internal {
	SceUIntPtr magic;
	int bsize;
	SceKernelLwMutexWork lwmtx;
	char name[32];
	heap_mspace_link prim;
} heap_work_internal;

void *heap_create_heap(const char *name, unsigned int heapblocksize, int flags, const heap_opt_param *optParam);
int   heap_delete_heap(void *heap);
void *heap_alloc_heap_memory(void *heap, unsigned int nbytes);
void *heap_alloc_heap_memory_with_option(void *heap, unsigned int nbytes, const heap_alloc_opt_param *optParam);
int   heap_free_heap_memory(void *heap, void *ptr);
void *heap_realloc_heap_memory(void *heap, void *ptr, unsigned int nbytes);
void *heap_free_heap_memory_with_option(void *heap, void *ptr, unsigned int nbytes, const heap_alloc_opt_param *optParam);

#define HEAP_OFFSET_TO_VALID_HEAP	768
#define HEAP_MSPACE_LINK_OVERHEAD	720

#ifdef __cplusplus
}
#endif

#endif

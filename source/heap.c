#include <psp2/kernel/sysmem.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/types.h>

#include "heap.h"

int _heap_query_block_info(void *heap, void *ptr, unsigned int *puiSize, int *piBlockIndex, heap_mspace_link **msplink);

void *heap_create_heap(const char *name, unsigned int heapblocksize, int flags, const heap_opt_param *optParam)
{
	heap_work_internal	*head;
	heap_mspace_link	*hp;
	SceUID uid;
	void *p;
	int res;
	int i;

	(void)optParam;

	if (((int)heapblocksize) <= 0) {
		return (SCE_NULL);
	}

	if (flags & ~HEAP_AUTO_EXTEND) {
		return (SCE_NULL);
	}

	heapblocksize = ((heapblocksize + 4095) >> 12) << 12;
	uid = sceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, heapblocksize, SCE_NULL);
	if (uid < 0) {
		return (SCE_NULL);
	}
	res = sceKernelGetMemBlockBase(uid, &p);
	if (res < 0) {
		sceKernelFreeMemBlock(uid);
		return (SCE_NULL);
	}

	head        = (heap_work_internal *)p;
	head->bsize = (flags & HEAP_AUTO_EXTEND) ? heapblocksize : 0;
	for(i=0; name[i]!=0 && i<31; i++) {
		head->name[i] = name[i];
	}
	head->name[i] = 0x00;

	res = sceKernelCreateLwMutex(&head->lwmtx, name, SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE | SCE_KERNEL_LW_MUTEX_ATTR_TH_FIFO, 0, SCE_NULL);
	if (res < 0) {
		sceKernelFreeMemBlock(uid);
		return (SCE_NULL);
	}

	hp = &head->prim;
	hp->next = hp;
	hp->prev = hp;
	hp->uid  = uid;
	hp->size = heapblocksize - sizeof(heap_work_internal);
	hp->msp  = sceClibMspaceCreate((hp + 1), hp->size);

	head->magic = (SceUIntPtr)(head + 1);
	return (head);
}

int heap_delete_heap(void *heap)
{
	heap_work_internal	*head;
	heap_mspace_link	*hp, *next;
	int res;

	head = (heap_work_internal *)heap;

	if (head == SCE_NULL) {
		return (HEAP_ERROR_INVALID_ID);
	}
	if (head->magic != (SceUIntPtr)(head + 1)) {
		return (HEAP_ERROR_INVALID_ID);
	}

	res = sceKernelLockLwMutex(&head->lwmtx, 1, SCE_NULL);
	if (res < 0) {
		return (res);
	}

	head->magic = 0;
	sceKernelUnlockLwMutex(&head->lwmtx, 1);

	sceKernelDeleteLwMutex(&head->lwmtx);

	for (hp = head->prim.next; ; hp = next) {
		next = hp->next;
		sceClibMspaceDestroy(hp->msp);
		sceKernelFreeMemBlock(hp->uid);
		if (hp == &(head->prim)) {
			break;
		}
	}

	return 0;
}

void *heap_alloc_heap_memory_with_option(void *heap, unsigned int nbytes, const heap_alloc_opt_param *optParam)
{
	heap_work_internal	*head;
	heap_mspace_link	*hp;
	SceSize	alignment;
	int		res;
	void	*result;

	head = (heap_work_internal *)heap;

	if (head == SCE_NULL) {
		return (SCE_NULL);
	}
	if (head->magic != (SceUIntPtr)(head + 1)) {
		return (SCE_NULL);
	}

	if (optParam != SCE_NULL) {
		if (optParam->size != sizeof(heap_alloc_opt_param)) {
			return (SCE_NULL);
		}

		alignment = optParam->alignment;
		if (alignment == 0 || alignment > 4096 || (alignment % sizeof(int) != 0) || (((alignment - 1) & alignment) != 0)) {
			return (SCE_NULL);
		}
	} else {
		alignment = 0;
	}

	res = sceKernelLockLwMutex(&head->lwmtx, 1, SCE_NULL);
	if (res < 0) {
		return (SCE_NULL);
	}

	result = SCE_NULL;
	for (hp = head->prim.next; ; hp = hp->next) {
		if (alignment != 0) {
			result = sceClibMspaceMemalign(hp->msp, alignment, nbytes);
		} else {
			result = sceClibMspaceMalloc(hp->msp, nbytes);
		}
		if (result != SCE_NULL) {
			sceKernelUnlockLwMutex(&head->lwmtx, 1);
			return (result);
		}
		if (hp == &(head->prim)) {
			break;
		}
	}
	if (head->bsize > 3) {
		SceUID uid;
		void *p;
		unsigned int hsize = (((head->bsize) >> 12) << 12);
		unsigned int nbytes2;

		if (alignment != 0) {
			SceUInt remainder = 0;
			SceUInt extrasize = 0;

			nbytes2 = ((nbytes + 7) >> 3) << 3;
			remainder = HEAP_OFFSET_TO_VALID_HEAP & (alignment - 1);
			if (remainder != 0) {
				extrasize = alignment - remainder;
			}
			if (nbytes2 + extrasize > hsize - HEAP_MSPACE_LINK_OVERHEAD) {
				hsize = nbytes2 + extrasize + HEAP_MSPACE_LINK_OVERHEAD;
				hsize = ((hsize + 4095) >> 12) << 12;
			}
		} else {
			if (nbytes > hsize - HEAP_MSPACE_LINK_OVERHEAD) {
				nbytes2 = ((nbytes + 7) >> 3) << 3;
				hsize = nbytes2 + HEAP_MSPACE_LINK_OVERHEAD;
				hsize = ((hsize + 4095) >> 12) << 12;
			}
		}

		uid = sceKernelAllocMemBlock(head->name, SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, hsize, SCE_NULL);
		if (uid < 0) {
			sceKernelUnlockLwMutex(&head->lwmtx, 1);
			return (SCE_NULL);
		}
		res = sceKernelGetMemBlockBase(uid, &p);
		if (res < 0) {
			sceKernelFreeMemBlock(uid);
			sceKernelUnlockLwMutex(&head->lwmtx, 1);
			return (SCE_NULL);
		}

		hp = (heap_mspace_link *)p;

		if (hp != SCE_NULL) {
			hp->uid  = uid;
			hp->size = hsize - sizeof(heap_mspace_link);
			hp->msp  = sceClibMspaceCreate((hp + 1), hp->size);

			hp->next = head->prim.next;
			hp->prev = head->prim.next->prev;
			head->prim.next->prev = hp;
			hp->prev->next        = hp;

			if (alignment != 0) {
				result = sceClibMspaceMemalign(hp->msp, alignment, nbytes);
			} else {
				result = sceClibMspaceMalloc(hp->msp, nbytes);
			}
		}
	}
	sceKernelUnlockLwMutex(&head->lwmtx, 1);
	return (result);
}

void *heap_alloc_heap_memory(void *heap, unsigned int nbytes)
{
	return (heap_alloc_heap_memory_with_option(heap, nbytes, SCE_NULL));
}

int	heap_free_heap_memory(void *heap, void *ptr)
{
	heap_work_internal	*head;
	heap_mspace_link	*hp;
	int res;

	head = (heap_work_internal *)heap;

	if (head == SCE_NULL) {
		return (HEAP_ERROR_INVALID_ID);
	}
	if (head->magic != (SceUIntPtr)(head + 1)) {
		return (HEAP_ERROR_INVALID_ID);
	}

	res = sceKernelLockLwMutex(&head->lwmtx, 1, SCE_NULL);
	if (res < 0) {
		return (res);
	}

	if (ptr == SCE_NULL) {
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (0);
	}
	for (hp = head->prim.next; ; hp = hp->next) {
		if (_heap_is_pointer_in_bound(hp, ptr)) {
			sceClibMspaceFree(hp->msp, ptr);

			if (hp != &head->prim && sceClibMspaceIsHeapEmpty(hp->msp)) {
				hp->next->prev = hp->prev;
				hp->prev->next = hp->next;
				sceClibMspaceDestroy(hp->msp);
				sceKernelFreeMemBlock(hp->uid);
			}
			sceKernelUnlockLwMutex(&head->lwmtx, 1);
			return (0);
		}
		if (hp == &(head->prim)) {
			break;
		}
	}
	sceKernelUnlockLwMutex(&head->lwmtx, 1);
	return (HEAP_ERROR_INVALID_POINTER);
}

void *heap_free_heap_memory_with_option(void *heap, void *ptr, unsigned int nbytes, const heap_alloc_opt_param *optParam)
{
	heap_work_internal	*head;
	heap_mspace_link	*hp;
	void *newptr;
	SceSize alignment;
	int res;
	unsigned int uiSize;

	if (ptr == SCE_NULL) {
		return (heap_alloc_heap_memory_with_option(heap, nbytes, optParam));
	}
	if (nbytes == 0) {
		heap_free_heap_memory(heap, ptr);
		return (SCE_NULL);
	}

	head = (heap_work_internal *)heap;

	if (head == SCE_NULL) {
		return (SCE_NULL);
	}
	if (head->magic != (SceUIntPtr)(head + 1)) {
		return (SCE_NULL);
	}

	if (optParam != SCE_NULL) {
		if (optParam->size != sizeof(heap_alloc_opt_param)) {
			return (SCE_NULL);
		}

		alignment = optParam->alignment;
		if (alignment == 0 || alignment > 4096 || (alignment % sizeof(int) != 0) || (((alignment - 1) & alignment) != 0)) {
			return (SCE_NULL);
		}
	} else {
		alignment = 0;
	}

	res = sceKernelLockLwMutex(&head->lwmtx, 1, SCE_NULL);
	if (res < 0) {
		return (SCE_NULL);
	}

	res = _heap_query_block_info(heap, ptr, &uiSize, SCE_NULL, &hp);
	if (res < 0) {
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (SCE_NULL);
	}
	if (uiSize == 0) {
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (SCE_NULL);
	}

	if (alignment == 0) {
		newptr = sceClibMspaceRealloc(hp->msp, ptr, nbytes);
	} else {
		newptr = sceClibMspaceReallocalign(hp->msp, ptr, nbytes, alignment);
	}
	if (newptr != SCE_NULL) {
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (newptr);
	}

	newptr = heap_alloc_heap_memory_with_option(heap, nbytes, optParam);
	if (newptr == SCE_NULL) {
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (SCE_NULL);
	}
	sceClibMemcpy(newptr, ptr, uiSize);
	res = heap_free_heap_memory(heap, ptr);

	sceKernelUnlockLwMutex(&head->lwmtx, 1);
	return (newptr);
}

void *heap_realloc_heap_memory(void *heap, void *ptr, unsigned int nbytes)
{
	return (heap_free_heap_memory_with_option(heap, ptr, nbytes, SCE_NULL));
}

int	_heap_query_block_info(void *heap, void *ptr, unsigned int *puiSize, int *piBlockIndex, heap_mspace_link **msplink)
{
	heap_work_internal	*head;
	heap_mspace_link	*hp;
	int res;
	int cnt;

	head = (heap_work_internal *)heap;

	if (head == SCE_NULL) {
		return (HEAP_ERROR_INVALID_ID);
	}
	if (head->magic != (SceUIntPtr)(head + 1)) {
		return (HEAP_ERROR_INVALID_ID);
	}

	res = sceKernelLockLwMutex(&head->lwmtx, 1, SCE_NULL);
	if (res < 0) {
		return (res);
	}

	cnt = 0;
	for (hp = head->prim.prev; ; hp = hp->prev) {
		if (ptr != SCE_NULL) {
			if (_heap_is_pointer_in_bound(hp, ptr)) {
				unsigned int sz;

				sz = sceClibMspaceMallocUsableSize(ptr);
				sceKernelUnlockLwMutex(&head->lwmtx, 1);

				if (puiSize != SCE_NULL) {
					*puiSize = sz;
				}
				if (msplink != SCE_NULL) {
					*msplink = hp;
				}
				if (piBlockIndex != SCE_NULL) {
					if (hp == &(head->prim)) {
						cnt = 0;
					} else {
						cnt++;
					}
					*piBlockIndex = cnt;
				}
				return (1);
			}
		}
		cnt++;
		if (hp == &(head->prim)) {
			break;
		}
	}
	if (piBlockIndex != SCE_NULL) {
		*piBlockIndex = cnt;
	}
	if (ptr == SCE_NULL) {
		sceKernelUnlockLwMutex(&head->lwmtx, 1);
		return (0);
	}
	sceKernelUnlockLwMutex(&head->lwmtx, 1);
	return (HEAP_ERROR_INVALID_ID);
}

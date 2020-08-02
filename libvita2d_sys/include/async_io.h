#ifndef ASYNC_IO_H
#define ASYNC_IO_H

#define MAX_ASYNC_IO_HANDLES 6

SceUID async_io[MAX_ASYNC_IO_HANDLES];

inline int _vita2d_get_free_async_slot(void)
{
	for (int i = 0; i < MAX_ASYNC_IO_HANDLES; i++) {
		if (async_io[i] == 0)
			return i;
	}
	return -1;
}

#endif

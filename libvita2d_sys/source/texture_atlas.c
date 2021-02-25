#include <kernel.h>
#include <libdbg.h>
#include "texture_atlas.h"
#include "heap.h"

extern void* vita2d_heap_internal;

texture_atlas *texture_atlas_create(int width, int height, SceGxmTextureFormat format)
{
	texture_atlas *atlas = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*atlas));
	if (!atlas) {
		SCE_DBG_LOG_ERROR("[ATLAS] heap_alloc_heap_memory() returned NULL");
		return NULL;
	}

	bp2d_rectangle rect;
	rect.x = 0;
	rect.y = 0;
	rect.w = width;
	rect.h = height;

	atlas->texture = vita2d_create_empty_texture_format(width,
							    height,
							    format);
	if (!atlas->texture) {
		SCE_DBG_LOG_ERROR("[ATLAS] vita2d_create_empty_texture_format() returned NULL");
		heap_free_heap_memory(vita2d_heap_internal, atlas);
		return NULL;
	}

	atlas->bp_root = bp2d_create(&rect);
	atlas->htab = int_htab_create(256);

	vita2d_texture_set_filters(atlas->texture,
				   SCE_GXM_TEXTURE_FILTER_POINT,
				   SCE_GXM_TEXTURE_FILTER_LINEAR);

	return atlas;
}

void texture_atlas_free(texture_atlas *atlas)
{
	vita2d_free_texture(atlas->texture);
	bp2d_free(atlas->bp_root);
	int_htab_free(atlas->htab);
	heap_free_heap_memory(vita2d_heap_internal, atlas);
}

int texture_atlas_insert(texture_atlas *atlas, unsigned int character,
			 const bp2d_size *size,
			 const texture_atlas_entry_data *data,
			 bp2d_position *inserted_pos)
{
	atlas_htab_entry *entry;
	bp2d_node *new_node;

	if (!bp2d_insert(atlas->bp_root, size, inserted_pos, &new_node))
		return 0;

	entry = heap_alloc_heap_memory(vita2d_heap_internal, sizeof(*entry));

	entry->rect.x = inserted_pos->x;
	entry->rect.y = inserted_pos->y;
	entry->rect.w = size->w;
	entry->rect.h = size->h;
	entry->data = *data;

	if (!int_htab_insert(atlas->htab, character, entry)) {
		bp2d_delete(atlas->bp_root, new_node);
		return 0;
	}

	return 1;
}

int texture_atlas_exists(texture_atlas *atlas, unsigned int character)
{
	return int_htab_find(atlas->htab, character) != NULL;
}

int texture_atlas_get(texture_atlas *atlas, unsigned int character,
		      bp2d_rectangle *rect, texture_atlas_entry_data *data)
{
	atlas_htab_entry *entry = int_htab_find(atlas->htab, character);
	if (!entry)
		return 0;

	*rect = entry->rect;
	*data = entry->data;

	return 1;
}

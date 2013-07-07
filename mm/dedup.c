/*
 * linux/mm/dedup.c - COW semtantics for identical pages in the pagecache
 *
 * Copyright (C) 2013 Calvin Owens
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THIS SOFTWARE.
 */

/* XXX: This may not need to be a seperate file eventually, but I did that for
 * now just to keep myself organized. */

/* Probably don't need all of these... but I haven't gotten around to checking */
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/mm.h>
#include <linux/kdev_t.h>
#include <linux/gfp.h>
#include <linux/bio.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/blkdev.h>
#include <linux/highmem.h>
#include <linux/prefetch.h>
#include <linux/mpage.h>
#include <linux/writeback.h>
#include <linux/backing-dev.h>
#include <linux/pagevec.h>
#include <linux/cleancache.h>
#include <linux/swap.h>
#include <linux/pagemap.h>
#include <linux/compiler.h>
#include <linux/uaccess.h>
#include <linux/aio.h>
#include <linux/capability.h>
#include <linux/kernel_stat.h>
#include <linux/mman.h>
#include <linux/file.h>
#include <linux/uio.h>
#include <linux/hash.h>
#include <linux/blkdev.h>
#include <linux/security.h>
#include <linux/cpuset.h>
#include <linux/memcontrol.h>
#include <linux/cleancache.h>
#include <linux/dedup.h>

/* This is for non-sparse deduplicated pages */
static int do_data_dedup_cow(struct page **page)
{
	BUG_ON(1); /* Not implemented */
}

/* Largely lifted from __delete_from_page_cache() in mm/filemap.c
 * We need this because dedup'd pages have NULL ->mapping and 0 ->index */
static void dedup_delete_from_page_cache(struct address_space *mapping, pgoff_t index, struct page *page)
{
	cleancache_put_page(page);

	radix_tree_delete(&mapping->page_tree, index);

	mapping->nrpages--;
	__dec_zone_page_state(page, NR_FILE_PAGES);
	if (PageSwapBacked(page))
		__dec_zone_page_state(page, NR_SHMEM);
	BUG_ON(page_mapped(page));
}

/* Largely lifted from replace_page_cache_page() in mm/filemap.c
 * Needed for same reasons as above. */
static int replace_pagecache_dedup_page(struct page *dedup, struct address_space *mapping, pgoff_t offset, struct page *new, gfp_t gfp_mask)
{
	int error;

	error = radix_tree_preload(gfp_mask & ~__GFP_HIGHMEM);
	if (!error) {
		page_cache_get(new);
		new->mapping = mapping;
		new->index = offset;

		spin_lock_irq(&mapping->tree_lock);
		dedup_delete_from_page_cache(mapping, offset, dedup);
		error = radix_tree_insert(&mapping->page_tree, offset, new);
		BUG_ON(error);
		mapping->nrpages++;
		__inc_zone_page_state(new, NR_FILE_PAGES);
		if (PageSwapBacked(new))
			__inc_zone_page_state(new, NR_SHMEM);
		spin_unlock_irq(&mapping->tree_lock);
		//mem_cgroup_replace_page_cache(dedup, new);
		radix_tree_preload_end();
		page_cache_release(dedup);
	}

	return error;
}

/* dedup_replace_in_pagecache - Replace a duplicate page with a dedup page.
 * @page:	Page to be replaced
 * @type:	Only DEDUP_SPARSE is currently supported
 *
 * This function takes a page that contains deduplicatable data, and replaces
 * it with a dedup page in the page cache.
 */
int dedup_replace_in_pagecache(struct page *page, int type)
{
	int err = -ENOSYS;

	switch (type) {
	case DEDUP_HOLE:
		err = replace_page_cache_page(page, ZERO_PAGE(0), GFP_KERNEL); /* Do we need to reimplement this? */
		BUG_ON(err); /* Obviously we don't want this eventually... */
		SetPageDedup(ZERO_PAGE(0));
		break;
	case DEDUP_DATA:
		BUG_ON(1);
	}

	/* Make sure we catch anybody touching these */
	ZERO_PAGE(0)->mapping = NULL;
	ZERO_PAGE(0)->index = 0;

	unlock_page(page);
	return err;
}

/* dedup_do_pagecache_cow - Replace a dedup page with a writable one in the pagecache
 * @mapping:	Address mapping we're working in
 * @page:	Pointer to pointer to page to be replaced, in the calling function
 *
 * This function takes a dedup'd page, copies its data to a fresh page, and replaces
 * the dedup'd page in the radix tree.
 */
int dedup_do_pagecache_cow(struct address_space *mapping, pgoff_t offset, struct page **page, gfp_t gfp_mask)
{
	struct page *new_page = page_cache_alloc_cold(mapping);
	if (!new_page)
		goto err;

	lock_page(new_page);

	if (*page == ZERO_PAGE(0))
		zero_user_segment(new_page, 0, PAGE_CACHE_SIZE);
	else
		do_data_dedup_cow(page);

	if (replace_pagecache_dedup_page(*page, mapping, offset, new_page, gfp_mask))
		goto err;
	lru_cache_add_file(new_page);

	unlock_page(*page); /* This looks bad... */
	*page = new_page; /* ...but it's not */
	return 0;
err:
	/* XXX: Not sure about the ordering here... */
	unlock_page(new_page);
	unlock_page(*page);
	page_cache_release(new_page);
	return -ENOMEM;
}

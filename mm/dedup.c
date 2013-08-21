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

#define DEDUP_ZERO_PAGE_ENTRY ((void*) ((unsigned long) ((ZERO_PAGE(0))) | RADIX_TREE_DEDUP_ENTRY))

/**
 * dedup_undup_hole - Replace @page with the ZERO_PAGE in the radix tree
 * @page:	The all-zero page that should be got rid of
 */
void dedup_undup_hole(struct page *page)
{
	void **slot;

	rcu_read_lock();
	/* @page is locked, so AFAICS this is always safe */
	spin_lock_irq(&page->mapping->tree_lock);
	slot = radix_tree_lookup_slot(&page->mapping->page_tree, page->index);
	radix_tree_replace_slot(slot, DEDUP_ZERO_PAGE_ENTRY);
	spin_unlock_irq(&page->mapping->tree_lock);
	rcu_read_unlock();

	/* This should be done when the ZERO_PAGE is initialized */
	page_cache_get(ZERO_PAGE(0));
	SetPageUptodate(ZERO_PAGE(0));

	/* Free the original page */
	page->mapping = NULL;
	cleancache_put_page(page);
	page_cache_release(page);
	unlock_page(page);
}
EXPORT_SYMBOL_GPL(dedup_replace_in_page_cache);

/**
 * dedup_do_cow - Replace a deduplicated page with a writable copy
 * @mapping:	Address space
 * @index:	Page index to be COW'd
 * @page:	Pointer to page pointer in calling function
 */
void dedup_do_cow(struct address_space *mapping, pgoff_t index, struct page **page)
{
	void **slot;
	struct page *slot_page;
	struct page *new_page = page_cache_alloc_cold(mapping);
	
	if (!new_page)
		goto out;

	new_page->mapping = mapping;
	new_page->index = index;

	spin_lock_irq(&mapping->tree_lock);
	slot = radix_tree_lookup_slot(&mapping->page_tree, index);

	/* Make sure somebody else didn't COW the page while we were waiting on the lock */
	slot_page = radix_tree_deref_slot(slot);
	if (unlikely(slot_page != *page))
		goto bail;
	
	radix_tree_replace_slot(slot, new_page);
	spin_unlock_irq(&mapping->tree_lock);
	
	if (slot_page == ZERO_PAGE(0))
		zero_user_segment(new_page, 0, PAGE_CACHE_SIZE);
	else
		BUG(); /* Not implemented */
out:
	*page = new_page;
	return;
bail:
	spin_unlock_irq(&mapping->tree_lock);
	*page = slot_page;
	return;
}
EXPORT_SYMBOL_GPL(dedup_do_cow);

/**
 * dedup_truncate - Truncate a deduplicated page
 * @mapping:	Address space
 * @index:	Index to truncate
 */
void dedup_truncate(struct address_space *mapping, pgoff_t index)
{
	spin_lock_irq(&mapping->tree_lock);
	radix_tree_delete(&mapping->page_tree, index);
	mapping->nrpages--;
	spin_unlock_irq(&mapping->tree_lock);
}
EXPORT_SYMBOL_GPL(dedup_truncate);

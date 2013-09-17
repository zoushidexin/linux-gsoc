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
#include <linux/fs.h>
#include <linux/mpage.h>
#include <linux/swap.h>
#include <linux/pagemap.h>
#include <linux/compiler.h>
#include <linux/mman.h>
#include <linux/file.h>
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

	/* @page is locked, so AFAICS this is always safe */
	rcu_read_lock();
	spin_lock_irq(&page->mapping->tree_lock);
	slot = radix_tree_lookup_slot(&page->mapping->page_tree, page->index);
	radix_tree_replace_slot(slot, DEDUP_ZERO_PAGE_ENTRY);
	spin_unlock_irq(&page->mapping->tree_lock);
	rcu_read_unlock();

	/* FIXME: This should be done when the ZERO_PAGE is initialized */
	page_cache_get(ZERO_PAGE(0));
	SetPageUptodate(ZERO_PAGE(0));

	/* Free the original page */
	page->mapping = NULL;
	cleancache_put_page(page);
	page_cache_release(page);
}

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
	struct page *new_page = __page_cache_alloc(mapping_gfp_mask(mapping) & ~__GFP_WAIT);
	int r;

	/* If the allocation fails, put NULL, and caller will try again */
	if (!new_page)
		goto out;

	page_cache_get(new_page);
	new_page->mapping = mapping;
	new_page->index = index;

	rcu_read_lock();
	spin_lock_irq(&mapping->tree_lock);

	/* Make sure somebody else didn't COW the page while we were waiting on the lock */
	slot = radix_tree_lookup_slot(&mapping->page_tree, index);
	slot_page = radix_tree_deref_slot(slot);
	radix_tree_dedup_mask(&slot_page);
	if ((r = likely(slot_page == *page)))
		radix_tree_replace_slot(slot, new_page);

	spin_unlock_irq(&mapping->tree_lock);
	rcu_read_unlock();

	if (r) {
		if (slot_page == ZERO_PAGE(0))
			zero_user_segment(new_page, 0, PAGE_CACHE_SIZE);
		else
			BUG();
		SetPageUptodate(new_page);
	}
out:
	*page = new_page;
	return;
}

/**
 * dedup_truncate - Truncate a deduplicated page
 * @mapping:	Address space
 * @index:	Index to truncate
 */
void dedup_truncate(struct address_space *mapping, pgoff_t index)
{
	spin_lock_irq(&mapping->tree_lock);

	/* Delete the dedup reference page */
	radix_tree_delete(&mapping->page_tree, index);

	/* Take care of page accounting */
	mapping->nrpages--;
	__dec_zone_page_state(ZERO_PAGE(0), NR_FILE_PAGES);

	spin_unlock_irq(&mapping->tree_lock);
}

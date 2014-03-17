/*
 * holes.c: Functions for dealing with holes in the page cache, and avoiding
 * backing them with physical pages full of zeros.
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
#include <linux/holes.h>

static unsigned long num_cur_hole_pages = 0; /* Current number of non-backed holes */
static unsigned long num_all_hole_pages = 0; /* Total non-backed holes ever seen */
static unsigned long num_all_holes_cow = 0; /* Total non-backed holes ever COW'd */

void unback_pagecache_hole(struct page *page)
{
	void **slot;

	rcu_read_lock();
	spin_lock_irq(&page->mapping->tree_lock);
	slot = radix_tree_lookup_slot(&page->mapping->page_tree, page->index);
	radix_tree_replace_slot(slot, PAGECACHE_HOLE_MAGIC);
	spin_unlock_irq(&page->mapping->tree_lock);
	rcu_read_unlock();

	/*
	 * No longer referenced from the radix tree. Caller still might have reference
	 * though, depending on how we got here, so this doesn't always free the page
	 */
	page_cache_release(page);
	page->mapping = NULL;
	num_cur_hole_pages++;
	num_all_hole_pages++;
}
EXPORT_SYMBOL_GPL(unback_pagecache_hole);

void cow_pagecache_hole(struct address_space *mapping, pgoff_t index, struct page **page)
{
	void **slot;
	struct page *slot_page, *new_page;
	int r;

	VM_BUG_ON_PAGE(!page_is_hole(*page), *page);

	/* FIXME: How do we handle -ENOMEM here? */
	new_page = __page_cache_alloc(mapping_gfp_mask(mapping) & ~__GFP_WAIT);
	BUG_ON(!new_page);

	/* Increment for the reference about to be put in the radix tree */
	page_cache_get(new_page);
	new_page->mapping = mapping;
	new_page->index = index;

	rcu_read_lock();
	spin_lock_irq(&mapping->tree_lock);

	/* Somebody else could have COW'd the page while we were waiting on the lock */
	slot = radix_tree_lookup_slot(&mapping->page_tree, index);
	slot_page = radix_tree_deref_slot(slot);
	if (likely(slot_page == *page)) {
		zero_user_segment(new_page, 0, PAGE_CACHE_SIZE);
		SetPageUptodate(new_page);
		radix_tree_replace_slot(slot, new_page);
		num_all_holes_cow++;
		num_cur_hole_pages--;
	} else {
		/* Somebody beat us to it: free the page we allocated and return the new one */
		page_cache_release(new_page);
		new_page = slot_page;
	}

	spin_unlock_irq(&mapping->tree_lock);
	rcu_read_unlock();

out:
	*page = new_page;
}
EXPORT_SYMBOL_GPL(cow_pagecache_hole);

void truncate_pagecache_hole(struct address_space *mapping, pgoff_t index)
{
	spin_lock_irq(&mapping->tree_lock);
	radix_tree_delete(&mapping->page_tree, index);
	mapping->nr_pages--;
	__dec_zone_page_state(PAGECACHE_HOLE_MAGIC, NR_FILE_PAGES);
	spin_unlock_irq(&mapping->tree_lock);
	num_cur_hole_pages--;

}
EXPORT_SYMBOL_GPL(truncate_pagecache_hole);

/* This only invoked for not present faults */
int mmap_unbacked_hole_page(struct mm_struct *mm, struct vm_area_struct *vma, unsigned long address, pmd_t *pmd, pgoff_t pgoff, unsigned int flags, pte_t orig_pte)
{
	if (flags & FAULT_FLAG_WRITE) {
		if (vma->vm_flags & VM_SHARED) {
			/* MAP_SHARED|MAP_FILE write fault */
		} else {
			/* MAP_PRIVATE|MAP_FILE write fault */
		}
	} else {
		if (vma->vm_flags & VM_SHARED) {
			/* MAP_SHARED|MAP_FILE read fault */
		} else {
			/* MAP_PRIVATE_MAP_FILE read fault */
		}
	}
}

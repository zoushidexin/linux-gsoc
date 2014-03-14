/*
 * holes.h: Functions for dealing with holes in the page cache, and avoiding
 * backing them with physical pages full of zeros.
 *
 * Copyright (C) 2014 Calvin Owens
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

#include <linux/err.h>

/*
 * This is the "magic" value that will be stored in the address_space radix
 * tree to indicate that a hole exists at a particular page offset. This
 * value was choosen since it will have the same alignment as a valid struct
 * page pointer, and will immediately cause a GPF on every architecture if
 * it is accitendially dereferenced somewhere.
 */
#define PAGECACHE_HOLE_MAGIC (ERR_PTR(-64UL))

/**
 * unback_pagecache_hole - Replace page with hole in its radix tree
 * @page: Pointer to struct page which represents a hole
 **/
void unback_pagecache_hole(struct page *page);

/**
 * cow_pagecache_hole - Replace a hole with a physical page of zeros
 * @mapping: Address space mapping for the file
 * @index: Page index within the file to COW
 * @page: Pointer to pointer to page in calling function
 **/
void cow_pagecache_hole(struct address_space *mapping, pgoff_t index, struct page **page);

/**
 * truncate_pagecache_hole - Remove the hole at the respective page index
 * @mapping: Address space mapping for the file
 * @index: Page index to truncate
 **/
void truncate_pagecache_hole(struct address_space *mapping, pgoff_t index);

static inline int page_is_hole(struct page *p)
{
	return (unsigned long) p == PAGECACHE_HOLE_MAGIC;
}

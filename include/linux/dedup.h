/*
 * linux/include/dedup.h
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

#ifndef _LINUX_DEDUP_H
#define _LINUX_DEDUP_H

#define DEDUP_HOLE 0
#define DEDUP_DATA 1

int dedup_replace_in_pagecache(struct page *page, int type);
int dedup_do_pagecache_cow(struct address_space *mapping, pgoff_t offset, struct page **page, gfp_t gfp_mask);
void dedup_truncate_page(struct page *page, struct address_space *mapping, pgoff_t index, int wait_on_lock);

#endif /* _LINUX_DEDUP_H */

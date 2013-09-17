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
#ifndef _LINUX_DEDUP_H
#define _LINUX_DEDUP_H

void dedup_undup_hole(struct page *page);
void dedup_do_cow(struct address_space *mapping, pgoff_t index, struct page **page);
void dedup_truncate(struct address_space *mapping, pgoff_t index);

#endif /* _LINUX_DEDUP_H */

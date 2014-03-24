/*
 * include/linux/pagevec.h
 *
 * In many places it is efficient to batch an operation up against multiple
 * pages.  A pagevec is a multipage container which is used for that.
 */

#ifndef _LINUX_PAGEVEC_H
#define _LINUX_PAGEVEC_H

/* 14 pointers + two long's align the pagevec structure to a power of two */
#define PAGEVEC_SIZE	14

struct page;
struct address_space;

struct pagevec {
	unsigned long nr;
	unsigned long cold;
	struct page *pages[PAGEVEC_SIZE];
};

void __pagevec_release(struct pagevec *pvec);
void __pagevec_lru_add(struct pagevec *pvec);
unsigned pagevec_lookup(struct pagevec *pvec, struct address_space *mapping,
		pgoff_t start, unsigned nr_pages);
unsigned pagevec_lookup_tag(struct pagevec *pvec,
		struct address_space *mapping, pgoff_t *index, int tag,
		unsigned nr_pages);

static inline void pagevec_init(struct pagevec *pvec, long cold)
{
	pvec->nr = 0;
	pvec->cold = test_bit(0, &cold);
}

static inline void pagevec_reinit(struct pagevec *pvec)
{
	pvec->nr = 0;
	pvec->cold &= 1UL;
}

static inline unsigned pagevec_count(struct pagevec *pvec)
{
	return pvec->nr;
}

static inline unsigned pagevec_space(struct pagevec *pvec)
{
	return PAGEVEC_SIZE - pvec->nr;
}

static inline unsigned pagevec_cold(struct pagevec *pvec)
{
	return test_bit(0, &pvec->cold);
}

/* When truncating pages, we rely on page->index to remove the page from the
 * particular radix tree; however, radix tree entries representing holes have
 * no struct page associated with them. So, in that case, use the upper bits
 * of the ->cold member of struct pagevec to indicate that the struct page
 * pointer at that offset in pages[] is actually the index to truncate, and
 * not a pointer. */
static inline unsigned page_is_offset(unsigned long *cold, int offset)
{
	return test_bit(offset + 1, cold);
}

static inline void mark_page_as_offset(unsigned long *cold, int offset)
{
	set_bit(offset + 1, cold);
}

/*
 * Add a page to a pagevec.  Returns the number of slots still available.
 */
static inline unsigned pagevec_add(struct pagevec *pvec, struct page *page)
{
	pvec->pages[pvec->nr++] = page;
	return pagevec_space(pvec);
}

static inline void pagevec_release(struct pagevec *pvec)
{
	if (pagevec_count(pvec))
		__pagevec_release(pvec);
}

#endif /* _LINUX_PAGEVEC_H */

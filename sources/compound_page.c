#include <memory/allocator/page.h>
#include <memory/page_flags.h>
#include <page.h>

static inline struct page *get_compound_page_head(struct page *page)
{
	unsigned long head = page->head;

	if(head & 1)
		return (struct page *) (head - 1);

	return page;
}

static void set_compound_page_head(struct page *page, struct page *head_page,
				unsigned int order)
{
	unsigned long head;

	head = head_page ? ((unsigned long)head_page | 0x1) : 0;

	for (int i = 0; i < (1 << order); i++) {
		(page + i)->head = head;
	}
}

static inline unsigned int get_compound_page_order(struct page *page)
{
	return page->order;
}

static inline void set_compound_page_order(struct page *page,
					unsigned int order)
{
	page->order = order;
}

/**
 * Convert @vaddr to @page to determine if the @page belongs to a compound page,
 * If yes, return to the head page of the compound page, otherwise return @page
 * directly
 */
struct page *virt_to_head_page(void *vaddr)
{
	struct page *page = virt_to_page(vaddr);

	return get_compound_page_head(page);
}

/**
 * According to the allocation flag @gfp_mask, allocate 2^@order pages,
 * and then retuen start page frame of compound page corresponding
 * struct page pointer
 */
struct page *__alloc_compound_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page;

	page = __alloc_pages(gfp_mask, order);
	if (!page)
		return 0;

	set_compound_page_order(page, order);
	set_compound_page_head(page, page, order);
	set_page_compound(page);

	return page;
}

/**
 * According to the allocation flag @gfp_mask, allocate 2^@order pages,
 * and then retuen start page frame of compound page corresponding
 * virtual address
 */
void *alloc_compound_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page;

	page = __alloc_compound_pages(gfp_mask, order);

	return page_address(page);
}

/**
 * Free with @page corresponding compound page frame
 * to page allocator
 */
void __free_compound_pages(struct page *page)
{
	unsigned int order;

	order = get_compound_page_order(page);

	set_compound_page_head(page, NULL, order);
	clear_page_compound(page);

	__free_pages(page, order);
}

/**
 * Free with virtual address @vaddr corresponding compound page frame
 * to page allocator
 */
void free_compound_pages(void *vaddr)
{
	struct page *page;

	if (vaddr != 0) {
		page = virt_to_head_page(vaddr);
		__free_compound_pages(page);
	}
}

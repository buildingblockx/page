#ifndef __PAGE_H
#define __PAGE_H

#include <memory/allocator/page.h>

#define MAX_ORDER	11

#define PAGE_SHIFT	12
#define PAGE_SIZE	((1UL) << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))

enum pageflags {
	PAGE_locked,
	PAGE_buddy,
	PAGE_slab,
	PAGE_compound,

	MAX_NR_PAGEFLAGS
};

enum migratetype {
	MIGRATE_UNMOVABLE,
	MIGRATE_MOVABLE,

	MIGRATE_TYPES
};

enum zone_type {
	ZONE_DMA,

	/*
	 * Normal addressable memory is in ZONE_NORMAL.
	 */
	ZONE_NORMAL,

	MAX_NR_ZONES
};

/*
 * page->flags layout:
 *
 * ┌─────────┬──────┬─────────────┬─────┬────────────┐
 * | SECTION | ZONE | MIGRATETYPE | ... | PAGE FLAGS |
 * └─────────┴──────┴─────────────┴─────┴────────────┘
 */
#define SECTIONS_SHIFT		0 /* reserve */
#define ZONES_SHIFT		1 /* Determined by MAX_NR_ZONES */
#define MIGRATETYPE_SHIFT	1 /* Determined by MIGRATE_TYPES */

#define SECTIONS_WIDTH		SECTIONS_SHIFT
#define ZONES_WIDTH		ZONES_SHIFT
#define MIGRATETYPE_WIDTH	MIGRATETYPE_SHIFT

#define SECTIONS_PGOFF		((sizeof(unsigned long)*8) - SECTIONS_WIDTH)
#define ZONES_PGOFF		(SECTIONS_PGOFF - ZONES_WIDTH)
#define MIGRATETYPE_PGOFF	(ZONES_PGOFF - MIGRATETYPE_WIDTH)

#define SECTIONS_MASK		((1UL << SECTIONS_SHIFT) - 1)
#define ZONES_MASK		((1UL << ZONES_SHIFT) - 1)
#define MIGRATETYPE_MASK	((1UL << MIGRATETYPE_SHIFT) - 1)

#define PAGE_TYPE_OPS(name)					\
/*								\
 * If @page is in ##name allocator?				\
 * Yes, return true; otherwise false;				\
 */								\
static inline int page_##name(struct page *page)		\
{								\
	return ((page->flags & PAGE_##name) == PAGE_##name);	\
}								\
/*								\
 * Indicates that this page is in the ##name allocator		\
 */								\
static inline void set_page_##name(struct page *page)		\
{								\
	page->flags |= PAGE_##name;				\
}								\
/*								\
 * Indicates that this page is not in the ##name allocator	\
 */								\
static inline void clear_page_##name(struct page *page)		\
{								\
	page->flags &= ~PAGE_##name;				\
}

PAGE_TYPE_OPS(locked)
PAGE_TYPE_OPS(buddy)
PAGE_TYPE_OPS(slab)
PAGE_TYPE_OPS(compound)

struct free_area {
	struct list_head	free_list[MIGRATE_TYPES];
	unsigned long		nr_free;
};

struct zone {
	/* free areas of different sizes */
	struct free_area	free_area[MAX_ORDER];

	unsigned long		start_pfn;
	unsigned long		pages;
};

typedef struct pglist_data {
	struct zone zones[MAX_NR_ZONES];
	int nr_zones;

	struct page *mem_map;
} pg_data_t;

extern pg_data_t pg_data;

#define get_page_private(page)		((page)->private)
#define set_page_private(page, v)	((page)->private = (v))

#define for_each_migratetype_order(order, type) \
	for (order = 0; order < MAX_ORDER; order++) \
		for (type = 0; type < MIGRATE_TYPES; type++)

static inline struct page *get_compound_page_head(struct page *page)
{
	unsigned long head = page->head;

	if(head & 1)
		return (struct page *) (head - 1);

	return page;
}

static inline void set_compound_page_head(struct page *page, struct page *head_page,
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

static inline void set_compound_page_order(struct page *page, unsigned int order)
{
	page->order = order;
}

/*
 * Convert a physical address to/from a Page Frame Number
 */
static inline unsigned long phys_to_pfn(phys_addr_t paddr)
{
	return (unsigned long)(paddr >> PAGE_SHIFT);
}

static inline phys_addr_t pfn_to_phys(unsigned long pfn)
{
	return (phys_addr_t)(pfn << PAGE_SHIFT);
}

static inline unsigned long phys_to_pfn_up(phys_addr_t paddr)
{
	return phys_to_pfn(paddr + PAGE_SIZE-1);
}

static inline unsigned long phys_to_pfn_down(phys_addr_t paddr)
{
	return phys_to_pfn(paddr);
}

/*
 * Convert a Page Frame Number to/from a struct *page
 */
#define PAGE_OFFSET	(KERNEL_RAM_BASE_ADDRESS)
#define PFN_OFFSET	(PAGE_OFFSET >> PAGE_SHIFT)

static inline struct page *pfn_to_page(unsigned long pfn)
{
	return (pg_data.mem_map + (pfn - PFN_OFFSET));
}

static inline unsigned long page_to_pfn(struct page *page)
{
	return (unsigned long)((page - pg_data.mem_map) + PFN_OFFSET);
}

/*
 * Convert a physical address to/from a struct *page
 */
static inline struct page *phys_to_page(phys_addr_t paddr)
{
	return pfn_to_page(phys_to_pfn(paddr));
}

static inline phys_addr_t page_to_phys(struct page *page)
{
	return pfn_to_phys(page_to_pfn(page));
}

/*
 * Convert a virtual address to/from a struct *page
 */
static inline struct page *virt_to_page(virt_addr_t vaddr)
{
	phys_addr_t paddr = virt_to_phys(vaddr);

	return phys_to_page(paddr);
}

static inline struct page *virt_to_head_page(virt_addr_t vaddr)
{
	struct page *page = virt_to_page(vaddr);

	return get_compound_page_head(page);
}

static inline virt_addr_t page_to_virt(struct page *page)
{
	phys_addr_t paddr = page_to_phys(page);

	return phys_to_virt(paddr);
}

#define page_address(page) page_to_virt(page)

#endif /* __PAGE_H */


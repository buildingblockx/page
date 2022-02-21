#include <page.h>
#include <memblock.h>
#include <list.h>

pg_data_t pg_data;

static inline struct page *get_page_from_free_area(struct free_area *area,
					    int migratetype)
{
	return list_first_entry_or_null(&area->free_list[migratetype],
					struct page, list);
}

static inline void del_page_from_free_area(struct page *page,
					struct free_area *area)
{
	list_del(&page->list);
	clear_page_buddy(page);
	set_page_private(page, 0UL);
	area->nr_free--;
}

static inline void add_page_to_free_area(struct page *page, unsigned int order,
				struct free_area *area, int migratetype)
{
	list_add(&page->list, &area->free_list[migratetype]);
	set_page_buddy(page);
	set_page_private(page, order);
	area->nr_free++;
}

static inline void expand(struct zone *zone, struct page *page,
	int low, int high, struct free_area *area, int migratetype)
{
	unsigned long size = 1 << high;

	while (high > low) {
		area--;
		high--;
		size >>= 1;

		add_page_to_free_area(&page[size], high, area, migratetype);
	}
}

/*
 * Look for smallest order of has free pages from @zone and @migratetype,
 * then remove the available page from the freelists and return
 */
static struct page *alloc_page_core(struct zone *zone, unsigned int order,
					int migratetype)
{
	unsigned int current_order;
	struct free_area *area;
	struct page *page;

	/* Find a page of the appropriate size in the preferred list */
	for (current_order = order; current_order < MAX_ORDER; ++current_order) {
		area = &(zone->free_area[current_order]);
		page = get_page_from_free_area(area, migratetype);
		if (!page)
			continue;
		del_page_from_free_area(page, area);
		expand(zone, page, order, current_order, area, migratetype);
		return page;
	}

	return NULL;
}

/*
 * Locate the buddy struct page for both the matching (B1)
 * and the combined order #n+1 parent page.
 *
 * 1) Any buddy B1 will have an same order #n twin B2 which satisfies
 * the following equation:
 *     B2 = B1 ^ (1 << n)
 *
 * 2) Any buddy B1 will have an order #n+1 parent P which satisfies
 * the following equation:
 *     P = B1 & ~(1 << (n + 1))
 *
 * For example, if the starting buddy (B1) is #8, its order #1 buddy is #10,
 * its order #2 parent is #8:
 *     B2 = 8 ^ (1 << 1) = 8 ^ 2 = 10
 *     P  = 8 & ~(1 << 2) = 8
 *
 * For example, if the starting buddy (B1) is #12, its order #1 buddy is #14,
 * its order #2 parent is #8:
 *     B2 = 12 ^ (1 << 1) = 12 ^ 2 = 14
 *     P  = 12 & ~(1 << 2) = 8
 */
static inline unsigned long find_buddy_pfn(unsigned long page_pfn,
						unsigned int order)
{
	return page_pfn ^ (1 << order);
}

/*
 * This function returns the order of a free page in the buddy system.
 */
static inline unsigned int page_order(struct page *page)
{
	return get_page_private(page);
}

static inline enum zone_type page_zonenum(struct page *page)
{
	return (page->flags >> ZONES_PGOFF) & ZONES_MASK;
}

/*
 * This function checks whether we can coalesce a page and its buddy.
 * (a) the buddy pfn greater than the min_pfn
 * (b) the buddy is in the buddy system.
 * (c) a page and its buddy have the same order.
 * (d) a page and its buddy are in the same zone.
 */
static inline int page_is_buddy(struct page *page, struct page *buddy,
						unsigned int order)
{
	if (page_to_pfn(buddy) < min_pfn)
		return 0;

	if (!page_buddy(buddy))
		return 0;

	if (page_order(buddy) != order)
		return 0;

	if (page_zonenum(page) != page_zonenum(buddy))
		return 0;

	return 1;
}

/*
 * Free one page to the corresponding freelist of order.
 *
 * If a page is freed, and its buddy is also free, then this triggers combined
 * into a page of larger size.
 */
static void free_page_core(struct page *page, unsigned long pfn,
		struct zone *zone, unsigned int order, int migratetype)
{
	unsigned long combined_pfn;
	unsigned long buddy_pfn;
	struct page *buddy;

	/*
	 * continue merging
	 */
	while (order < (MAX_ORDER - 1)) {
		buddy_pfn = find_buddy_pfn(pfn, order);
		buddy = page + (buddy_pfn - pfn);

		if (!page_is_buddy(page, buddy, order))
			break;

		/*
		 * Our buddy is free, merge with it and move up one order.
		 */
		del_page_from_free_area(buddy, &zone->free_area[order]);

		combined_pfn = buddy_pfn & pfn;
		page = page + (combined_pfn - pfn);
		pfn = combined_pfn;
		order++;
	}

	/*
	 * done merging
	 */
	add_page_to_free_area(page, order, &zone->free_area[order],
				migratetype);
}

static void alloc_struct_page(void)
{
	struct memblock_region *region;
	struct page *page_array;
	unsigned long pages = 0;
	unsigned long i;

	for_each_memblock_region(i, &memblock.memory, region)
		pages += region->size / PAGE_SIZE;

	page_array = memblock_alloc(pages * sizeof(struct page), PAGE_SIZE);

	pg_data.mem_map = page_array;
}

static int zones_size_init(struct pglist_data *pgdat)
{
	unsigned long max_zone_pfn[MAX_NR_ZONES];
	unsigned long start_pfn, end_pfn;
	unsigned int i;

	max_zone_pfn[ZONE_DMA]		= min(MAX_DMA_PFN, max_pfn);
	max_zone_pfn[ZONE_NORMAL]	= max_pfn;

	start_pfn = min_pfn;

	for (i = 0; i < MAX_NR_ZONES; i++) {
		phys_addr_t base, size;
		int start_region, end_region;
		struct zone *zone;
		unsigned int ret;

		end_pfn = max_zone_pfn[i];

		if (end_pfn < start_pfn)
			continue;

		base = start_pfn << PAGE_SHIFT;
		size = (end_pfn << PAGE_SHIFT) - base;

		/*
		 * This may appear here that the DMA region and the NORMAL region
		 * is in the same regions, and calling memblock_isolate_range()
		 * can isolate to two different regions, but
		 * if you call memblock_add() later, it will cause the two
		 * different regions to be rejoined into a large region again,
		 * because them is close neighbor.
		 */
		ret = memblock_isolate_range(&memblock.memory, base, size,
					&start_region, &end_region);
		if (ret) {
			pr_error("Can't isolate memblock range [0x%lx, 0x%lx)\n",
				base, base + size);
			return 0;
		}

		zone = pgdat->zones + i;
		zone->start_pfn = start_pfn;
		zone->pages = end_pfn - start_pfn;

		start_pfn = end_pfn;
	}
}

static void zone_init_free_lists(struct zone *zone)
{
	unsigned int order, type;

	for_each_migratetype_order(order, type) {
		INIT_LIST_HEAD(&zone->free_area[order].free_list[type]);
		zone->free_area[order].nr_free = 0;
	}
}

static inline void set_page_zone(struct page *page, enum zone_type zone)
{
	page->flags &= ~(ZONES_MASK << ZONES_PGOFF);
	page->flags |= (zone & ZONES_MASK) << ZONES_PGOFF;
}

static inline void set_page_links(struct page *page, enum zone_type zone)
{
	set_page_zone(page, zone);
}

static void init_single_page(struct page *page, enum zone_type zone)
{
	set_page_links(page, zone);

	INIT_LIST_HEAD(&page->list);
}

static void zone_init(unsigned long start_pfn, unsigned long size, enum zone_type zone)
{
	unsigned long pfn, end_pfn = start_pfn + size;
	struct page *page;

	for (pfn = start_pfn; pfn < end_pfn; pfn++) {
		page = pfn_to_page(pfn);
		init_single_page(page, zone);
	}
}

static void zones_init(struct pglist_data *pgdat)
{
	struct zone *zone;
	unsigned long zone_start_pfn, size;
	enum zone_type i;

	zones_size_init(pgdat);

	for (i = 0; i < MAX_NR_ZONES; i++) {
		zone = pgdat->zones + i;
		zone_start_pfn = zone->start_pfn;
		size = zone->pages;

		zone_init_free_lists(zone);
		zone_init(zone_start_pfn, size, i);
	}
}

static unsigned long free_mem_core(phys_addr_t start, phys_addr_t end)
{
	unsigned long start_pfn = phys_to_pfn_up(start);
	unsigned long end_pfn = phys_to_pfn_down(end);
	unsigned long pages = 0;
	int order;

	if (start_pfn >= end_pfn)
		return 0;

	while (start_pfn < end_pfn) {
		order = MAX_ORDER - 1UL;

		while (start_pfn + (1UL << order) > end_pfn)
			order--;

		__free_pages(pfn_to_page(start_pfn), order);
		start_pfn += (1UL << order);
		pages += (1UL << order);
	}

	return pages;
}

/**
 * alloc struct page array and initialize, and then
 * release free pages to the page allocator,
 * and return the number of pages actually released.
 */
unsigned long page_allocator_init(void)
{
	phys_addr_t start, end;
	unsigned long pages = 0;
	u64 i;

	alloc_struct_page();

	zones_init(&pg_data);

	for_each_free_memblock_region(i, &start, &end)
		pages += free_mem_core(start, end);

	return pages;
}

static inline struct zone *gfp_to_zone(const gfp_t gfp_mask)
{
	return &pg_data.zones[ZONE_NORMAL];
}

static inline enum migratetype gfp_to_migratetype(const gfp_t gfp_mask)
{
	return MIGRATE_UNMOVABLE;
}

/**
 * According to the allocation flag @gfp_mask, allocate 2^@order pages,
 * and then retuen start page frame corresponding struct page pointer
 */
struct page *__alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	struct zone *zone = gfp_to_zone(gfp_mask);
	int migratetype = gfp_to_migratetype(gfp_mask);

	return alloc_page_core(zone, order, migratetype);
}

/**
 * According to the allocation flag @gfp_mask, allocate 2^@order pages,
 * and then retuen start page frame corresponding virtual address
 */
virt_addr_t alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page;

	page = __alloc_pages(gfp_mask, order);
	if (!page)
		return 0;

	return page_address(page);
}

static inline struct zone *page_zone(struct page *page)
{
	return &pg_data.zones[page_zonenum(page)];
}

static inline enum migratetype get_page_migratetype(struct page *page)
{
	return MIGRATE_UNMOVABLE;
}

/**
 * Start with @page corresponding page frame,
 * free 2^@order pages
 */
void __free_pages(struct page *page, unsigned int order)
{
	unsigned long pfn = page_to_pfn(page);
	struct zone *zone = page_zone(page);
	int migratetype = get_page_migratetype(page);

	free_page_core(page, pfn, zone, order, migratetype);
}

/**
 * Start with virtual address @vaddr corresponding page frame,
 * free 2^@order pages
 */
void free_pages(virt_addr_t vaddr, unsigned int order)
{
	struct page *page;

	if (vaddr != 0) {
		 page = virt_to_page(vaddr);
		__free_pages(page, order);
	}
}


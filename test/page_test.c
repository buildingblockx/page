#include <memory/allocator/page.h>
#include <print.h>

int page_allocator_test(void)
{
	unsigned long *buffer0, *buffer1, *buffer2;
	unsigned long order;

	pr_info("\npage allocator init\n");
	page_allocator_init();

	order = 0;
	pr_info("allocate %d pages\n", (1 << order));
	buffer0 = alloc_page(0);
	*buffer0 = 0x123456;
	pr_info("\tbuffer0: value 0x%lx, address %p\n", *buffer0, buffer0);

	order = 1;
	pr_info("allocate %d pages\n", (1 << order));
	buffer1 = alloc_pages(0, order);
	*buffer1 = 0x7890;
	pr_info("\tbuffer1: value 0x%lx, address %p\n", *buffer1, buffer1);

	order = 2;
	pr_info("allocate %d pages\n", (1 << order));
	buffer2 = alloc_pages(0, order);
	*buffer2 = 0x1234567890;
	pr_info("\tbuffer2: value 0x%lx, address %p\n", *buffer2, buffer2);

	order = 0;
	pr_info("free %d pages\n", (1 << order));
	free_page(buffer0);

	order = 1;
	pr_info("free %d pages\n", (1 << order));
	free_pages(buffer1, order);

	order = 2;
	pr_info("free %d pages\n", (1 << order));
	free_pages(buffer2, order);

	return 0;
}

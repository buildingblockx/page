# page allocator

## 概述

之前通过`memblock allocator`进行管理所有内存区域，有简单易用的优点，但是有不太灵活、效率低等因素，从而需要一个灵活并且高效率的新内存分配器。于是，以`page`为单位的`page allocator`就诞生了。

## 原理

```
   zone    free_erea     list_head          page
┌────────┐──►┌───┐────►┌───────────┐───►┌───────────┐
│ NORMAL │   │ 0 │     │ UNMOVABLE │    │ 2^0 pages │
│  zone  │   ├───┤     ├───────────┤    ├───────────┤
└────────┘   │ 1 │     │  MOVABLE  │    │ 2^0 pages │
             ├───┤     └───────────┘    ├───────────┤
             │...│                      │    ...    │
             ├───┤                      ├───────────┤
             │ 9 │                      │ 2^0 pages │
             ├───┤                      └───────────┘
             │10 │
             └───┘
```

如上图，`NORMAL zone`的`0`阶链表（迁移类型为`UNMOVABLE`）

所有内存划分为不同的`zone`（`DMA zone`、`NORMAL zone`），不同的`zone`都有不同阶的`free_erea`（阶`order = 0~10`），不同阶的`free_erea`都有不同迁移类型的链表（`UNMOVABLE`、`MOVABLE`），分别指向包含`2^order pages`的`page`。

```
◄────── memblock allocator ────►◄─────────────────── page allocator ───────────────────────►
                    regions             page           list_head       free_erea    zone
                ┌─►┌─────────┐──┬──►┌───────────┐◄───┌───────────┐◄─────┌───┐◄────┌────────┐
     memblock   │  │region[0]│  │   │ 2^0 pages │    │ UNMOVABLE │      │ 0 │     │ NORMAL │
    ┌────────┐  │  ├─────────┤  │   └───────────┘    └───────────┘      ├───┤     │  zone  │
    │        │  │  │region[1]│  ├──►┌───────────┐◄───┌───────────┐◄──┐  │...│     └────────┘
    │ memory ├──┘  ├─────────┤  │   │ 2^3 pages │    │ UNMOVABLE │   └──├───┤
    │        │     │   ...   │  │   └───────────┘    └───────────┘      │ 3 │
    ├────────┤     └─────────┘  ├──►┌───────────┐◄───┌───────────┐◄──┐  ├───┤
    │        │      regions     │   │ 2^9 pages │    │ UNMOVABLE │   │  │...│
    │reserved├────►┌─────────┐  │   └───────────┘    └───────────┘   └──├───┤
    │        │     │region[1]│  └──►┌────────────┐◄──┌───────────┐◄──┐  │ 9 │
    └────────┘     ├─────────┤      │ 2^10 pages │   │ UNMOVABLE │   └──├───┤
                   │region[2]│      └────────────┘   └───────────┘      │10 │
                   ├─────────┤                                          └───┘
                   │   ...   │
                   └─────────┘
```

如上图，`memblock allocator`把`memblock.memory.regions[0]`的所有可用内存释放给`page allocator`的`NORMAL zone`的`0`阶、`3`阶、`9`阶、`10`阶对应的链表（迁移类型为`UNMOVABLE`）

从`memblock.memory`区域中申请一片内存块，用作存储`struct page`并且初始化（如：`page->flags`），接着将`memblock.memory & !memblock.reserve`的所有可用内存拆分成不同`order`阶的页，然后一页一页释放给`page allocator`。

* 如何释放`order`阶的页？

计算需要释放页A的伙伴页B，判断伙伴页B是不是可以合并的伙伴页？

如果是，在order阶的链表删除伙伴页B，同时将需要释放页A与伙伴页B合并成新需要释放页A，以此类推，直至无法合并为至，将新需要释放页A加入对应的`order + n`阶的链表中。

如果不是，直接将需要释放页A加入对应的`order`阶的链表中。

* 如何申请`order`阶的页？

先尝试从`order`阶的链表中获得一页A，判断申请的页A是否为有效页？如果是，直接返回。

如果不是，继续尝试从`current_order = order+1`阶的链表申请页A，以此类推，直至获得有效页为至。如果`current_order > order`, 将多余的内存拆分到`current_order--`阶的链表中，以此类推，直至不能拆分为至

## 如何使用？

使用`page allocator`时，只需要在`memblock allocater`初始化结束后，调用`page_allocator_init()`即可，接下来就可以调用相关API进行分配/释放内存（以`page`为单位）

使用时记得加上头文件：`#include <memory/allocator/page.h>`

## 功能

* 已完成功能：

1. 从`memblock allocator`释放所有`可用`内存到`page allocator`，包括`DMA zone`, `NORMAL zone`
2. 从`[2^0, 2^10] pages`对应的链表中分配/释放内存（以`page`为单位，`one page = 4KB`）

* 未完成功能：

1. 内存地址不连续，出现空洞现象，如 多个NODES、内存热插拔 -> `section`
2. 内存溢出OOM
3. 当内存不足时，进行内存回收
4. 运行时间久后，内存碎片化 -> 迁移类型
5. 复合页面`compound page`
6. 大页`hugepage`

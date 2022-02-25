# compound page subsystem

## 概述

当用户空间的进程申请4MB内存时，首先需要在进程虚拟地址空间申请一块4MB虚拟地址空间，然后当进程真正读/写这4MB内存，触发缺页中断，向内核空间申请物理内存并且映射到进程虚拟地址空间中。

如果没有复合页，可能在向内核空间申请物理内存时，会一页一页申请，然后一页一页映射，效率太低；如果有复合页，直接申请4MB物理内存并且一次性将4KB内存映射到进程虚拟地址空间；于是 compound page subsystem 就诞生了。

## 原理

将`page allocater`分配得到的`2~N`页组合成`一个`复合页

## 如何使用？

使用`compound page subsystem`时，只需要在`page allocater`初始化结束后，调用相关API进行分配/释放内存即可

使用时记得加上头文件：`#include <memory/allocator/compound_page.h>`

## 功能

* 已完成功能：

1. 分配/释放复合页

* 未完成功能：

1.

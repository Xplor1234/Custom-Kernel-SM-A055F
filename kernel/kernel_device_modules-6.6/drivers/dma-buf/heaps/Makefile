# SPDX-License-Identifier: GPL-2.0

ccflags-y += -I$(srctree)/drivers/dma-buf/heaps

# system heap
system_heap-objs := \
	mtk_system_heap.o \
	mtk_page_pool.o \
	mtk_cache_pool.o \
	mtk_dmabuf_monitor.o \
	mtk-deferred-free-helper.o

obj-$(CONFIG_DEVICE_MODULES_DMABUF_HEAPS_DEFERRED_FREE)	+= deferred-free-helper.o
obj-$(CONFIG_DEVICE_MODULES_DMABUF_HEAPS_PAGE_POOL)	+= page_pool.o
obj-$(CONFIG_DEVICE_MODULES_DMABUF_HEAPS_SYSTEM)	+= system_heap.o
obj-$(CONFIG_DEVICE_MODULES_DMABUF_HEAPS_REFILL)	+= mtk_heap_refill.o

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/iommu/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/iommu/arm/arm-smmu-v3/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/trusted_mem/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/monitor_hang/
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/
ifndef CONFIG_GRT_HYPERVISOR
obj-$(CONFIG_MTK_TRUSTED_MEMORY_SUBSYSTEM)	+= mtk_sec_heap.o
endif
obj-$(CONFIG_DMABUF_HEAPS_DEBUG) += mtk_heap_debug.o

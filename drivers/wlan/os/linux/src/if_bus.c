/*
 * Copyright (c) 2004 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
#include <linux/kallsyms.h>
#include <linux/pci.h>
#include <if_ath_ahb.h>
#include <if_ath_pci.h>
#include <if_bus.h>

#define BUS_DMA_FROMDEVICE     0
#define BUS_DMA_TODEVICE       1

/* set bus cachesize in 4B word units */
void bus_dma_sync_single(void *hwdev,
			dma_addr_t dma_handle,
			size_t size, int direction)
{
	osdev_t devhandle = (osdev_t)hwdev;	
    HAL_BUS_CONTEXT	*bc = &(devhandle->bc);
    unsigned long	addr;


    addr = (unsigned long) __va(dma_handle);

    if (!(devhandle->bdev) || bc->bc_bustype == HAL_BUS_TYPE_AHB) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
        dma_cache_sync((void *)addr, size,
            (direction == BUS_DMA_TODEVICE)? DMA_TO_DEVICE : DMA_FROM_DEVICE);
#else
        dma_cache_sync(devhandle->bdev, (void *)addr, size,
            (direction == BUS_DMA_TODEVICE)? DMA_TO_DEVICE : DMA_FROM_DEVICE);
#endif
    } else {

        ath_dma_sync_single(devhandle->bdev, dma_handle, size,
            (direction == BUS_DMA_TODEVICE)? PCI_DMA_TODEVICE : PCI_DMA_FROMDEVICE);
    }
}
EXPORT_SYMBOL(bus_dma_sync_single);

dma_addr_t bus_map_single(void *hwdev, void *ptr,
			 size_t size, int direction)
{
	osdev_t devhandle = (osdev_t)hwdev;	
    HAL_BUS_CONTEXT	*bc = &(devhandle->bc);

    if (!(devhandle->bdev) || bc->bc_bustype == HAL_BUS_TYPE_AHB) {
        dma_map_single(devhandle->bdev, ptr, size,
            (direction == BUS_DMA_TODEVICE)? DMA_TO_DEVICE : DMA_FROM_DEVICE);
    } else {
        pci_map_single(devhandle->bdev, ptr, size,
            (direction == BUS_DMA_TODEVICE)? PCI_DMA_TODEVICE : PCI_DMA_FROMDEVICE);
    }

    return __pa(ptr);
}
EXPORT_SYMBOL(bus_map_single);

void bus_unmap_single(void *hwdev, dma_addr_t dma_addr,
		     size_t size, int direction)
{
	osdev_t devhandle = (osdev_t)hwdev;	
    HAL_BUS_CONTEXT	*bc = &(devhandle->bc);


    if (!(devhandle->bdev) || bc->bc_bustype == HAL_BUS_TYPE_AHB) {
        dma_unmap_single(devhandle->bdev, dma_addr, size,
            (direction == BUS_DMA_TODEVICE)? DMA_TO_DEVICE : DMA_FROM_DEVICE);
    } else {
        pci_unmap_single(devhandle->bdev, dma_addr, size,
            (direction == BUS_DMA_TODEVICE)? PCI_DMA_TODEVICE : PCI_DMA_FROMDEVICE);
    }
}
EXPORT_SYMBOL(bus_unmap_single);

void *
bus_alloc_consistent(void *hwdev, size_t size, dma_addr_t * dma_handle)
{
	osdev_t devhandle = (osdev_t)hwdev;	
    HAL_BUS_CONTEXT	*bc = &(devhandle->bc);


    if (!(devhandle->bdev) || bc->bc_bustype == HAL_BUS_TYPE_AHB) {
        return ahb_alloc_consistent(devhandle->bdev, size, dma_handle);
    } else {
        return pci_alloc_consistent(devhandle->bdev, size, dma_handle);
    }
}

void
bus_free_consistent(void *hwdev, size_t size, void *vaddr, dma_addr_t dma_handle)
{
	osdev_t devhandle = (osdev_t)hwdev;	
    HAL_BUS_CONTEXT	*bc = &(devhandle->bc);


    if (!(devhandle->bdev) || bc->bc_bustype == HAL_BUS_TYPE_AHB) {
        ahb_free_consistent(devhandle->bdev, size, vaddr, dma_handle);
    } else {
        pci_free_consistent(devhandle->bdev, size, vaddr, dma_handle);
    }
}

void *
bus_alloc_nonconsistent(void *hwdev, size_t size, dma_addr_t *dma_handle)
{
	osdev_t devhandle = (osdev_t)hwdev;	
	struct pci_dev *pdev;
	pdev = (struct pci_dev *)devhandle->bdev;
	return dma_alloc_noncoherent(devhandle->bdev == NULL ? NULL:&(pdev->dev), size, dma_handle, GFP_ATOMIC);
}

void
bus_free_nonconsistent(void *hwdev, size_t size, void *vaddr, dma_addr_t dma_handle)
{
	osdev_t devhandle = (osdev_t)hwdev;	
	struct pci_dev *pdev;
	pdev = (struct pci_dev *)devhandle->bdev;
	dma_free_noncoherent(devhandle->bdev == NULL ? NULL:&(pdev->dev), size, vaddr, dma_handle);
}

void
bus_read_cachesize(osdev_t osdev, int *csz, int bustype)
{

    if (bustype == HAL_BUS_TYPE_AHB) {
        ahb_read_cachesize(osdev, csz);
    } else {
       pci_read_cachesize(osdev, csz);
    }
}

#ifdef ATH_BUS_PM
int bus_device_suspend(osdev_t osdev)
{
    HAL_BUS_CONTEXT     *bc = &(osdev->bc);
    int bustype = bc->bc_bustype;

    if (bustype == HAL_BUS_TYPE_PCI) {
      return pci_suspend(osdev);
    } else {
      printk("Not supported on this bus\n");
      return 1;
    }
}

int bus_device_resume(osdev_t osdev)
{
    HAL_BUS_CONTEXT     *bc = &(osdev->bc);
    int bustype = bc->bc_bustype;

    if (bustype == HAL_BUS_TYPE_PCI) {
      return pci_resume(osdev);
    } else {
      printk("Not supported on this bus\n");
      return 1;
    }
}

EXPORT_SYMBOL(bus_device_suspend);
EXPORT_SYMBOL(bus_device_resume);
#endif /* ATH_BUS_PM */

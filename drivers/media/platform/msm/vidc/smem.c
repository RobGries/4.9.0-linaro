/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/dma-attrs.h>
#include <linux/dma-direction.h>
#include <linux/iommu.h>
#include <linux/qcom_iommu.h>
#include <linux/slab.h>
#include <linux/types.h>

#include "smem.h"
#include "resources.h"

static int alloc_dma_mem(struct smem_client *client, size_t size, u32 align,
			 u32 flags, struct smem *mem, int map_kernel)
{
	struct device *dev = client->dev;
	int ret;

	if (align > 1) {
		align = ALIGN(align, SZ_4K);
		size = ALIGN(size, align);
	}

	size = ALIGN(size, SZ_4K);

	mem->flags = flags;
	mem->size = size;
	mem->kvaddr = NULL;
	mem->smem_priv = NULL;

	mem->iommu_dev = dev;
	if (IS_ERR(mem->iommu_dev))
		return PTR_ERR(mem->iommu_dev);

	init_dma_attrs(&mem->attrs);

	if (!map_kernel)
		dma_set_attr(DMA_ATTR_NO_KERNEL_MAPPING, &mem->attrs);

	mem->kvaddr = dma_alloc_attrs(mem->iommu_dev, size, &mem->da,
				      GFP_KERNEL, &mem->attrs);
	if (!mem->kvaddr) {
		dev_err(dev, "cannot allocate dma memory\n");
		return -ENOMEM;
	}

	mem->sgt = kmalloc(sizeof(*mem->sgt), GFP_KERNEL);
	if (!mem->sgt) {
		ret = -ENOMEM;
		goto error_sgt;
	}

	ret = dma_get_sgtable_attrs(mem->iommu_dev, mem->sgt, mem->kvaddr,
				    mem->da, mem->size, &mem->attrs);
	if (ret)
		goto error;

	return 0;
error:
	kfree(mem->sgt);
error_sgt:
	dma_free_attrs(mem->iommu_dev, mem->size, mem->kvaddr,
		       mem->da, &mem->attrs);
	return ret;
}

static void free_dma_mem(struct smem_client *client, struct smem *mem)
{
	dma_free_attrs(mem->iommu_dev, mem->size, mem->kvaddr,
		       mem->da, &mem->attrs);
	sg_free_table(mem->sgt);
	kfree(mem->sgt);
}

static int sync_dma_cache(struct smem *mem, enum smem_cache_ops cache_op)
{
	if (cache_op == SMEM_CACHE_CLEAN) {
		dma_sync_sg_for_device(mem->iommu_dev, mem->sgt->sgl,
				       mem->sgt->nents, DMA_TO_DEVICE);
	} else if (cache_op == SMEM_CACHE_INVALIDATE) {
		dma_sync_sg_for_cpu(mem->iommu_dev, mem->sgt->sgl,
				    mem->sgt->nents, DMA_FROM_DEVICE);
	} else {
		dma_sync_sg_for_device(mem->iommu_dev, mem->sgt->sgl,
				       mem->sgt->nents, DMA_TO_DEVICE);
		dma_sync_sg_for_cpu(mem->iommu_dev, mem->sgt->sgl,
				    mem->sgt->nents, DMA_FROM_DEVICE);
	}

	return 0;
}

int smem_cache_operations(struct smem_client *client, struct smem *mem,
			  enum smem_cache_ops cache_op)
{
	if (!client)
		return -EINVAL;

	return sync_dma_cache(mem, cache_op);
}

struct smem_client *smem_new_client(struct device *dev)
{
	struct smem_client *client;

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return ERR_PTR(-ENOMEM);

	client->dev = dev;

	return client;
}

void smem_delete_client(struct smem_client *client)
{
	if (!client)
		return;

	kfree(client);
}

struct smem *smem_alloc(struct smem_client *client, size_t size, u32 align,
			u32 flags, int map_kernel)
{
	struct smem *mem;
	int ret;

	if (!client || !size)
		return ERR_PTR(-EINVAL);

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem)
		return ERR_PTR(-ENOMEM);

	ret = alloc_dma_mem(client, size, align, flags, mem, map_kernel);
	if (ret) {
		kfree(mem);
		return ERR_PTR(ret);
	}

	return mem;
}

void smem_free(struct smem_client *client, struct smem *mem)
{
	if (!client || !mem)
		return;

	free_dma_mem(client, mem);
	kfree(mem);
};

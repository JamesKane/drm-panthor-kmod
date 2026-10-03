/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 James Kane
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* What io-pgtable needs of the IOMMU API, besides LinuxKPI's. */
#ifndef _PANTHOR_FREEBSD_LINUX_IOMMU_H_
#define	_PANTHOR_FREEBSD_LINUX_IOMMU_H_

#include_next <linux/iommu.h>

#ifndef IOMMU_READ
#define	IOMMU_READ	(1 << 0)
#define	IOMMU_WRITE	(1 << 1)
#define	IOMMU_CACHE	(1 << 2)
#define	IOMMU_NOEXEC	(1 << 3)
#define	IOMMU_MMIO	(1 << 4)
#define	IOMMU_PRIV	(1 << 5)
#endif

struct iommu_iotlb_gather {
	unsigned long	start;
	unsigned long	end;
	size_t		pgsize;
	bool		queued;
};

struct iommu_dirty_bitmap;

static inline void
iommu_iotlb_gather_init(struct iommu_iotlb_gather *gather)
{
	*gather = (struct iommu_iotlb_gather){ .start = ULONG_MAX };
}

static inline bool
iommu_iotlb_gather_queued(struct iommu_iotlb_gather *gather)
{
	return (gather != NULL && gather->queued);
}

#endif

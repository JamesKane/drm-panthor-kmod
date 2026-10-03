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

/*
 * Small Linux interfaces panthor uses that LinuxKPI lacks.  Included before
 * everything else in every source file (see panthor/Makefile).  Generic
 * ones are candidates for LinuxKPI.
 */
#ifndef _PANTHOR_FREEBSD_COMPAT_H_
#define	_PANTHOR_FREEBSD_COMPAT_H_

#ifndef PANTHOR_FBSD_BUS	/* panthor_freebsd_bus.c uses FreeBSD's struct resource */

#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/log2.h>
#include <linux/mutex.h>
#include <linux/refcount.h>
#include <linux/slab.h>
#include <linux/iosys-map.h>
#include <linux/uaccess.h>
#include <linux/ioport.h>
#include <linux/pci.h>		/* IORESOURCE_* */
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <asm/io.h>

struct device;
struct drm_device;
struct drm_gem_object;
struct dma_buf;
struct drm_gpuvm;
struct seq_file;

/* Linux mappings of the direct map; arm64 has one for all of RAM. */
#ifndef __pa
#define	__pa(x)			vtophys(x)
#endif
#ifndef __va
#define	__va(pa)		((void *)PHYS_TO_DMAP(pa))
#endif

#ifndef dma_wmb
#define	dma_wmb()		wmb()
#endif
#define	cmpxchg64_relaxed(p, o, n)	cmpxchg64((p), (o), (n))
#ifndef VM_BUG_ON
#define	VM_BUG_ON(c)		WARN_ON(c)
#endif
#define	const_ilog2(n)		ilog2(n)
#define	time_before64(a, b)	((s64)((a) - (b)) < 0)
#define	time_after64(a, b)	time_before64(b, a)
#define	kmemleak_ignore(p)	((void)(p))
/* The highest user address, for a VM's user range. */
#define	TASK_SIZE_OF(td)	((unsigned long)VM_MAXUSER_ADDRESS)

#ifndef IOSYS_MAP_INIT_VADDR
#define	IOSYS_MAP_INIT_VADDR(v)						\
	((struct iosys_map){ .vaddr = (v), .is_iomem = false })
#endif

#define	readl_relaxed_poll_timeout(addr, val, cond, us, to)		\
	readl_poll_timeout(addr, val, cond, us, to)
#define	readl_relaxed_poll_timeout_atomic(addr, val, cond, us, to)	\
	readl_poll_timeout_atomic(addr, val, cond, us, to)

/* Whether the device's DMA snoops CPU caches (ACPI _CCA). */
enum dev_dma_attr {
	DEV_DMA_NOT_SUPPORTED,
	DEV_DMA_NON_COHERENT,
	DEV_DMA_COHERENT,
};
enum dev_dma_attr device_get_dma_attr(struct device *dev);

/* Decrement unless the count is one: then return false. */
static inline bool
refcount_dec_not_one(refcount_t *r)
{
	unsigned int old;

	old = atomic_read(r);
	do {
		if (old == 1)
			return (false);
	} while (!atomic_try_cmpxchg(r, &old, old - 1));
	return (true);
}

/* Decrement; at zero, return true with the mutex held. */
static inline bool
refcount_dec_and_mutex_lock(refcount_t *r, struct mutex *lock)
{
	if (refcount_dec_not_one(r))
		return (false);
	mutex_lock(lock);
	if (!refcount_dec_and_test(r)) {
		mutex_unlock(lock);
		return (false);
	}
	return (true);
}

static inline int
kmem_cache_alloc_bulk(struct kmem_cache *cache, gfp_t gfp, size_t n,
    void **p)
{
	size_t i;

	for (i = 0; i < n; i++) {
		p[i] = kmem_cache_alloc(cache, gfp);
		if (p[i] == NULL) {
			while (i-- > 0)
				kmem_cache_free(cache, p[i]);
			return (0);
		}
	}
	return (n);
}

static inline void
kmem_cache_free_bulk(struct kmem_cache *cache, size_t n, void **p)
{
	while (n-- > 0)
		kmem_cache_free(cache, p[n]);
}

/* drm-kmod's drm_managed has fewer helpers than Linux's. */
void	*drmm_kzalloc(struct drm_device *, size_t, int);
int	drmm_add_action_or_reset(struct drm_device *,
	    void (*)(struct drm_device *, void *), void *);
#define	drmm_kmalloc(dev, size, gfp)	drmm_kzalloc((dev), (size), (gfp))

static inline void
panthor_fbsd_drmm_mutex_release(struct drm_device *dev __unused, void *lock)
{
	mutex_destroy(lock);
}

static inline int
drmm_mutex_init(struct drm_device *dev, struct mutex *lock)
{
	mutex_init(lock);
	return (drmm_add_action_or_reset(dev, panthor_fbsd_drmm_mutex_release,
	    lock));
}

/*
 * Cache maintenance FreeBSD's DMA tag skips, as the firmware calls the GPU
 * coherent (see device_get_dma_attr() in panthor_freebsd.c).
 */
#include <machine/cpufunc.h>
static inline void
panthor_fbsd_dcache_wbinv(const void *va, size_t len)
{
	cpu_dcache_wbinv_range(__DECONST(void *, va), len);
}

static inline void
panthor_fbsd_dcache_wb(const void *va, size_t len)
{
	cpu_dcache_wb_range(__DECONST(void *, va), len);
}

/* The direct map's address of DMA memory: the GPU has no IOMMU. */
#include <machine/vmparam.h>
#define	panthor_fbsd_dma_va(addr)	((void *)PHYS_TO_DMAP(addr))

struct sg_table;
void panthor_fbsd_sgt_wbinv(struct sg_table *sgt);

/* Linux v7.0 interfaces panthor and its bundled DRM helpers use. */
#include <linux/bits.h>
#include <linux/cleanup.h>
#include <linux/llist.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#ifndef BIT_U64
#define	BIT_U64(n)		(1ULL << (n))
#endif
#ifndef PMD_ORDER
#define	PMD_ORDER		(PMD_SHIFT - PAGE_SHIFT)
#endif
#ifndef GENMASK_U64
#define	GENMASK_U64(h, l)	GENMASK_ULL(h, l)
#endif

#define	init_llist_node(n)	((n)->next = (n))

/* Linux v7.0's typed allocator LinuxKPI lacks. */
#define	kvmalloc_objs(obj, count, ...)					\
	kvmalloc_array((count), sizeof(obj), default_gfp(__VA_ARGS__))

/* drm_gpuvm's lock on a GEM object's VM_BO list (panthor_gem.c). */
struct mutex *panthor_fbsd_gpuva_lock(struct drm_gem_object *obj);

/* The scheduler's check for a dying process, as FreeBSD sees it. */
#define	panthor_fbsd_current_exiting()					\
	((curproc->p_flag & P_WEXIT) != 0)
#define	panthor_fbsd_current_killed()	panthor_fbsd_current_exiting()

DEFINE_LOCK_GUARD_1(spinlock, spinlock_t, spin_lock(_T->lock),
    spin_unlock(_T->lock))

/*
 * Linux 6.10's disable_work*(): no more queueing until re-enabled.
 * LinuxKPI has no such state; cancelling is what panthor needs at
 * teardown, where these are used.
 */
#define	disable_work_sync(w)		(cancel_work_sync(w), true)
#define	disable_delayed_work_sync(w)	(cancel_delayed_work_sync(w), true)
#define	enable_delayed_work(w)		((void)(w), true)

/* No nvmem: the shader core mask is the GPU's own. */
#define	nvmem_cell_read_variable_le_u64(dev, id, val)	(*(val) = 0, -ENOENT)

/* Module parameters (hw.panthor), panthor_freebsd_bus.c. */
#include <sys/sysctl.h>
SYSCTL_DECL(_hw_panthor);

/* drm-kmod (Linux v6.13) names. */
bool panthor_fbsd_prime_exported(struct drm_device *dev,
    struct dma_buf *dma_buf);
#define	drm_gem_is_prime_exported_dma_buf(dev, dma_buf)			\
	panthor_fbsd_prime_exported((dev), (dma_buf))
#define	drm_gem_is_imported(obj)	((obj)->import_attach != NULL)
#define	drm_gem_vmap(obj, map)		drm_gem_vmap_unlocked(obj, map)
#define	drm_gem_vunmap(obj, map)	drm_gem_vunmap_unlocked(obj, map)

/* Kernel mappings of buffers the CPU also caches: arm64 is PIPT. */
#define	flush_kernel_vmap_range(va, size)				\
	panthor_fbsd_dcache_wb((va), (size))
#define	invalidate_kernel_vmap_range(va, size)				\
	panthor_fbsd_dcache_wbinv((va), (size))

/* Tracepoints are not built. */
#define	trace_drm_sched_job_unschedulable_enabled()	false
#define	trace_drm_sched_job_add_dep_enabled()		false

/* drm-kmod builds it only for Linux. */
static inline int
drm_debugfs_gpuva_info(struct seq_file *m __unused,
    struct drm_gpuvm *gpuvm __unused)
{
	return (-ENODEV);
}

/* Runtime PM is built (CONFIG_PM). */
#ifndef pm_ptr
#define	pm_ptr(p)		(p)
#endif

#define	DEFINE_RUNTIME_DEV_PM_OPS(name, sfn, rfn, ifn)			\
const struct dev_pm_ops name = {					\
	RUNTIME_PM_OPS(sfn, rfn, ifn)					\
	.suspend = pm_runtime_force_suspend,				\
	.resume = pm_runtime_force_resume,				\
}

static inline void
panthor_fbsd_pm_runtime_disable(void *dev)
{
	pm_runtime_disable(dev);
}

static inline int
devm_pm_runtime_enable(struct device *dev)
{
	pm_runtime_enable(dev);
	return (devm_add_action_or_reset(dev, panthor_fbsd_pm_runtime_disable,
	    dev));
}

static inline void __iomem *
devm_platform_get_and_ioremap_resource(struct platform_device *pdev,
    unsigned int index, struct resource **res)
{
	if (res != NULL)
		*res = platform_get_resource(pdev, IORESOURCE_MEM, index);
	return (devm_platform_ioremap_resource(pdev, index));
}

/* No 32-bit processes use the GPU. */
#define	TIF_32BIT		0
#define	test_tsk_thread_flag(t, f)	((void)(t), (void)(f), false)

/*
 * Copy a structure from user space that may be smaller or larger than the
 * kernel's: the rest of a smaller one reads as zero; a larger one's extra
 * bytes must be zero.
 */
static inline int
copy_struct_from_user(void *dst, size_t ksize, const void __user *src,
    size_t usize)
{
	size_t n = min(ksize, usize);
	unsigned char c;
	size_t i;

	if (usize > ksize) {
		for (i = ksize; i < usize; i++) {
			if (copy_from_user(&c, (const char __user *)src + i, 1))
				return (-EFAULT);
			if (c != 0)
				return (-E2BIG);
		}
	}
	if (copy_from_user(dst, src, n))
		return (-EFAULT);
	if (ksize > usize)
		memset((char *)dst + n, 0, ksize - n);
	return (0);
}

/* io-pgtable-arm.c: no dirty tracking. */
#define	IOMMU_DIRTY_NO_CLEAR	(1 << 0)
#define	iommu_dirty_bitmap_record(d, iova, size)			\
	((void)(d), (void)(iova), (void)(size))

#endif /* !PANTHOR_FBSD_BUS */
#endif /* !_PANTHOR_FREEBSD_COMPAT_H_ */

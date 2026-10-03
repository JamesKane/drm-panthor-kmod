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
 * The Linux side of the glue: the platform device panthor attaches to,
 * and the Linux interfaces it uses for its clocks, regulators and DMA.
 */

#include <linux/clk.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>

#include <drm/drm_device.h>
#include <drm/drm_gem.h>
#include <drm/drm_prime.h>
#include <linux/dma-buf.h>

#include <arm64/cix/sky1_scmi.h>

#include "panthor_device.h"
#include "panthor_freebsd_bus.h"

/* Platform device */

static struct platform_device *panthor_fbsd_pdev;
static bool panthor_fbsd_pm_held;

static void
panthor_fbsd_pdev_release(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	kfree(pdev->resource);
	kfree(pdev);
}

int
panthor_fbsd_linux_attach(device_t dev, uint64_t pa, uint64_t size,
    const int *irqs)
{
	static const char *irq_names[PANTHOR_FBSD_NIRQS] =
	    { "job", "mmu", "gpu" };
	struct platform_device *pdev;
	struct resource *res;
	int error, i;

	linux_set_current(curthread);
	pdev = kzalloc(sizeof(*pdev), GFP_KERNEL);
	res = kcalloc(1 + PANTHOR_FBSD_NIRQS, sizeof(*res), GFP_KERNEL);
	res[0].start = pa;
	res[0].end = pa + size - 1;
	res[0].flags = IORESOURCE_MEM;
	for (i = 0; i < PANTHOR_FBSD_NIRQS; i++) {
		/* LinuxKPI's request_irq() takes FreeBSD interrupt numbers. */
		res[1 + i].name = irq_names[i];
		res[1 + i].start = res[1 + i].end = irqs[i];
		res[1 + i].flags = IORESOURCE_IRQ;
	}
	pdev->name = "panthor";
	pdev->id = PLATFORM_DEVID_NONE;
	pdev->resource = res;
	pdev->num_resources = 1 + PANTHOR_FBSD_NIRQS;
	/* LinuxKPI names it and sets up DMA through the ACPI device. */
	pdev->dev.bsddev = dev;
	pdev->dev.release = panthor_fbsd_pdev_release;
	error = platform_device_register(pdev);
	if (error != 0) {
		platform_device_put(pdev);
		return (error);
	}
	panthor_fbsd_pdev = pdev;

	/*
	 * Keep the GPU resumed: on suspend, Linux's panthor zaps the user
	 * mappings of its MMIO page through the DRM device's anon_inode,
	 * which FreeBSD has not, and a mapping left to a powered-down GPU
	 * would fault the bus.  So the glue never lets it suspend yet.
	 */
	if (platform_get_drvdata(pdev) != NULL &&
	    pm_runtime_resume_and_get(&pdev->dev) == 0)
		panthor_fbsd_pm_held = true;
	return (0);
}

void
panthor_fbsd_linux_detach(void)
{
	linux_set_current(curthread);
	if (panthor_fbsd_pm_held)
		pm_runtime_put(&panthor_fbsd_pdev->dev);
	panthor_fbsd_pm_held = false;
	if (panthor_fbsd_pdev != NULL)
		platform_device_unregister(panthor_fbsd_pdev);
	panthor_fbsd_pdev = NULL;
}

bool
panthor_fbsd_linux_busy(void)
{
	struct drm_device *ddev;

	if (panthor_fbsd_pdev == NULL)
		return (false);
	ddev = platform_get_drvdata(panthor_fbsd_pdev);
	return (ddev != NULL && atomic_read(&ddev->open_count) > 0);
}

/*
 * Clocks: SCMI clocks, by the names of the GPU's ACPI CLKT entries, which
 * are not panthor's (the default clock is "gpu_clk_core").
 */

struct clk {
	uint32_t	id;
	bool		on;
};

static struct clk *
panthor_fbsd_clk_get(struct device *dev, const char *id)
{
	const char *name;
	struct clk *clk;
	uint32_t scmi_id;

	if (id == NULL || strcmp(id, "core") == 0)
		name = "gpu_clk_core";
	else if (strcmp(id, "stacks") == 0)
		name = "gpu_clk_stacks";
	else
		return (NULL);
	if (panthor_fbsd_clk_id(name, &scmi_id) != 0)
		return (NULL);
	clk = devm_kzalloc(dev, sizeof(*clk), GFP_KERNEL);
	if (clk == NULL)
		return (ERR_PTR(-ENOMEM));
	clk->id = scmi_id;
	/* The FreeBSD side enabled them for the GPU's bring-up. */
	clk->on = true;
	return (clk);
}

struct clk *
devm_clk_get(struct device *dev, const char *id)
{
	struct clk *clk;

	clk = panthor_fbsd_clk_get(dev, id);
	return (clk != NULL ? clk : ERR_PTR(-ENOENT));
}

struct clk *
devm_clk_get_optional(struct device *dev, const char *id)
{
	return (panthor_fbsd_clk_get(dev, id));
}

int
clk_prepare_enable(struct clk *clk)
{
	int error;

	if (clk == NULL || clk->on)
		return (0);
	error = sky1_scmi_clk_enable(clk->id, true);
	if (error == 0)
		clk->on = true;
	return (-error);
}

void
clk_disable_unprepare(struct clk *clk)
{
	if (clk == NULL || !clk->on)
		return;
	if (sky1_scmi_clk_enable(clk->id, false) == 0)
		clk->on = false;
}

unsigned long
clk_get_rate(struct clk *clk)
{
	uint64_t hz;

	if (clk == NULL || sky1_scmi_clk_get_rate(clk->id, &hz) != 0)
		return (0);
	return (hz);
}

int
clk_set_rate(struct clk *clk, unsigned long rate)
{
	if (clk == NULL)
		return (0);
	return (-sky1_scmi_clk_set_rate(clk->id, rate));
}

/*
 * DMA: not coherent, as CIX's Linux runs it.  The GPU speaks ACE-Lite, one
 * way: its reads snoop the CPU's caches, but the CPU's writes do not reach
 * its L2, which then serves stale command streams (DATA_INVALID_FAULT) when
 * buffers are mapped cacheable to it.  So its mappings are uncached, and
 * the CPU's write-combining.
 *
 * The firmware says coherent (_CCA), and FreeBSD's DMA tag for the device
 * follows it, doing no cache maintenance: the glue does what panthor needs
 * of it, writing back and invalidating new GEM and page table pages, and
 * writing back page table updates (panthor_fbsd_dcache_*()).
 */
enum dev_dma_attr
device_get_dma_attr(struct device *dev __unused)
{
	return (DEV_DMA_NON_COHERENT);
}

/*
 * Linux v7.0's drm_gem_is_prime_exported_dma_buf(): drm-kmod's PRIME ops are
 * static, but its release function is exported.
 */
bool
panthor_fbsd_prime_exported(struct drm_device *dev, struct dma_buf *dma_buf)
{
	struct drm_gem_object *obj = dma_buf->priv;

	return (dma_buf->ops->release == drm_gem_dmabuf_release &&
	    obj->dev == dev);
}

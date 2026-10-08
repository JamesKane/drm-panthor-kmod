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
 * panthor's devfreq, in place of panthor_devfreq.c, which needs Linux's
 * devfreq and OPP cores.  The same policy: the scheduler's busy and idle
 * notes give the GPU's utilization, sampled every 50 ms, and devfreq's
 * simple_ondemand governor, with panthor's thresholds, picks the clock
 * from it.  The clock is the SCMI performance domain the GPU's _DSD names
 * "perf" (gpu_core on Sky1), set through sky1_scmi; the firmware sets the
 * voltage with it.  dev.panthor.N.freq_fixed pins it instead.
 */

#include <linux/clk.h>
#include <linux/ktime.h>
#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include <arm64/cix/sky1_scmi.h>

#include "panthor_device.h"
#include "panthor_devfreq.h"
#include "panthor_freebsd_bus.h"

#define	DF_POLL_MS		50	/* ~3 frames, as Linux */
#define	DF_UPTHRESHOLD		45	/* panthor's simple_ondemand */
#define	DF_DOWNDIFFERENTIAL	5
#define	DF_MAXLEVELS		16

static struct panthor_fbsd_df {
	struct panthor_device	*ptdev;
	spinlock_t		lock;		/* the accounting below */
	ktime_t			busy_time;
	ktime_t			idle_time;
	ktime_t			last_update;
	bool			busy;
	struct delayed_work	work;
	bool			running;
	struct mutex		set_lock;	/* cur, setting it */
	uint32_t		domain;
	int			nlevels;	/* 0: no DVFS */
	uint32_t		levels[DF_MAXLEVELS];	/* kHz, ascending */
	uint32_t		cur;		/* kHz */
	uint32_t		fixed;		/* kHz, 0 for the governor */
	int			load;		/* percent, last sample */
} df;

static void
df_update(ktime_t now)
{
	if (df.busy)
		df.busy_time = ktime_add(df.busy_time,
		    ktime_sub(now, df.last_update));
	else
		df.idle_time = ktime_add(df.idle_time,
		    ktime_sub(now, df.last_update));
	df.last_update = now;
}

static void
df_reset(void)
{
	unsigned long flags;

	spin_lock_irqsave(&df.lock, flags);
	df.busy_time = 0;
	df.idle_time = 0;
	df.last_update = ktime_get();
	spin_unlock_irqrestore(&df.lock, flags);
}

/* The lowest level at or above khz (devfreq_recommended_opp()). */
static uint32_t
df_ceil(uint64_t khz)
{
	int i;

	for (i = 0; i < df.nlevels; i++)
		if (df.levels[i] >= khz)
			return (df.levels[i]);
	return (df.levels[df.nlevels - 1]);
}

/* devfreq's simple_ondemand, in kHz. */
static uint32_t
df_ondemand(uint64_t busy, uint64_t total)
{
	uint32_t max = df.levels[df.nlevels - 1];

	if (total == 0)
		return (max);
	if (busy > total)
		busy = total;
	if (busy * 100 > total * DF_UPTHRESHOLD)
		return (max);
	if (busy * 100 > total * (DF_UPTHRESHOLD - DF_DOWNDIFFERENTIAL))
		return (df.cur);
	return (df_ceil((uint64_t)df.cur * busy / total * 100 /
	    (DF_UPTHRESHOLD - DF_DOWNDIFFERENTIAL / 2)));
}

static void
df_set(uint32_t khz)
{
	int error;

	mutex_lock(&df.set_lock);
	if (khz == df.cur) {
		mutex_unlock(&df.set_lock);
		return;
	}
	error = sky1_scmi_perf_set(df.domain, khz);
	if (error == 0)
		df.cur = khz;
	else
		DRM_DEV_ERROR(df.ptdev->base.dev,
		    "cannot set the GPU clock to %u kHz: %d\n", khz, error);
	mutex_unlock(&df.set_lock);
}

static void
df_work(struct work_struct *work __unused)
{
	unsigned long flags;
	uint64_t busy, total;

	spin_lock_irqsave(&df.lock, flags);
	df_update(ktime_get());
	busy = ktime_to_ns(df.busy_time);
	total = ktime_to_ns(ktime_add(df.busy_time, df.idle_time));
	df.busy_time = 0;
	df.idle_time = 0;
	spin_unlock_irqrestore(&df.lock, flags);

	df.load = total != 0 ? busy * 100 / total : 0;
	df_set(df.fixed != 0 ? df.fixed : df_ondemand(busy, total));
	schedule_delayed_work(&df.work, msecs_to_jiffies(DF_POLL_MS));
}

int
panthor_devfreq_init(struct panthor_device *ptdev)
{
	uint32_t khz[DF_MAXLEVELS];
	int error, i, n;

	df.ptdev = ptdev;
	spin_lock_init(&df.lock);
	mutex_init(&df.set_lock);
	INIT_DELAYED_WORK(&df.work, df_work);
	df.nlevels = 0;
	/* Without a performance domain, the clock firmware set. */
	if (panthor_fbsd_perf_domain(&df.domain) != 0)
		return (0);
	error = sky1_scmi_perf_levels(df.domain, khz, DF_MAXLEVELS, &n);
	if (error != 0 || n == 0 ||
	    (error = sky1_scmi_perf_get(df.domain, &df.cur)) != 0) {
		DRM_DEV_INFO(ptdev->base.dev,
		    "no levels for performance domain %u (%d)\n", df.domain,
		    error);
		return (0);
	}
	n = MIN(n, DF_MAXLEVELS);
	/* Ascending, as the firmware gives them; insist. */
	for (i = 0; i < n; i++) {
		if (i > 0 && khz[i] <= khz[i - 1])
			return (0);
		df.levels[i] = khz[i];
	}
	df.nlevels = n;
	ptdev->fast_rate = df.levels[n - 1] * 1000UL;
	df.fixed = 0;
	df_reset();
	df.running = true;
	schedule_delayed_work(&df.work, msecs_to_jiffies(DF_POLL_MS));
	return (0);
}

/* Before panthor goes: the sampling work uses it. */
void
panthor_fbsd_devfreq_fini(void)
{
	if (df.nlevels == 0)
		return;
	df.running = false;
	cancel_delayed_work_sync(&df.work);
	df.nlevels = 0;
	mutex_destroy(&df.set_lock);
}

void
panthor_devfreq_resume(struct panthor_device *ptdev __unused)
{
	if (df.nlevels == 0 || df.running)
		return;
	df_reset();
	df.running = true;
	schedule_delayed_work(&df.work, msecs_to_jiffies(DF_POLL_MS));
}

void
panthor_devfreq_suspend(struct panthor_device *ptdev __unused)
{
	if (df.nlevels == 0 || !df.running)
		return;
	df.running = false;
	cancel_delayed_work_sync(&df.work);
}

void
panthor_devfreq_record_busy(struct panthor_device *ptdev __unused)
{
	unsigned long flags;

	if (df.nlevels == 0)
		return;
	spin_lock_irqsave(&df.lock, flags);
	df_update(ktime_get());
	df.busy = true;
	spin_unlock_irqrestore(&df.lock, flags);
}

void
panthor_devfreq_record_idle(struct panthor_device *ptdev __unused)
{
	unsigned long flags;

	if (df.nlevels == 0)
		return;
	spin_lock_irqsave(&df.lock, flags);
	df_update(ktime_get());
	df.busy = false;
	spin_unlock_irqrestore(&df.lock, flags);
}

unsigned long
panthor_devfreq_get_freq(struct panthor_device *ptdev)
{
	if (df.nlevels != 0)
		return (df.cur * 1000UL);
	return (clk_get_rate(ptdev->clks.core));
}

/* For the sysctls (panthor_freebsd_bus.c). */
int
panthor_fbsd_devfreq_levels(uint32_t *khz, int max)
{
	int i;

	for (i = 0; i < df.nlevels && i < max; i++)
		khz[i] = df.levels[i];
	return (df.nlevels);
}

void
panthor_fbsd_devfreq_state(uint32_t *cur, uint32_t *fixed, int *load)
{
	*cur = df.cur;
	*fixed = df.fixed;
	*load = df.load;
}

int
panthor_fbsd_devfreq_fix(uint32_t khz)
{
	int i;

	if (df.nlevels == 0)
		return (ENXIO);
	for (i = 0; khz != 0 && i < df.nlevels; i++)
		if (df.levels[i] == khz)
			break;
	if (khz != 0 && i == df.nlevels)
		return (EINVAL);
	/* Now: sampling stops while the GPU is suspended. */
	df.fixed = khz;
	if (khz != 0)
		df_set(khz);
	return (0);
}

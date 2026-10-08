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
 * The ACPI GPU device of CIX Sky1 (CIXH5000), a Mali-G720.
 *
 * Its resources: memory 0 is CIX's RCSU, a control block of which only
 * two registers may be read (others hang the bus); memory 1 the GPU;
 * interrupts job, MMU and GPU.  Before any GPU register access, in this
 * order (CIX's GPU development guide, as their Linux does):
 *
 *  1. the GPU power domain, through TF-A;
 *  2. its clocks, gpu_top and gpu_core, which firmware leaves off, through
 *     SCMI (the ids are in the device's ACPI CLKT table);
 *  3. its power resource's _ON (\_SB.GPUP.PPRS): Q-channels, and repair of
 *     the GPU's memories;
 *  4. the GPU's reset, pulsed;
 *  5. Q-channel clock gating, in the RCSU.
 *
 * Then panthor_freebsd.c creates the platform device panthor attaches to.
 * The power domain stays on while the driver is attached; panthor gates
 * the clocks itself, through the glue.
 *
 * This file uses bus resources, so it is built without the Linux headers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <arm64/cix/sky1_scmi.h>

#include "panthor_freebsd_bus.h"

#define	SKY1_PD_GPU		21
#define	SKY1_CLK_GPU_CORE	"gpu_clk_core"
#define	SKY1_CLK_GPU_STACKS	"gpu_clk_stacks"

#define	RCSU_PGCTRL		0x218
#define	 PGCTRL_QCH_CLK_GATE	(1u << 0)

/* The reset controller (ACPI RST0), active low. */
#define	SKY1_RST_PA		0x16000000UL
#define	 RST_S0_DOMAIN_0	0x400
#define	  RST_GPU		(1u << 6)

static struct panthor_fbsd_softc {
	device_t		dev;
	struct resource		*rcsu;
	bool			powered;
	bool			clocked;
} *panthor_fbsd_sc;

static char *panthor_fbsd_acpi_ids[] = { "CIXH5000", NULL };

/* panthor's module parameters, and those of its GPU scheduler. */
SYSCTL_NODE(_hw, OID_AUTO, panthor, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "panthor parameters");

/*
 * The SCMI id of a clock the GPU's CLKT names: packages of the id, the
 * name, and the device.
 */
int
panthor_fbsd_clk_id(const char *name, uint32_t *id)
{
	struct panthor_fbsd_softc *sc = panthor_fbsd_sc;
	ACPI_BUFFER buf;
	ACPI_OBJECT *pkg, *e;
	int error;
	u_int i;

	if (sc == NULL)
		return (ENXIO);
	buf.Pointer = NULL;
	buf.Length = ACPI_ALLOCATE_BUFFER;
	if (ACPI_FAILURE(AcpiEvaluateObject(acpi_get_handle(sc->dev), "CLKT",
	    NULL, &buf)))
		return (ENOENT);
	pkg = buf.Pointer;
	error = ENOENT;
	for (i = 0; pkg->Type == ACPI_TYPE_PACKAGE && i < pkg->Package.Count;
	    i++) {
		e = &pkg->Package.Elements[i];
		if (e->Type != ACPI_TYPE_PACKAGE || e->Package.Count < 2 ||
		    e->Package.Elements[0].Type != ACPI_TYPE_INTEGER ||
		    e->Package.Elements[1].Type != ACPI_TYPE_STRING ||
		    strcmp(e->Package.Elements[1].String.Pointer, name) != 0)
			continue;
		*id = e->Package.Elements[0].Integer.Value;
		error = 0;
		break;
	}
	AcpiOsFree(buf.Pointer);
	return (error);
}

static int
panthor_fbsd_clocks(struct panthor_fbsd_softc *sc, bool on)
{
	static const char *names[] = { SKY1_CLK_GPU_CORE, SKY1_CLK_GPU_STACKS };
	uint32_t id;
	int error, i;

	for (i = 0; i < nitems(names); i++) {
		if ((error = panthor_fbsd_clk_id(names[i], &id)) != 0 ||
		    (error = sky1_scmi_clk_enable(id, on)) != 0) {
			device_printf(sc->dev, "clock %s: %d\n", names[i],
			    error);
			return (error);
		}
	}
	return (0);
}

/* Steps 3-5: the power resource, the reset, Q-channel clock gating. */
static int
panthor_fbsd_gpu_up(struct panthor_fbsd_softc *sc)
{
	ACPI_STATUS st;
	uint8_t *rst;

	st = AcpiEvaluateObject(NULL, "\\_SB.GPUP.PPRS._ON", NULL, NULL);
	if (ACPI_FAILURE(st)) {
		device_printf(sc->dev, "GPU power resource: %s\n",
		    AcpiFormatException(st));
		return (ENXIO);
	}
	rst = pmap_mapdev(SKY1_RST_PA, PAGE_SIZE);
#define	RST(o)	(*(volatile uint32_t *)(rst + (o)))
	RST(RST_S0_DOMAIN_0) &= ~RST_GPU;
	DELAY(20);
	RST(RST_S0_DOMAIN_0) |= RST_GPU;
#undef RST
	pmap_unmapdev(rst, PAGE_SIZE);
	DELAY(20);
	bus_write_4(sc->rcsu, RCSU_PGCTRL,
	    bus_read_4(sc->rcsu, RCSU_PGCTRL) | PGCTRL_QCH_CLK_GATE);
	return (0);
}

static void
panthor_fbsd_teardown(struct panthor_fbsd_softc *sc)
{
	if (sc->clocked)
		(void)panthor_fbsd_clocks(sc, false);
	sc->clocked = false;
	if (sc->powered)
		(void)sky1_scmi_power_set(SKY1_PD_GPU, false);
	sc->powered = false;
	if (sc->rcsu != NULL)
		bus_release_resource(sc->dev, SYS_RES_MEMORY, 0, sc->rcsu);
	sc->rcsu = NULL;
}

/*
 * The SCMI performance domain named "perf": "power-domains" is a package of
 * (reference, domain) pairs, "power-domain-names" their names.
 */
int
panthor_fbsd_perf_domain(uint32_t *domain)
{
	struct panthor_fbsd_softc *sc = panthor_fbsd_sc;
	const ACPI_OBJECT *pds, *names, *e;
	device_t bus;
	uint32_t i, k;

	if (sc == NULL)
		return (ENXIO);
	bus = device_get_parent(sc->dev);
	if (ACPI_FAILURE(ACPI_GET_PROPERTY(bus, sc->dev, "power-domains",
	    &pds)) || pds->Type != ACPI_TYPE_PACKAGE ||
	    ACPI_FAILURE(ACPI_GET_PROPERTY(bus, sc->dev, "power-domain-names",
	    &names)))
		return (ENOENT);
	for (i = 0, k = 0; i + 1 < pds->Package.Count; i++) {
		e = &pds->Package.Elements[i];
		if (e->Type != ACPI_TYPE_LOCAL_REFERENCE)
			continue;
		e++;
		if (e->Type != ACPI_TYPE_INTEGER)
			return (ENOENT);
		/* The k-th pair's name: a string, or a package of them. */
		if ((names->Type == ACPI_TYPE_STRING && k == 0 &&
		    strcmp(names->String.Pointer, "perf") == 0) ||
		    (names->Type == ACPI_TYPE_PACKAGE &&
		    k < names->Package.Count &&
		    names->Package.Elements[k].Type == ACPI_TYPE_STRING &&
		    strcmp(names->Package.Elements[k].String.Pointer,
		    "perf") == 0)) {
			*domain = e->Integer.Value;
			return (0);
		}
		k++;
	}
	return (ENOENT);
}

static int
panthor_fbsd_freq_sysctl(SYSCTL_HANDLER_ARGS)
{
	uint32_t cur, fixed;
	int load, val;

	panthor_fbsd_devfreq_state(&cur, &fixed, &load);
	val = arg2 == 0 ? cur / 1000 : arg2 == 1 ? load : fixed / 1000;
	return (sysctl_handle_int(oidp, &val, 0, req));
}

static int
panthor_fbsd_fixed_sysctl(SYSCTL_HANDLER_ARGS)
{
	uint32_t cur, fixed;
	int error, load, mhz;

	panthor_fbsd_devfreq_state(&cur, &fixed, &load);
	mhz = fixed / 1000;
	error = sysctl_handle_int(oidp, &mhz, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (mhz < 0)
		return (EINVAL);
	return (panthor_fbsd_devfreq_fix(mhz * 1000));
}

static int
panthor_fbsd_levels_sysctl(SYSCTL_HANDLER_ARGS)
{
	uint32_t khz[16];
	struct sbuf sb;
	int error, i, n;

	n = MIN(panthor_fbsd_devfreq_levels(khz, nitems(khz)), nitems(khz));
	sbuf_new_for_sysctl(&sb, NULL, 64, req);
	for (i = 0; i < n; i++)
		sbuf_printf(&sb, "%s%u", i > 0 ? " " : "", khz[i] / 1000);
	error = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return (error);
}

/*
 * The GPU's clock, if the firmware gives it a performance domain.  panthor
 * may not have probed yet (LinuxKPI defers it at boot): until it has, and
 * if its devfreq finds no levels, they read as none.
 */
static void
panthor_fbsd_devfreq_sysctls(device_t dev)
{
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *kids;
	uint32_t domain;

	if (panthor_fbsd_perf_domain(&domain) != 0)
		return;
	ctx = device_get_sysctl_ctx(dev);
	kids = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));
	SYSCTL_ADD_PROC(ctx, kids, OID_AUTO, "freq",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
	    panthor_fbsd_freq_sysctl, "I", "GPU clock (MHz)");
	SYSCTL_ADD_PROC(ctx, kids, OID_AUTO, "load",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 1,
	    panthor_fbsd_freq_sysctl, "I", "GPU busy in the last 50 ms (%)");
	SYSCTL_ADD_PROC(ctx, kids, OID_AUTO, "freq_fixed",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, 0,
	    panthor_fbsd_fixed_sysctl, "I",
	    "GPU clock pinned (MHz, one of freq_levels), 0 to follow the load");
	SYSCTL_ADD_PROC(ctx, kids, OID_AUTO, "freq_levels",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
	    panthor_fbsd_levels_sysctl, "A", "GPU clock levels (MHz)");
}

static int
panthor_fbsd_probe(device_t dev)
{
	int rv;

	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, panthor_fbsd_acpi_ids,
	    NULL);
	if (rv <= 0)
		device_set_desc(dev, "Arm Mali GPU (CIX Sky1)");
	return (rv);
}

static int
panthor_fbsd_attach(device_t dev)
{
	struct panthor_fbsd_softc *sc = device_get_softc(dev);
	rman_res_t start, count, gpu_pa, gpu_size;
	int error, irqs[PANTHOR_FBSD_NIRQS], i, rid;

	if (panthor_fbsd_sc != NULL)
		return (ENXIO);
	sc->dev = dev;
	panthor_fbsd_sc = sc;

	rid = 0;
	sc->rcsu = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->rcsu == NULL ||
	    bus_get_resource(dev, SYS_RES_MEMORY, 1, &gpu_pa, &gpu_size) != 0) {
		device_printf(dev, "no registers\n");
		error = ENXIO;
		goto fail;
	}
	for (i = 0; i < PANTHOR_FBSD_NIRQS; i++) {
		if (bus_get_resource(dev, SYS_RES_IRQ, i, &start, &count) != 0) {
			device_printf(dev, "no interrupt %d\n", i);
			error = ENXIO;
			goto fail;
		}
		irqs[i] = start;
	}

	if ((error = sky1_scmi_power_set(SKY1_PD_GPU, true)) != 0) {
		device_printf(dev, "cannot power the GPU: %d\n", error);
		goto fail;
	}
	sc->powered = true;
	if ((error = panthor_fbsd_clocks(sc, true)) != 0)
		goto fail;
	sc->clocked = true;
	if ((error = panthor_fbsd_gpu_up(sc)) != 0)
		goto fail;

	/* The clocks are panthor's from now on, through clk_*(). */
	sc->clocked = false;
	error = -panthor_fbsd_linux_attach(dev, gpu_pa, gpu_size, irqs);
	if (error != 0) {
		panthor_fbsd_devfreq_fini();
		panthor_fbsd_linux_detach();
		sc->clocked = true;
		goto fail;
	}
	panthor_fbsd_devfreq_sysctls(dev);
	return (0);
fail:
	panthor_fbsd_teardown(sc);
	panthor_fbsd_sc = NULL;
	return (error);
}

static int
panthor_fbsd_detach(device_t dev)
{
	struct panthor_fbsd_softc *sc = device_get_softc(dev);

	/* Closing files later would call into the unloaded module. */
	if (panthor_fbsd_linux_busy())
		return (EBUSY);
	panthor_fbsd_devfreq_fini();
	panthor_fbsd_linux_detach();
	panthor_fbsd_teardown(sc);
	panthor_fbsd_sc = NULL;
	return (0);
}

static device_method_t panthor_fbsd_methods[] = {
	DEVMETHOD(device_probe,		panthor_fbsd_probe),
	DEVMETHOD(device_attach,	panthor_fbsd_attach),
	DEVMETHOD(device_detach,	panthor_fbsd_detach),
	DEVMETHOD_END
};

static driver_t panthor_fbsd_driver = {
	"panthor",
	panthor_fbsd_methods,
	sizeof(struct panthor_fbsd_softc),
};

DRIVER_MODULE(panthor, acpi, panthor_fbsd_driver, 0, 0);
ACPI_PNP_INFO(panthor_fbsd_acpi_ids);
MODULE_DEPEND(panthor, acpi, 1, 1, 1);
MODULE_DEPEND(panthor, drmn, 2, 2, 2);
MODULE_DEPEND(panthor, dmabuf, 1, 1, 1);
MODULE_DEPEND(panthor, linuxkpi, 1, 1, 1);
MODULE_DEPEND(panthor, linuxkpi_video, 1, 1, 1);

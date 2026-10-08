# drm-panthor-kmod

The Linux panthor DRM driver (Arm Mali CSF GPUs) for FreeBSD, built against
[drm-kmod](https://github.com/freebsd/drm-kmod)'s DRM core and LinuxKPI.
Brought up on the CIX Sky1 (Orange Pi 6 Plus), whose Mali-G720 Immortalis
firmware describes in ACPI.

- `drivers/gpu/drm/panthor/`: the driver from Linux v7.0, and
  `include/uapi/drm/panthor_drm.h`.  GPL-2.0-only or MIT.
- `drivers/gpu/drm/drm_gem_shmem_helper.c`, `drm_gpuvm.c`, `scheduler/`,
  `include/drm/`: Linux v7.0's GEM shmem helper, GPU VA manager and GPU
  scheduler.  drm-kmod (Linux v6.13) lacks the first and has older versions of
  the others, so they are built into the module, their global symbols
  prefixed `panthor_` (`freebsd/include/panthor_freebsd_rename.h`).
  GPL-2.0 (shmem helper), GPL-2.0-only or MIT (drm_gpuvm), MIT (scheduler).
- `drivers/iommu/`, `include/linux/io-pgtable.h`: Linux v7.0's io-pgtable
  and its Arm LPAE format, for the GPU's own MMU.  GPL-2.0.
- `panthor/`: the module build.
- `freebsd/`: FreeBSD glue (BSD-2-Clause):
  - `panthor_freebsd_bus.c`: the ACPI driver (CIXH5000).  Brings the GPU
    up as CIX's firmware requires before any register access: the power
    domain (TF-A), the clocks (SCMI), the power resource, the reset and
    Q-channel clock gating, through FreeBSD's `sky1_scmi`.
  - `panthor_freebsd.c`: the platform device panthor attaches to; clocks by
    the names of the ACPI CLKT table; DMA attributes.
  - `panthor_freebsd_devfreq.c`: in place of `panthor_devfreq.c`, Linux's
    policy without its devfreq and OPP cores: utilization from panthor's
    busy and idle notes, sampled every 50 ms, and `simple_ondemand` with
    panthor's thresholds (45, 5), over the SCMI performance domain the GPU's
    `_DSD` names `"perf"` (`gpu_core`: 72 to 1000 MHz), through `sky1_scmi`.
    `dev.panthor.0.freq`, `load`, `freq_levels`, and `freq_fixed` to pin a
    level (0 follows the load).
  - `include/`: Linux headers LinuxKPI lacks or stubs, searched first, and
    `panthor_freebsd_compat.h`, included into every file, with what
    LinuxKPI and drm-kmod lack of Linux v7.0.

Changes to the Linux files are marked `#ifdef __linux__` / `__FreeBSD__`.

## Building

Against a FreeBSD source tree with the `sky1_scmi` driver (branch
`orangepi-6-plus` of [JamesKane/freebsd-src](https://github.com/JamesKane/freebsd-src))
and a drm-kmod checkout:

    make DRMKMOD=/path/to/drm-kmod SYSDIR=/path/to/src/sys

The GPU firmware, `arm/mali/arch12.8/mali_csffw.bin` from linux-firmware
(LICENCE.mali_csffw), goes in `/boot/firmware/arm/mali/arch12.8/`.

## Status

The CSF firmware boots (interface v3.13) and `/dev/dri/renderD128` appears.
Not yet: runtime suspend (the glue keeps the GPU resumed, as FreeBSD cannot
yet zap the user mappings of panthor's MMIO page), DVFS, mmap of imported
dma-bufs.

## Licensing

The Linux files keep their licences (SPDX tags; texts in `LICENSES/`); the
FreeBSD glue is BSD-2-Clause.

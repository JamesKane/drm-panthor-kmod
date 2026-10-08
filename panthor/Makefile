# The panthor DRM driver (Arm Mali CSF GPUs), built against drm-kmod.

DRMKMOD?=	${.CURDIR:H:H}/drm-kmod
.include "${DRMKMOD}/kconfig.mk"

PANTHORDIR=	${.CURDIR:H}/drivers/gpu/drm/panthor

.PATH:	${PANTHORDIR} ${.CURDIR:H}/drivers/gpu/drm ${.CURDIR:H}/drivers/iommu \
	${.CURDIR:H}/drivers/gpu/drm/scheduler \
	${.CURDIR:H}/freebsd

KMOD=	panthor

SRCS=	panthor_device.c \
	panthor_drv.c \
	panthor_fw.c \
	panthor_gem.c \
	panthor_gpu.c \
	panthor_heap.c \
	panthor_hw.c \
	panthor_mmu.c \
	panthor_pwr.c \
	panthor_sched.c

# In place of panthor_devfreq.c: simple_ondemand over SCMI (Sky1).
SRCS+=	panthor_freebsd_devfreq.c

SRCS+=	panthor_freebsd.c \
	panthor_freebsd_bus.c

# What drm-kmod and LinuxKPI lack, or have older (drm-kmod is Linux v6.13).
SRCS+=	drm_gem_shmem_helper.c \
	drm_gpuvm.c \
	sched_entity.c \
	sched_fence.c \
	sched_main.c \
	io-pgtable.c \
	io-pgtable-arm.c

SRCS+=	opt_acpi.h \
	opt_drm.h \
	opt_platform.h \
	opt_vm.h \
	acpi_if.h \
	bus_if.h \
	device_if.h \
	pci_if.h \
	vnode_if.h

.include <bsd.kmod.mk>

.include "${DRMKMOD}/linuxkpi_version.mk"
.include "${DRMKMOD}/compiler_flags.mk"

CFLAGS+= -I${.CURDIR:H}/freebsd/include
CFLAGS+= -I${.CURDIR:H}/include
CFLAGS+= -I${.CURDIR:H}/include/uapi
CFLAGS+= -I${PANTHORDIR}
CFLAGS+= -I${.CURDIR:H}/freebsd
CFLAGS+= -I${DRMKMOD}/linuxkpi/gplv2/include
CFLAGS+= -I${DRMKMOD}/linuxkpi/bsd/include
CFLAGS+= -I${SYSDIR}/compat/linuxkpi/common/include
CFLAGS+= -I${SYSDIR}/compat/linuxkpi/dummy/include
CFLAGS+= -I${DRMKMOD}/include
CFLAGS+= -I${DRMKMOD}/include/drm
CFLAGS+= -I${DRMKMOD}/include/uapi
CFLAGS+= -I${DRMKMOD}/include/uapi/drm
CFLAGS+= -I${DRMKMOD}/drivers/gpu

CFLAGS+= '-DKBUILD_MODNAME="${KMOD}"'
CFLAGS+= '-DLINUXKPI_PARAM_PREFIX=panthor_' -DDRM_SYSCTL_PARAM_PREFIX=_${KMOD}
CFLAGS+= ${KCONFIG:C/(.*)/-DCONFIG_\1/}
CFLAGS+= -include ${.CURDIR:H}/freebsd/include/panthor_freebsd_rename.h
CFLAGS+= -include ${.CURDIR:H}/freebsd/include/panthor_freebsd_compat.h
CFLAGS+= -DCONFIG_DRM_PANTHOR -DCONFIG_PM -DCONFIG_IOMMU_IO_PGTABLE_LPAE

CFLAGS.panthor_freebsd_bus.c+= -DPANTHOR_FBSD_BUS
CWARNFLAGS+=	-Wno-format -Wno-gnu-pointer-arith -Wno-cast-qual
CWARNFLAGS.io-pgtable-arm.c+=	-Wno-macro-redefined
CWARNFLAGS.panthor_drv.c+=	-Wno-unused-variable	# panthor_groups: no dev_groups
# LinuxKPI's refcount_read() and folio functions take no const pointers.
CWARNFLAGS.drm_gem_shmem_helper.c+= -Wno-incompatible-pointer-types-discards-qualifiers
CWARNFLAGS.panthor_mmu.c+=	-Wno-incompatible-pointer-types-discards-qualifiers

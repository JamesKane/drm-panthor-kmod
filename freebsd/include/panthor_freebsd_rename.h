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
 * panthor builds Linux v7.0's GPU scheduler and drm_gpuvm into the module,
 * as drm-kmod's (Linux v6.13) are older: their global symbols get a prefix,
 * so that they do not clash with drm.ko's.  Included first everywhere.
 */
#ifndef _PANTHOR_FREEBSD_RENAME_H_
#define	_PANTHOR_FREEBSD_RENAME_H_

#define	drm_sched_policy	panthor_drm_sched_policy
#define	drm_gpuva_find	panthor_drm_gpuva_find
#define	drm_gpuva_find_first	panthor_drm_gpuva_find_first
#define	drm_gpuva_find_next	panthor_drm_gpuva_find_next
#define	drm_gpuva_find_prev	panthor_drm_gpuva_find_prev
#define	drm_gpuva_insert	panthor_drm_gpuva_insert
#define	drm_gpuva_link	panthor_drm_gpuva_link
#define	drm_gpuva_map	panthor_drm_gpuva_map
#define	drm_gpuva_ops_free	panthor_drm_gpuva_ops_free
#define	drm_gpuva_remap	panthor_drm_gpuva_remap
#define	drm_gpuva_remove	panthor_drm_gpuva_remove
#define	drm_gpuva_unlink	panthor_drm_gpuva_unlink
#define	drm_gpuva_unlink_defer	panthor_drm_gpuva_unlink_defer
#define	drm_gpuva_unmap	panthor_drm_gpuva_unmap
#define	drm_gpuvm_bo_create	panthor_drm_gpuvm_bo_create
#define	drm_gpuvm_bo_deferred_cleanup	panthor_drm_gpuvm_bo_deferred_cleanup
#define	drm_gpuvm_bo_evict	panthor_drm_gpuvm_bo_evict
#define	drm_gpuvm_bo_extobj_add	panthor_drm_gpuvm_bo_extobj_add
#define	drm_gpuvm_bo_find	panthor_drm_gpuvm_bo_find
#define	drm_gpuvm_bo_obtain_locked	panthor_drm_gpuvm_bo_obtain_locked
#define	drm_gpuvm_bo_obtain_prealloc	panthor_drm_gpuvm_bo_obtain_prealloc
#define	drm_gpuvm_bo_put	panthor_drm_gpuvm_bo_put
#define	drm_gpuvm_bo_put_deferred	panthor_drm_gpuvm_bo_put_deferred
#define	drm_gpuvm_bo_unmap_ops_create	panthor_drm_gpuvm_bo_unmap_ops_create
#define	drm_gpuvm_exec_lock	panthor_drm_gpuvm_exec_lock
#define	drm_gpuvm_exec_lock_array	panthor_drm_gpuvm_exec_lock_array
#define	drm_gpuvm_exec_lock_range	panthor_drm_gpuvm_exec_lock_range
#define	drm_gpuvm_init	panthor_drm_gpuvm_init
#define	drm_gpuvm_interval_empty	panthor_drm_gpuvm_interval_empty
#define	drm_gpuvm_madvise_ops_create	panthor_drm_gpuvm_madvise_ops_create
#define	drm_gpuvm_prefetch_ops_create	panthor_drm_gpuvm_prefetch_ops_create
#define	drm_gpuvm_prepare_objects	panthor_drm_gpuvm_prepare_objects
#define	drm_gpuvm_prepare_range	panthor_drm_gpuvm_prepare_range
#define	drm_gpuvm_prepare_vm	panthor_drm_gpuvm_prepare_vm
#define	drm_gpuvm_put	panthor_drm_gpuvm_put
#define	drm_gpuvm_range_valid	panthor_drm_gpuvm_range_valid
#define	drm_gpuvm_resv_add_fence	panthor_drm_gpuvm_resv_add_fence
#define	drm_gpuvm_resv_object_alloc	panthor_drm_gpuvm_resv_object_alloc
#define	drm_gpuvm_sm_map	panthor_drm_gpuvm_sm_map
#define	drm_gpuvm_sm_map_exec_lock	panthor_drm_gpuvm_sm_map_exec_lock
#define	drm_gpuvm_sm_map_ops_create	panthor_drm_gpuvm_sm_map_ops_create
#define	drm_gpuvm_sm_unmap	panthor_drm_gpuvm_sm_unmap
#define	drm_gpuvm_sm_unmap_exec_lock	panthor_drm_gpuvm_sm_unmap_exec_lock
#define	drm_gpuvm_sm_unmap_ops_create	panthor_drm_gpuvm_sm_unmap_ops_create
#define	drm_gpuvm_validate	panthor_drm_gpuvm_validate
#define	drm_sched_entity_destroy	panthor_drm_sched_entity_destroy
#define	drm_sched_entity_error	panthor_drm_sched_entity_error
#define	drm_sched_entity_fini	panthor_drm_sched_entity_fini
#define	drm_sched_entity_flush	panthor_drm_sched_entity_flush
#define	drm_sched_entity_init	panthor_drm_sched_entity_init
#define	drm_sched_entity_modify_sched	panthor_drm_sched_entity_modify_sched
#define	drm_sched_entity_pop_job	panthor_drm_sched_entity_pop_job
#define	drm_sched_entity_push_job	panthor_drm_sched_entity_push_job
#define	drm_sched_entity_select_rq	panthor_drm_sched_entity_select_rq
#define	drm_sched_entity_set_priority	panthor_drm_sched_entity_set_priority
#define	drm_sched_fault	panthor_drm_sched_fault
#define	drm_sched_fence_alloc	panthor_drm_sched_fence_alloc
#define	drm_sched_fence_finished	panthor_drm_sched_fence_finished
#define	drm_sched_fence_free	panthor_drm_sched_fence_free
#define	drm_sched_fence_init	panthor_drm_sched_fence_init
#define	drm_sched_fence_scheduled	panthor_drm_sched_fence_scheduled
#define	drm_sched_fini	panthor_drm_sched_fini
#define	drm_sched_increase_karma	panthor_drm_sched_increase_karma
#define	drm_sched_init	panthor_drm_sched_init
#define	drm_sched_is_stopped	panthor_drm_sched_is_stopped
#define	drm_sched_job_add_dependency	panthor_drm_sched_job_add_dependency
#define	drm_sched_job_add_implicit_dependencies	panthor_drm_sched_job_add_implicit_dependencies
#define	drm_sched_job_add_resv_dependencies	panthor_drm_sched_job_add_resv_dependencies
#define	drm_sched_job_add_syncobj_dependency	panthor_drm_sched_job_add_syncobj_dependency
#define	drm_sched_job_arm	panthor_drm_sched_job_arm
#define	drm_sched_job_cleanup	panthor_drm_sched_job_cleanup
#define	drm_sched_job_has_dependency	panthor_drm_sched_job_has_dependency
#define	drm_sched_job_init	panthor_drm_sched_job_init
#define	drm_sched_job_is_signaled	panthor_drm_sched_job_is_signaled
#define	drm_sched_pick_best	panthor_drm_sched_pick_best
#define	drm_sched_resubmit_jobs	panthor_drm_sched_resubmit_jobs
#define	drm_sched_resume_timeout	panthor_drm_sched_resume_timeout
#define	drm_sched_rq_add_entity	panthor_drm_sched_rq_add_entity
#define	drm_sched_rq_remove_entity	panthor_drm_sched_rq_remove_entity
#define	drm_sched_rq_update_fifo_locked	panthor_drm_sched_rq_update_fifo_locked
#define	drm_sched_start	panthor_drm_sched_start
#define	drm_sched_stop	panthor_drm_sched_stop
#define	drm_sched_suspend_timeout	panthor_drm_sched_suspend_timeout
#define	drm_sched_tdr_queue_imm	panthor_drm_sched_tdr_queue_imm
#define	drm_sched_wakeup	panthor_drm_sched_wakeup
#define	drm_sched_wqueue_ready	panthor_drm_sched_wqueue_ready
#define	drm_sched_wqueue_start	panthor_drm_sched_wqueue_start
#define	drm_sched_wqueue_stop	panthor_drm_sched_wqueue_stop
#define	to_drm_sched_fence	panthor_to_drm_sched_fence

#endif /* !_PANTHOR_FREEBSD_RENAME_H_ */

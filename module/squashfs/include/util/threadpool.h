/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * threadpool.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "sqfs/predef.h"

typedef int (*thread_pool_worker_t)(void *user, void *work_item);

/**
 * @struct thread_pool_t
 *
 * @brief A thread pool with a ticket number based work item ordering.
 *
 * While the order in which items are non-deterministic, the thread pool
 * implementation internally uses a ticket system to ensure the completed
 * items are deqeueued in the same order that they were enqueued.
 */
typedef struct thread_pool_t {
	/**
	 * @brief Shutdown and destroy a thread pool.
	 *
	 * @param pool A pointer to a pool returned by thread_pool_create
	 */
	void (*destroy)(struct thread_pool_t *pool);

	/**
	 * @brief Get the actual number of worker threads available.
	 *
	 * @return A number greater or equal to 1.
	 */
	size_t (*get_worker_count)(struct thread_pool_t *pool);

	/**
	 * @brief Change the user data pointer for a thread pool worker
	 *        by index.
	 *
	 * @param idx A zero-based index into the worker list.
	 * @param ptr A user pointer that this specific worker thread should
	 *            pass to the worker callback.
	 */
	void (*set_worker_ptr)(struct thread_pool_t *pool, size_t idx,
			       void *ptr);

	/**
	 * @brief Submit a work item to a thread pool.
	 *
	 * This function will fail on allocation failure or if the internal
	 * error state is set was set by one of the workers.
	 *
	 * @param ptr A pointer to a work object to enqueue.
	 *
	 * @return Zero on success.
	 */
	int (*submit)(struct thread_pool_t *pool, void *ptr);

	/**
	 * @brief Wait for a work item to be completed.
	 *
	 * This function dequeues a single completed work item. It may block
	 * until one of the worker threads signals completion of an additional
	 * item.
	 *
	 * This function guarantees to return the items in the same order as
	 * they were submitted, so the function can actually block longer than
	 * necessary, because it has to wait until the next item in sequence
	 * is finished.
	 *
	 * @return A pointer to a new work item or NULL if there are none
	 *         in the pipeline.
	 */
	void *(*dequeue)(struct thread_pool_t *pool);

	/**
	 * @brief Get the internal worker return status value.
	 *
	 * If the worker functions returns a non-zero exit status in one of the
	 * worker threads, the thread pool stors the value internally and shuts
	 * down. This function can be used to retrieve the value.
	 *
	 * @return A non-zero value returned by the worker callback or zero if
	 *         everything is A-OK.
	 */
	int (*get_status)(struct thread_pool_t *pool);
} thread_pool_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a thread pool instance.
 *
 * @param num_jobs The number of worker threads to launch.
 * @param worker A function to call from the worker threads to process
 *               the work items.
 *
 * @return A pointer to a thread pool on success, NULL on failure.
 */
SQFS_INTERNAL thread_pool_t *thread_pool_create(size_t num_jobs,
						thread_pool_worker_t worker);

/**
 * @brief Create a serial mockup thread pool implementation.
 *
 * This returns a @ref thread_pool_t implementation that, instead of running a
 * thread pool actually does the work in-situ when dequeueing.
 *
 * @param worker A function to call from the worker threads to process
 *               the work items.
 *
 * @return A pointer to a thread pool on success, NULL on failure.
 */
SQFS_INTERNAL
thread_pool_t *thread_pool_create_serial(thread_pool_worker_t worker);

#ifdef __cplusplus
}
#endif

#endif /* THREADPOOL_H */

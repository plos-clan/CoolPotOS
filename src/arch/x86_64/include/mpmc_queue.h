#pragma once

#include "cptype.h"
/**
 * The header file for the MPMC ring buffer
 * The data structure and function declarations live here
 *
 * This buffer behaves as a queue (first-in, first-out)
 *
 * They are implemented in buf.c
 */

#define PTR_FREE(x)                                                                                \
    do {                                                                                           \
        if ((x) != NULL) {                                                                         \
            free(x);                                                                               \
            (x) = NULL;                                                                            \
        }                                                                                          \
    } while (0)

/**
 * The queue type uses void* right now
 * Modify it to use another type if you want
 * make sure either sizeof() works, or you set the size correctly with the init
 * function Or both
 */
typedef void *queue_entry_t;

/**
 * @enum overwrite_behavior
 * @brief Defines the behavior when attempting to overwrite an existing value.
 *
 * This enumeration specifies how the system should handle scenarios where
 * an operation might overwrite an existing value.
 *
 * @var FAIL
 *      Do not overwrite the value; the operation fails.
 * @var OVERWRITE
 *      Allow overwriting existing entries in the queue.
 * @var OVERWRITE_ENUM_MAX
 *      Sentinel value indicating the maximum enumeration value. Not used for
 * logic.
 */
typedef enum overwrite_behavior {
    FAIL               = 0, /**< Do not overwrite the value; the operation fails. */
    OVERWRITE          = 1, /**< Allow overwriting the existing values. */
    OVERWRITE_ENUM_MAX = 2  /**< Sentinel value for the maximum enum value. */
} overwrite_behavior_t;

/**
 * @enum status_code
 * @brief Represents various statuses that can be returned by an operation.
 *
 * This enumeration provides a comprehensive set of status codes
 * that indicate the outcome of a function or operation.
 *
 * @var SUCCESS
 *      The operation completed successfully.
 * @var FAILURE
 *      The operation encountered an error.
 * @var FULL
 *      The resource is full and cannot accommodate more data.
 * @var EMPTY
 *      The resource is empty and cannot provide more data.
 * @var BUSY
 *      The resource is busy and cannot process the request at this time.
 * @var UNAVAILABLE
 *      The resource is unavailable for use.
 * @var INVALID
 *      The operation was attempted with invalid input or configuration.
 * @var STATUS_ENUM_MAX
 *      Sentinel value indicating the maximum enumeration value. Not used for
 * logic.
 */
typedef enum status_code {
    SUCCESS         = 0, /**< The operation completed successfully. */
    FAILURE         = 1, /**< The operation encountered an error. */
    FULL            = 2, /**< The resource is full and cannot accommodate more data. */
    EMPTY           = 3, /**< The resource is empty and cannot provide more data. */
    BUSY            = 4, /**< The resource is busy and cannot process the request at this
                  time. */
    UNAVAILABLE     = 5, /**< The resource is unavailable for use. */
    INVALID         = 6, /**< The operation was attempted with invalid input or
                         configuration. */
    STATUS_ENUM_MAX = 7  /**< Sentinel value for the maximum enum value. */
} status_code_t;

/**
 * The buffer struct
 */
typedef struct mpmc_queue {
    queue_entry_t       *array; // < The actual storage array
    bool                *ready; // < A list of bools equal in length to the array. Used for
    // synchronization
    // These should be accessed atomically. If you use this in C++,
    // you can make the variables atomic. In C, make sure you access
    // them correctly. N.B. C++ atomics semantics bother me. A
    // /variable/ cannot be atomic. An /operation/ on that variable
    // is what's atomic. For example, a compare and swap or increment
    // (both forms of read-modify-write) may be done atomically. The
    // variable itself is just a variable. It's not atomic.
    uint32_t             capacity; // The capacity of the queue. Measured in units of
    // "elements". Must be u32_max or less.
    uint32_t             head;         // The pointer to the head (place where writes happen to)
    uint32_t             tail;         // The pointer to the tail (place where reads happen from)
    uint32_t             element_size; // The size of one entry in the queue, in bytes
    overwrite_behavior_t overwrite_behavior; // Used to control behavior of the queue when full. If
    // OVERWRITE, overwrite the oldest entries in the
    // queue. If FAIL, return failure.
} mpmc_queue_t;

/**
 * @brief Retrieves an entry from the queue in a thread-safe manner.
 *
 * This function attempts to dequeue an entry from the queue. It is
 * non-blocking and lock-free, making it suitable for high-performance,
 * multi-threaded applications.
 *
 * @param[in] queue Pointer to the queue structure.
 * @param[out] entry Pointer to the location where the dequeued entry will be
 * stored. User must allocate memory for this pointer
 * @return A status code indicating the result of the operation:
 *         - SUCCESS: The entry was successfully retrieved.
 *         - EMPTY: The queue is empty.
 *         - INVALID: The provided arguments are invalid.
 *         - BUSY: that entry of the queue is busy. Try again later
 */
status_code_t get(mpmc_queue_t *queue, queue_entry_t *entry);

/**
 * @brief Adds an entry to the queue in a thread-safe manner.
 *
 * This function attempts to enqueue an entry into the queue. It is
 * non-blocking and lock-free. If the queue is full, behavior depends
 * on the overwrite policy specified during initialization or set later.
 *
 * @param queue Pointer to the queue structure.
 * @param entry Pointer to the entry to be enqueued.
 * @return A status code indicating the result of the operation:
 *         - SUCCESS: The entry was successfully added.
 *         - FULL: The queue is full, and the entry was not added.
 *         - INVALID: The provided arguments are invalid.
 *         - BUSY: that entry of the queue is busy. Try again later
 */
status_code_t put(mpmc_queue_t *queue, queue_entry_t *entry);

/**
 * @brief Initializes the queue with the specified capacity and element size.
 *
 * This function sets up the queue as a lock-free, thread-safe ring buffer.
 * The behavior when the queue is full can be controlled using the
 * overwrite policy.
 *
 * @param[out] queue Pointer to the queue structure to be initialized. - If
 * NULL, a new buffer is malloc()ed.
 * @param[in] capacity Maximum number of elements the queue can hold.
 * @param[in] element_size Size of each element in bytes.
 * @param[in] overwrite_behavior Specifies the behavior when the queue is full:
 *        - FAIL: Disallow overwrites; insertion fails when full.
 *        - OVERWRITE: Allow overwriting the oldest entry.
 *        - Setting this invalid or to ENUM_MAX results in choosing the FAIL
 * operation
 * @return A status code indicating the result of the operation:
 *         - SUCCESS: The queue was successfully initialized.
 *         - INVALID: The provided arguments are invalid.
 */
status_code_t init(mpmc_queue_t *queue, uint32_t capacity, uint32_t element_size,
                   overwrite_behavior_t overwrite_behavior);

/**
 * @brief Destroys the queue and releases its resources.
 *
 * This function cleans up the queue, ensuring that all resources allocated
 * during initialization are properly freed. The queue should not be used
 * after being destroyed.
 *
 * @param[in|out] queue Pointer to the queue structure to be destroyed.
 * @return A status code indicating the result of the operation:
 *         - SUCCESS: The queue was successfully destroyed.
 *         - INVALID: The provided argument is invalid.
 */
status_code_t destroy(mpmc_queue_t *queue);

/**
 * @brief Sets the overwrite behavior for the queue.
 *
 * This function updates the overwrite policy for the queue, determining
 * how the queue behaves when it is full.
 *
 * @param[in|out] queue Pointer to the queue structure.
 * @param[in] overwrite_behavior The new overwrite behavior:
 *        - OVERWRITE: Allow overwriting the oldest entry when full.
 *        - FAIL: Disallow overwriting; insertion fails when full.
 * @return A status code indicating the result of the operation:
 *         - SUCCESS: The overwrite behavior was successfully updated.
 *         - INVALID: The provided arguments are invalid.
 */
status_code_t set_overwrite_behavior(mpmc_queue_t *queue, overwrite_behavior_t overwrite_behavior);

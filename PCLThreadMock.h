/**
 * PCLThreadMock.h - Thread-related mock functions for the PCL API
 * 
 * This file provides mock implementations of the thread-related functions
 * in the PCL API.
 */

#ifndef PCL_THREAD_MOCK_H
#define PCL_THREAD_MOCK_H

#include "PCLMockAPI.h"
#include <cstddef> // for size_t
#include <cstdint> // for uint32_t

namespace pcl_mock {

// Thread status flags (match PCL's definitions)
const uint32_t THREAD_RUNNING  = 0x80000000;
const uint32_t THREAD_FINISHED = 0x40000000;
const uint32_t THREAD_CANCELED = 0x20000000;

// Thread priority levels
enum thread_priority {
   ThreadPriorityIdle         = 0,
   ThreadPriorityLowest       = 1,
   ThreadPriorityLow          = 2,
   ThreadPriorityNormal       = 3,
   ThreadPriorityHigh         = 4,
   ThreadPriorityHighest      = 5,
   ThreadPriorityTimeCritical = 6,
   ThreadPriorityInherit      = 7,
   ThreadPriorityDefault      = ThreadPriorityNormal
};

// Thread-related function declarations

/**
 * Create a new thread
 * 
 * @param module_handle The module handle
 * @param thread_object The thread object (typically a pcl::Thread instance)
 * @param flags Thread creation flags
 * @return A handle to the created thread
 */
void* CreateThread(void* module_handle, void* thread_object, int flags);

/**
 * Set the execution routine for a thread
 * 
 * @param thread_handle The thread handle
 * @param dispatcher The thread dispatcher function
 */
  api_bool SetThreadExecRoutine(thread_handle thread_handle, pcl::thread_exec_routine dispatcher);

/**
 * Start a thread
 * 
 * @param thread_handle The thread handle
 * @param priority The thread priority
 * @return api_true if successful, api_false otherwise
 */
int StartThread(void* thread_handle, int priority);

/**
 * Check if a thread is active
 * 
 * @param thread_handle The thread handle
 * @return api_true if active, api_false otherwise
 */
api_bool IsThreadActive(const void* thread_handle);

/**
 * Get the current thread
 * 
 * @return The handle of the current thread, or nullptr if this is the root thread
 */
thread_handle GetCurrentThread();

/**
 * Get a thread's status
 * 
 * @param thread_handle The thread handle
 * @return The thread status
 */
uint32_t GetThreadStatus(void* thread_handle);

/**
 * Get a thread's status with extended options
 * 
 * @param thread_handle The thread handle
 * @param status Pointer to receive the status
 * @param flags Option flags
 * @return api_true if successful, api_false otherwise
 */
int GetThreadStatusEx(void* thread_handle, uint32_t* status, int flags);

/**
 * Set a thread's status
 * 
 * @param thread_handle The thread handle
 * @param status The new status
 */
void SetThreadStatus(void* thread_handle, uint32_t status);

/**
 * Get a thread's priority
 * 
 * @param thread_handle The thread handle
 * @return The thread priority
 */
int GetThreadPriority(void* thread_handle);

/**
 * Set a thread's priority
 * 
 * @param thread_handle The thread handle
 * @param priority The new priority
 */
void SetThreadPriority(void* thread_handle, int priority);

/**
 * Kill a thread
 * 
 * @param thread_handle The thread handle
 */
void KillThread(void* thread_handle);

/**
 * Wait for a thread to complete
 * 
 * @param thread_handle The thread handle
 * @param timeout_ms Timeout in milliseconds, or 0xFFFFFFFF to wait indefinitely
 * @return api_true if the thread completed, api_false if timed out
 */
int WaitThread(void* thread_handle, uint32_t timeout_ms);

/**
 * Pause the current thread
 * 
 * @param thread_handle The thread handle (can be null)
 * @param ms The number of milliseconds to sleep
 */
void SleepThread(void* thread_handle, uint32_t ms);

/**
 * Get a thread's stack size
 * 
 * @param thread_handle The thread handle
 * @return The stack size in bytes
 */
int GetThreadStackSize(void* thread_handle);

/**
 * Set a thread's stack size
 * 
 * @param thread_handle The thread handle
 * @param stack_size The new stack size in bytes
 */
void SetThreadStackSize(void* thread_handle, int stack_size);

/**
 * Add text to a thread's console output buffer
 * 
 * @param thread_handle The thread handle
 * @param text The text to append
 */
void AppendThreadConsoleOutputText(void* thread_handle, const char* text);

/**
 * Clear a thread's console output buffer
 * 
 * @param thread_handle The thread handle
 */
void ClearThreadConsoleOutputText(void* thread_handle);

/**
 * Get a thread's console output text
 * 
 * @param thread_handle The thread handle
 * @param buffer Buffer to receive the text, or nullptr to just get the size
 * @param size Pointer to size of buffer on input, receives actual size on output
 * @return api_true if successful, api_false otherwise
 */
int GetThreadConsoleOutputText(void* thread_handle, char* buffer, size_t* size);

/**
 * Get optimal number of threads for a performance analysis
 * 
 * @param algorithm Algorithm identifier
 * @param length Data length
 * @param item_size Size of each data item
 * @param floating_point Non-zero for floating point data
 * @param kernel_size Kernel size for convolution operations
 * @param width Width for 2D operations
 * @param height Height for 2D operations
 * @return Recommended number of threads
 */
int PerformanceAnalysisValue(int algorithm, size_t length, size_t item_size, 
                             int floating_point, int kernel_size, int width, int height);

/**
 * Register all thread-related functions with the mock API
 */
void RegisterThreadFunctions();

} // namespace pcl_mock

#endif // PCL_THREAD_MOCK_H

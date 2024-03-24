/**
 * @file rtos_helper_task.hpp
 * 
 * Helpful API to make use of FreeRTOS a little easier.
 * Supports both ESP-IDF and Arduino IDE.
 *
 * Author: Alexandr Antonov (@Bismuth208)
 * Licence: MIT
 *
 * Minimal FreeRTOS version: v10.4.3
 *
 */

#ifndef _RTOS_HELPER_TASK_HPP
#define _RTOS_HELPER_TASK_HPP

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/task.h"
#endif


#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#ifdef __cplusplus

// clang-format off

#include "rtos_helper_core.hpp"

/* -------------------------------------------------------------- */
/* -------------------- Definitions ------------------------ */
/* -------------------------------------------------------------- */

// - - - - - - - - - - - - - - - - - - - - - - - -

/* -------------------------------------------------------------- */
/* -------------------- Global functions ------------------------ */
/* -------------------------------------------------------------- */



/**
 * @brief Template class for Task creation and manipulation
 *
 * @code{cpp}
 * // Create task and provide 400 words for stack.
 * OSTask <2048u>AppMainTask(vAppMainTask, "AppMainTask");
 * ...
 * {
 *     ...
 *     // Call an actual OS Task creation
 *     AppMainTask.init();
 *     ...
 * }
 * @endcode
 * 
 * In case of multi-core MCU use this construction:
 * 
 * @code{cpp}
 * // Create task on CPU0 ( PRO_CPU for ESP32) and provide 4096 words for it.
 * OSTask <4096u>AppMainTask(vAppMainTask, "AppMainTask", nullptr, SomeTaskPriority, OS_MCU_CORE_0);
 * @endcode
 */
template <uint32_t TStackSize>
class OSTask
{
private:
    // Pointer to callback function with Main code containing endless loop
    void (*m_TaskFuncPtr)(void*) = nullptr; // TODO: move to std::function ?
    // Task name what will be used and visible during Debug
    const char* m_TaskName = nullptr;
    // Pointer to the argument passed on Thread/Task launch
    void* m_TaskArgument = nullptr;
    // How much time OS scheduler will provide to this Thread/Task
    UBaseType_t m_TaskPriority = tskIDLE_PRIORITY;
    // Number of Pinned MCU core (only used if MCU has multiple cores!)
    os_mcu_core_num_t m_ePinnedCore = OS_MCU_CORE_NONE;

    // An OS object handler.
    TaskHandle_t m_TaskHandle = nullptr;
    
    // Amount of physical RAM provided to the Thread/Task in words
    uint32_t m_TaskStackSize = TStackSize;
    
    // Required only for @ref SyncWait to use monotonic time with exact execution time 
    TickType_t m_xLastWakeTime = 0u;

    // Status flag showing if @ref init() was done with success
    bool m_initialized = false;

#if (configSUPPORT_STATIC_ALLOCATION == 1)
    // Lines below are OS specific data in RAM created and located at compile time.
    StaticTask_t m_xTaskControlBlock;
    StackType_t m_xTaskStack[TStackSize];
#endif // configSUPPORT_STATIC_ALLOCATION


public:
    OSTask(void (*TaskFuncPtr)(void*),
                        const char* TaskName, void* TaskArgument = nullptr, 
                        uint32_t TaskPriority = tskIDLE_PRIORITY,
                        os_mcu_core_num_t ePinnedCore = OS_MCU_CORE_NONE)
                                : m_TaskFuncPtr(TaskFuncPtr), 
                                m_TaskName(TaskName), m_TaskArgument(TaskArgument),
                                m_TaskPriority(TaskPriority),
                                m_ePinnedCore(ePinnedCore) {};

    ~OSTask()
    {
#if (INCLUDE_vTaskDelete == 1)
        assert(m_TaskHandle);
        vTaskDelete(m_TaskHandle);
        m_TaskHandle = nullptr;

#else
        // This will not work if assertion is disabled
        assert(nullptr);
#endif
    }


    /**
     * @brief Create software Thread/Task with OS functions
     * 
     * @return "true" if successful, "false" if not initialised
     * 
     * @note 1. This method is NOT thread-safe
     *       2. If called in ANY Thread/Task after OS scheduler is started,
     *          it will immediately force @ref Task's code to execute!
     *          This may happen under ESP32.
    */
    bool init()
    {
        assert(m_TaskName != nullptr);
        assert(m_TaskFuncPtr != nullptr);
        assert(m_TaskStackSize != 0u);

#ifdef OS_MCU_ENABLE_MULTICORE_SUPPORT
#if configSUPPORT_STATIC_ALLOCATION
        if (m_ePinnedCore < OS_MCU_CORE_NONE) {
          m_TaskHandle = xTaskCreateStaticPinnedToCore(
              static_cast<TaskFunction_t>(m_TaskFuncPtr), m_TaskName,
              m_TaskStackSize, m_TaskArgument, m_TaskPriority, &m_xTaskStack[0],
              &m_xTaskControlBlock, (BaseType_t)m_ePinnedCore);
        } else {
          m_TaskHandle = xTaskCreateStatic(static_cast<TaskFunction_t>(m_TaskFuncPtr),
                                      m_TaskName, m_TaskStackSize, m_TaskArgument,
                                      m_TaskPriority, &m_xTaskStack[0], &m_xTaskControlBlock);
        }
#else
        if (m_ePinnedCore < OS_MCU_CORE_NONE) {
          xTaskCreatePinnedToCore(static_cast<TaskFunction_t>(m_TaskFuncPtr),
                                  m_TaskName, m_TaskStackSize, m_TaskArgument,
                                  m_TaskPriority, &m_xTask, (BaseType_t)m_ePinnedCore);
        } else {
          xTaskCreate(static_cast<TaskFunction_t>(m_TaskFuncPtr), m_TaskName,
                      m_TaskStackSize, m_TaskArgument, m_TaskPriority, &m_xTask);
        }
#endif // configSUPPORT_STATIC_ALLOCATION
#else
#if configSUPPORT_STATIC_ALLOCATION
        m_TaskHandle = xTaskCreateStatic(static_cast<TaskFunction_t>(m_TaskFuncPtr),
                                m_TaskName, m_TaskStackSize, m_TaskArgument,
                                m_TaskPriority, &m_xTaskStack[0], &m_xTaskControlBlock);
#else
        xTaskCreate(static_cast<TaskFunction_t>(m_TaskFuncPtr), m_TaskName,
                    m_TaskStackSize, m_TaskArgument, m_TaskPriority, &m_TaskHandle);
#endif
#endif // OS_MCU_ENABLE_MULTICORE_SUPPORT
        assert(m_TaskHandle);
        if (m_TaskHandle != nullptr) {
            m_initialized = true;
        }

        return m_initialized;
    }

    /**
     * @brief Get an OS Thread/Task handler for direct manipulation
     * 
     * @retval Pointer to the OS type of the RAW handler.
     * 
     * @note 1. It's possible ONLY when @ref init() was DONE!
     * @note 2. Be careful, all what you will do, you'll do on your own risk !
    */
    TaskHandle_t getHandler()
    {
        return m_TaskHandle;
    }

    /**
     * @brief Sets the name of the Task (required only for debug)
     * 
     * @param newName New name of the Thread/Task
     * 
     * @return "true" if successful, "false" IF IT WAS initialised
     * 
     * @note It's possible ONLY when @ref init() was NOT DONE!
    */
    bool setName(const char* newName)
    {
        assert(m_initialized == false);
        assert(newName != nullptr);
        if (m_initialized) {
            return false;
        }

        m_TaskName = newName;
        return true;
    }

    /**
     * @brief Get the name of the Task (required only for debug)
     * 
     * @retval Name of the Thread/Task
     * 
     * @note If used before calling @ref init(), may return nullptr!
    */
    const char* getName(void)
    {
        return m_TaskName;
    }

    /**
     * @brief Sets main Task code callback function
     * 
     * @param newTaskFuncPtr Pointer to the callback function with Main code containing endless loop
     * 
     * @return "true" if successful, "false" IF IT WAS initialised
     * 
     * @note It's possible ONLY when @ref init() was NOT DONE!
    */
    bool setFunction(void (*newTaskFuncPtr)(void*))
    {
        assert(m_initialized == false);
        assert(newTaskFuncPtr != nullptr);
        if (m_initialized) {
            return false;
        }

        m_TaskFuncPtr = newTaskFuncPtr;
        return true;
    }

    /**
     * @brief Get main Task code callback function
     * 
     * @retval Pointer to the callback function with Main code containing endless loop
     * 
     * @note If used before calling @ref init(), may return nullptr!
    */
    void (*getFunction(void))(void*)
    {
        return m_TaskFuncPtr;
    }

    /**
     * @brief Sets Task argument parameter provided at it's launch
     * 
     * @param newTaskArgument Pointer to the argument passed on Thread/Task launch
     * 
     * @return "true" if successful, "false" IF IT WAS initialised
     * 
     * @note It's possible ONLY when @ref init() was NOT DONE!
    */
    bool setArg(void* newTaskArgument)
    {
        assert(m_initialized == false);
        assert(newTaskArgument != nullptr);
        if (m_initialized) {
            return false;
        }

        m_TaskArgument = newTaskArgument;
        return true;
    }

    /**
     * @brief Get Task argument parameter provided at it's launch
     * 
     * @retval Pointer to the argument passed on Thread/Task launch
     * 
     * @note If used before calling @ref init(), may return nullptr!
    */
    void* getArg(void* newTaskArgument)
    {
        return m_TaskArgument;
    }


#if (INCLUDE_vTaskSuspend == 1)
    /**
     * @brief If for some reason Task must be stopped
     * 
     * @return "true" if successful, "false" if not initialised
     * 
     * @note requires INCLUDE_vTaskSuspend to be 1
     */
    bool stop(void)
    { 
        assert(m_TaskHandle);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

        vTaskSuspend(m_TaskHandle);
        return true;
    }
#endif // INCLUDE_vTaskSuspend


#if (INCLUDE_vTaskResume == 1)
    /**
     * @brief If for some reason Task must be started
     * 
     * @return "true" if successful, "false" if not initialised
     * 
     * @note requires INCLUDE_vTaskResume to be 1
     */
    bool start(void)
    {
        assert(m_TaskHandle);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

#if (__cplusplus >= 201703L)
        return (bool)execIsrFunc([&]() -> BaseType_t {
            vTaskResume(m_TaskHandle);
            return true;
        }, [&](auto status, auto yieldFunc) -> BaseType_t {
            *status = xTaskResumeFromISR(m_TaskHandle);
            yieldFunc(status);
            return true;
        });
#else
    if (xPortInIsrContext() == pdFALSE) {
      vTaskResume(m_TaskHandle);
    } else {
      BaseType_t xHigherPriorityStatus = xTaskResumeFromISR(m_TaskHandle);
      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return true;
#endif

    }
#endif // INCLUDE_vTaskResume

#if (configUSE_TASK_NOTIFICATIONS == 1)
    /**
     * @brief This method allow to unblock Task.
     *        Lock/unlock mechanism similar to binary semaphore,
     *        but according to docs for FreeRTOS it's way more lightweight.
     * 
     * @return "true" if successful, "false" if not initialised
     *
     * @note 1. This method can be used in any task, even by task itself!
     * @note 2. Requires configUSE_TASK_NOTIFICATIONS to be 1
     */
    bool emitSignal(void)
    {
        assert(m_TaskHandle);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

#if (__cplusplus >= 201703L)
        return (bool)execIsrFunc([&]() -> BaseType_t {
            return xTaskNotifyGive(m_TaskHandle);
        }, [&](auto status, auto yieldFunc) -> BaseType_t {
            vTaskNotifyGiveFromISR(m_TaskHandle, status);
            yieldFunc(status);
            return true;
        });
#else
    if (xPortInIsrContext() == pdFALSE) {
      return xTaskNotifyGive(m_TaskHandle);
    } else {
      BaseType_t xHigherPriorityStatus = pdFALSE;
      vTaskNotifyGiveFromISR(m_TaskHandle, &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }

      return true;
    }
#endif
    }
#endif // configUSE_TASK_NOTIFICATIONS


#if (configUSE_TASK_NOTIFICATIONS == 1)
    /**
     * @brief This method allow to block Task.
     *        Lock/unlock mechanism similar to binary semaphore,
     *        but according to docs for FreeRTOS it's way more lightweight.
     * 
     * @param xMsToWait Amount of time code will be blocked/paused
     *
     * @note 1. This method must be used inside Task what need to be blocked!
     * @note 2. Requires configUSE_TASK_NOTIFICATIONS to be 1
     */
    void waitSignal(size_t xMsToWait = portMAX_DELAY_MS)
    {
        // assert(INCLUDE_xTaskGetCurrentTaskHandle == 1);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);

#if (INCLUDE_xTaskGetCurrentTaskHandle == 1)
        // Protection against Another Thread/Task will call this method
        assert(m_TaskHandle == xTaskGetCurrentTaskHandle());
#else
#warning "INCLUDE_xTaskGetCurrentTaskHandle is not enabled! Using .waitSignal() is not thread safe!"
#endif // INCLUDE_xTaskGetCurrentTaskHandle

        do {
            portNOP();
        } while (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(xMsToWait)) == pdFALSE);
    }
#endif // configUSE_TASK_NOTIFICATIONS


#if (INCLUDE_vTaskDelay == 1)
    /**
     * @brief Yet another way to wait and pause(block) Task
     * 
     * @param xDurationMs Amount of time code will be blocked/paused
     *
     * @code{cpp}
     *  // Block code execution for 500 ms.
     *  OSTask<0>::delay(500);
     * @endcode
     * 
     * @note requires INCLUDE_vTaskDelay to be 1
     */
    static void delay(size_t xDurationMs = 1ul)
    {
        vTaskDelay(pdMS_TO_TICKS(xDurationMs));
    }
#endif // INCLUDE_vTaskDelay

    /**
     * @brief Switch to another task
     * 
     * @code{cpp}
     *  OSTask<0>::yield();
     * @endcode
     */
    static void yield(void)
    {
        taskYIELD();
    }

#if (INCLUDE_vTaskDelete == 1)
    /**
     * @brief Self delete task (not an object see notes)
     * 
     * @code{cpp}
     *  OSTask<0u>::selfDelete();
     * @endcode
     * 
     * @note 1. Requires INCLUDE_vTaskDelete to be 1
     * @note 2. This method should be used ONLY as last resort safety
     * @note 3. This method WILL NOT delete created Task object !
     */
    static void selfDelete()
    {
        vTaskDelete(NULL);
    }
#endif // INCLUDE_vTaskDelete


    /**
     * @brief Initialize starting point for @ref SyncWait
     * 
     * @note Must be called inside of Task's code !
    */
    void syncWaitInit()
    {
        // assert(INCLUDE_xTaskGetCurrentTaskHandle == 1);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);

#if (INCLUDE_xTaskGetCurrentTaskHandle == 1)
        // Protection against Another Thread/Task will call this method
        assert(m_TaskHandle == xTaskGetCurrentTaskHandle());
#else
#warning "INCLUDE_xTaskGetCurrentTaskHandle is not enabled! Using .syncWaitInit() is not thread safe!"
#endif // INCLUDE_xTaskGetCurrentTaskHandle

        m_xLastWakeTime = xTaskGetTickCount();
    }

    /**
     * @brief Perform Task block for exact amount of time
     * 
     * @param xMsToWait How much time to wait in milliseconds for next Sync
     * 
     * @note Must be called inside of Task's loop code !
     */
    void syncWait(size_t xMsToWait = portMAX_DELAY_MS)
    {
        // assert(INCLUDE_xTaskGetCurrentTaskHandle == 1);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        
#if (INCLUDE_xTaskGetCurrentTaskHandle == 1)
        // Protection against Another Thread/Task will call this method
        assert(m_TaskHandle == xTaskGetCurrentTaskHandle());
#else
#warning "INCLUDE_xTaskGetCurrentTaskHandle is not enabled! Using .syncWait() is not thread safe!"
#endif // INCLUDE_xTaskGetCurrentTaskHandle

        xTaskDelayUntil(&m_xLastWakeTime, pdMS_TO_TICKS(xMsToWait));
    }

    /**
     * @brief Get RAW value of OS system time
     * 
     * @retval Time since last Sync event
     */
    static TickType_t syncWaitGetRAWTime()
    {
        return xTaskGetTickCount();
    }

    /**
     * @brief Suspend all Threads/Tasks
     * 
     * @note Normally you should not use this, 
     *       since it's would not work under Linux.
     *       Only under FreeRTOS, Unix, Window an some other OS.
    */
    static void stopAll(void)
    {
        vTaskSuspendAll();
    }

    /**
     * @brief Resume all Threads/Tasks
     * 
     * @note Normally you should not use this, 
     *       since it's would not work under Linux.
     *       Only under FreeRTOS, Unix, Window an some other OS.
    */
    static void StartAll(void)
    {
        xTaskResumeAll();
    }
};

// - - - - - - - - - - - - - - - - - - - - - - - -


// clang-format on

#endif // __cplusplus

#endif // _RTOS_HELPER_TASK_HPP
/**
 * @file rtos_helper_mutex.hpp
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

#ifndef _RTOS_HELPER_MUTEX_HPP
#define _RTOS_HELPER_MUTEX_HPP

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/semphr.h"
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


#if (configUSE_MUTEXES == 1)
/**
 * @brief Implementation of Mutex Class over Semaphore
 *
 * @code{cpp}
 * OSMutex spiMutex;  // Creation of Mutex object
 * ...
 * {
 *     ...
 *     // Call an actual OS Mutex creation
 *     spiMutex.init();
 *     ...
 * }
 * ...
 * spiMutex.lock(); // blocks resource for other tasks
 * ... some actions ...
 * spiMutex.unlock(); // unblocks resource for other tasks
 * @endcode
 *
 * @note Do not use Mutexes inside ISR context!
 */
class OSMutex
{
private:
    // An OS object handler.
    SemaphoreHandle_t m_MutexHandler = nullptr;

#if (configSUPPORT_STATIC_ALLOCATION == 1)
    // Lines below are OS specific data in RAM created and located at compile time.
    StaticSemaphore_t m_MutexControlBlock;
#endif

    // Status flag showing if @ref init() was done with success
    bool m_initialized = false;

public:
    OSMutex(){};

    /**
     * @brief Create software Mutex with OS functions
     * 
     * @return "true" if successful, "false" if not initialised
     * 
     * @note This method is NOT thread-safe and NOT ISR safe
    */
    bool init()
    {
#if (configSUPPORT_STATIC_ALLOCATION == 1)
        m_MutexHandler = xSemaphoreCreateMutexStatic(&m_MutexControlBlock);
#else
        m_MutexHandler = xSemaphoreCreateMutex();
#endif

        assert(m_MutexHandler);
        if (m_MutexHandler != nullptr) {
            m_initialized = true;
        }

        return (m_MutexHandler == nullptr);
    }

    /**
     * @brief Get an OS Mutex handler for direct manipulation
     * 
     * @retval Pointer to the OS type of the RAW handler.
     * 
     * @note 1. It's possible ONLY when @ref init() was DONE!
     * @note 2. Be careful, all what you will do, you'll do on your own risk !
    */
    SemaphoreHandle_t getHandler()
    {
        return m_MutexHandler;
    }

    /**
     * @brief Request for blocking resource for single use
     * 
     * @param xMsToWait How much time to wait in milliseconds for an obtaining resource
     * 
     * @retval "true" if successful, "false" if not initialised and/or timeout reached
     * 
     * @note 1. DO NOT use it in ISR
     * @note 2. This method is thread-safe
     * @note 3. This method is NOT an ISR safe
     * @note 4. This Mutex does not provide recursive ownership !
    */
    bool lock(size_t xMsToWait = portMAX_DELAY_MS)
    {
        assert(m_MutexHandler);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

        bool res = (bool)xSemaphoreTake(m_MutexHandler, pdMS_TO_TICKS(xMsToWait));

        return res;
    }

    /**
     * @brief Free previously blocked resource with @ref Lock()
     * 
     * @retval "true" if successful, "false" if not initialised
     * 
     * @note 1. DO NOT use it in ISR
     * @note 2. This method is thread-safe
     * @note 3. This method is NOT an ISR safe
     * @note 4. This Mutex does not provide recursive ownership !
    */
    bool unlock()
    {
        assert(m_MutexHandler);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

        bool res = (bool)xSemaphoreGive(m_MutexHandler);

        return res;
    }
};
#endif // configUSE_MUTEXES

// - - - - - - - - - - - - - - - - - - - - - - - -


// clang-format on

#endif // __cplusplus

#endif // _RTOS_HELPER_MUTEX_HPP
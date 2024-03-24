/**
 * @file rtos_helper_counter.hpp
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

#ifndef _RTOS_HELPER_COUNTER_HPP
#define _RTOS_HELPER_COUNTER_HPP

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


#if configUSE_COUNTING_SEMAPHORES
/**
 * @brief Implementation of Counter Class over Counting Semaphore
 *
 * @code{cpp}
 * // Creation:
 * Counter btnPressCounter;
 * ...
 * {
 *     ...
 *     // Call an actual OS Timer creation
 *     btnPressCounter.init();
 *     ...
 * }
 * ...
 * // In one Task, ISR or any callback:
 * ...
 * if (btn_read(SOME_BTN_NUM) == 1) {
 *   btnPressCounter.give();
 * }
 * ...
 *
 * // Meanwhile in other Task:
 * ...
 * while (btnPressCounter.take(0u) == pdTRUE) {
 *   blink_ok_led();
 * }
 * ...
 * @endcode
 */
template <size_t MaxCount> class Counter
{
private:
    size_t m_MaxCount = MaxCount;

    // An OS object handler.
    SemaphoreHandle_t m_xCounter = nullptr;

    // Status flag showing if @ref init() was done with success
    bool m_initialized = false;

#if (configSUPPORT_STATIC_ALLOCATION == 1)
    // Lines below are OS specific data in RAM created and located at compile time.
    StaticSemaphore_t m_xSemaphoreControlBlock;
#endif

public:
    Counter(){};

    ~Counter()
    {
        assert(m_xCounter);
        vSemaphoreDelete(m_xCounter);
        m_xCounter = nullptr;
    };


    /**
     * @brief Create counting semaphore with OS functions
     * 
     * @return "true" if successful, "false" if not initialised
     * 
     * @note 1. This method is NOT thread-safe
     * @note 2. This method is NOT an ISR safe
    */
    bool init()
    {
        assert(m_MaxCount != 0u);

#if (configSUPPORT_STATIC_ALLOCATION == 1)
        m_xCounter = xSemaphoreCreateCountingStatic(m_MaxCount, 0u,
                                                   &m_xSemaphoreControlBlock);
#else
        m_xCounter = xSemaphoreCreateCounting(m_MaxCount, 0u);
#endif

        assert(m_xCounter);
        if (m_xCounter != nullptr) {
            m_initialized = true;
        }

        return m_initialized;
    }

    /**
     * @brief Get an OS Semaphore handler for direct manipulation
     * 
     * @retval Pointer to the OS type of the RAW handler.
     * 
     * @note 1. It's possible ONLY when @ref init() was DONE!
     * @note 2. Be careful, all what you will do, you'll do on your own risk !
    */
    SemaphoreHandle_t getHandler()
    {
        return m_xCounter;
    }

    /**
     * @brief Decrement by one
     * 
     * @param xMsToWait How much time to wait in milliseconds for a single item/count
     * 
     * @return "true" if successful, "false" if not initialised or no items pending
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
    */
    bool take(size_t xMsToWait = portMAX_DELAY_MS)
    {
        assert(m_xCounter);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

#if (__cplusplus >= 201703L)
        return (bool)execIsrFunc([&]() -> BaseType_t {
            return xSemaphoreTake(m_xCounter, pdMS_TO_TICKS(xMsToWait));
        }, [&](auto status, auto yieldFunc) -> BaseType_t {
            auto res = xSemaphoreTakeFromISR(m_xCounter, status);
            yieldFunc(status);
            return res;
        });
#else
    BaseType_t res = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      res = xSemaphoreTake(m_xCounter, xMsToWait);
    } else {
      BaseType_t xHigherPriorityStatus = pdFALSE;
      res = xSemaphoreTakeFromISR(m_xCounter, &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return (bool)res;
#endif
    }

    /**
     * @brief Increment by one
     * 
     * @return "true" if successful, "false" if not initialised or no free slots(counts)
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
    */
    bool give()
    {
        assert(m_xCounter);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

#if (__cplusplus >= 201703L)
        return (bool)execIsrFunc([&]() -> BaseType_t {
            return xSemaphoreGive(m_xCounter);
        }, [&](auto status, auto yieldFunc) -> BaseType_t {
            auto res = xSemaphoreGiveFromISR(m_xCounter, status);
            yieldFunc(status);
            return res;
        });
#else
    BaseType_t res = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      res = xSemaphoreGive(m_xCounter);
    } else {
      BaseType_t xHigherPriorityStatus = pdFALSE;
      res = xSemaphoreGiveFromISR(m_xCounter, &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return (bool)res;
#endif
    }

    /**
     * @brief Clear all pending items
     * 
     * @return "true" if successful, "false" if not initialised or no items pending
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe (?)
    */
    bool reset()
    {
        assert(m_xCounter);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

#if (__cplusplus >= 201703L)
        return (bool)execIsrFunc([&]() -> BaseType_t {
            while (xSemaphoreTake(m_xCounter, 0u))
                ;
            return pdTRUE;
        }, [&](auto status, auto yieldFunc) -> BaseType_t {
            // This part is entirely wrong. Have to rewrite it someday...
            while (xSemaphoreTakeFromISR(m_xCounter, status))
                ;
            yieldFunc(status);
            return pdTRUE;
        });
#else
    if (xPortInIsrContext() == pdFALSE) {
      while (xSemaphoreTake(m_xCounter, 1UL))
        ;
        return true;
    } else {
      BaseType_t xHigherPriorityStatus = pdFALSE;
      // This part is entirely wrong. Have to rewrite it someday...
      while (xSemaphoreTakeFromISR(m_xCounter, &xHigherPriorityStatus))
        ;

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }

      return true;
    }
#endif

        // This might work, but must be tested under ISR.
        // return (bool)xQueueReset(m_xCounter);
    }
};
#endif // configUSE_COUNTING_SEMAPHORES

// - - - - - - - - - - - - - - - - - - - - - - - -


// clang-format on

#endif // __cplusplus

#endif // _RTOS_HELPER_COUNTER_HPP
/**
 * @file rtos_helper_timer.hpp
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

#ifndef _RTOS_HELPER_TIMER_HPP
#define _RTOS_HELPER_TIMER_HPP

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/timers.h"
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

#if (configUSE_TIMERS == 1)
/**
 * @brief Class for software Timer
 *
 * @code{cpp}
 * // creation of "one shot" timer object
 * OSTimer wifiOffTimer(wifiOffTimerFunc, "wifiOffTimer");
 * ...
 * {
 *     ...
 *     // Call an actual OS Timer creation
 *     wifiOffTimer.init();
 *     ...
 * }
 * ...
 * if (xSomeCondition == true) {
 *   if (wifiOffTimer.IsActive() == false) {
 *     // Shut down WiFi after 500ms.
 *     wifiOffTimer.start(500u);
 *   }
 * }
 * ...
 * @endcode
 * 
 * @note Callback function for the Timer SHOULD NOT block/pause/delay/suspend code execution
 *       (it will break Timers and OS scheduler) !
 */
class OSTimer
{
private:
    // Pointer to callback function to call when Timer will fire
    void (*m_TimerFuncPtr)(TimerHandle_t) = nullptr;
    // Timer name used for debug purpose
    const char* m_TimerName = nullptr;
    // Status flag telling to automatically restart Timer with @ref xPeriodInMs
    UBaseType_t m_uxAutoReload = false;
    // If same callback is used for mutiple timers, this argument will be passed to the callback
    void *m_pvTimerID = nullptr;

    // An OS object handler.
    TimerHandle_t m_xTimerHandler = nullptr;

    // Status flag showing if @ref init() was done with success
    bool m_initialized = false;

#if (configSUPPORT_STATIC_ALLOCATION == 1)
    // Lines below are OS specific data in RAM created and located at compile time.
    StaticTimer_t m_xTimerHandlerControlBlock;
#endif

public:
    OSTimer(void (*callbackFunction)(TimerHandle_t),
            const char* timerName,
            bool autoReload = false, void* timerID = nullptr)
            : m_TimerFuncPtr(callbackFunction),
            m_TimerName(timerName),
            m_uxAutoReload(autoReload),
            m_pvTimerID(timerID) {};

    ~OSTimer() {
        assert(m_xTimerHandler);
        // delete immediately
        xTimerDelete(m_xTimerHandler, 0u);
        m_xTimerHandler = nullptr;
    };

    /**
     * @brief Create software Timer with OS functions
     * 
     * @return "true" if successful, "false" if not initialised
     * 
     * @note 1. This method is NOT thread-safe
     * @note 2. This method is NOT an ISR safe
    */
    bool init()
    {
        assert(m_TimerName != nullptr);
        assert(m_TimerFuncPtr != nullptr);

#if (configSUPPORT_STATIC_ALLOCATION == 1)
            m_xTimerHandler = xTimerCreateStatic(
                        m_TimerName, 1, m_uxAutoReload, m_pvTimerID,
                        static_cast<TimerCallbackFunction_t>(m_TimerFuncPtr),
                        &m_xTimerHandlerControlBlock);
#else

            m_xTimerHandler = xTimerCreate(
                        m_TimerName, 1, m_uxAutoReload, m_pvTimerID,
                        static_cast<TimerCallbackFunction_t>(m_TimerFuncPtr));
#endif
        assert(m_xTimerHandler);
        if (m_xTimerHandler != nullptr) {
            m_initialized = true;
        }

        return m_initialized;
    }

    /**
     * @brief Get an OS Timer handler for direct manipulation
     * 
     * @retval Pointer to the OS type of the RAW handler.
     * 
     * @note 1. It's possible ONLY when @ref init() was DONE!
     * @note 2. Be careful, all what you will do, you'll do on your own risk !
    */
    TimerHandle_t getHandler()
    {
        return m_xTimerHandler;
    }

    /**
     * @brief Sets the name of the Timer (required only for debug)
     * 
     * @param newName New name of the Timer
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

        m_TimerName = newName;
        return true;
    }

    /**
     * @brief Get the name of the Timer (required only for debug)
     * 
     * @retval Name of the Timer
     * 
     * @note If used before calling @ref init(), may return nullptr!
    */
    const char* getName(void)
    {
        return m_TimerName;
    }

    /**
     * @brief Star timer with provided time
     * 
     * @param xPeriodInMs Amount of time has to be passed before timer will shot.
     * 
     * @return "true" if successful, "false" if not initialised
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
    */
    bool start(size_t xPeriodInMs = 0u)
    {
        assert(m_xTimerHandler);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

#if (__cplusplus >= 201703L)
        return (bool)execIsrFunc([&]() -> BaseType_t {
            xTimerChangePeriod(m_xTimerHandler, pdMS_TO_TICKS(xPeriodInMs), 0ul);
            return xTimerStart(m_xTimerHandler, 0ul);
        }, [&](auto status, auto yieldFunc) -> BaseType_t {
            xTimerChangePeriodFromISR(m_xTimerHandler, pdMS_TO_TICKS(xPeriodInMs), status);
            auto res = xTimerStartFromISR(m_xTimerHandler, status);
            yieldFunc(status);
            return res;
        });
#else
        BaseType_t res = pdFALSE;
        BaseType_t xHigherPriorityStatus = pdFALSE;

        if (xPortInIsrContext() == pdFALSE) {
          xTimerChangePeriod(m_xTimerHandler, pdMS_TO_TICKS(xPeriodInMs), 0UL);
          res = xTimerStart(m_xTimerHandler, 0UL);
        } else {
          xTimerChangePeriodFromISR(m_xTimerHandler, pdMS_TO_TICKS(xPeriodInMs),
                                    &xHigherPriorityStatus);
          res = xTimerStartFromISR(m_xTimerHandler, &xHigherPriorityStatus);

          if (pdTRUE == xHigherPriorityStatus) {
            portYIELD_FROM_ISR();
          }
        }

        return (bool)res;
#endif
    }

    /**
     * @brief Stop the software Timer
     * 
     * @return "true" if successful, "false" if not initialised
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
    */
    bool stop()
    {
        assert(m_xTimerHandler);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

#if (__cplusplus >= 201703L)
        return (bool)execIsrFunc([&]() -> BaseType_t {
            return xTimerStop(m_xTimerHandler, 0ul);
        }, [&](auto status, auto yieldFunc) -> BaseType_t {
            auto res = xTimerStopFromISR(m_xTimerHandler, status);
            yieldFunc(status);
            return res;
        });
#else
        BaseType_t res = pdFALSE;
        BaseType_t xHigherPriorityStatus = pdFALSE;

        if (xPortInIsrContext() == pdFALSE) {
          res = xTimerStop(m_xTimerHandler, 0UL);
        } else {
          res = xTimerStopFromISR(m_xTimerHandler, &xHigherPriorityStatus);

          if (pdTRUE == xHigherPriorityStatus) {
            portYIELD_FROM_ISR();
          }
        }

        return (bool)res;
#endif
    }

    /**
     * @brief Restart timer with provided time (if already running)
     * 
     * @param xPeriodInMs Amount of time has to be passed before timer will shot.
     * 
     * @return "true" if successful, "false" if not initialised
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
    */
    bool restart(size_t xPeriodInMs = 0u)
    {
        assert(m_xTimerHandler);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

#if (__cplusplus >= 201703L)
        return (bool)execIsrFunc([&]() -> BaseType_t {
            return xTimerReset(m_xTimerHandler, pdMS_TO_TICKS(xPeriodInMs));
        }, [&](auto status, auto yieldFunc) -> BaseType_t {
            auto res = xTimerResetFromISR(m_xTimerHandler, status);
            yieldFunc(status);
            return res;
        });
#else
        BaseType_t res = pdFALSE;
        BaseType_t xHigherPriorityStatus = pdFALSE;

        if (xPortInIsrContext() == pdFALSE) {
          res = xTimerReset(m_xTimerHandler, pdMS_TO_TICKS(xPeriodInMs));
        } else {
          res = xTimerResetFromISR(m_xTimerHandler, &xHigherPriorityStatus);

          if (pdTRUE == xHigherPriorityStatus) {
            portYIELD_FROM_ISR();
          }
        }

        return res;
#endif
    }

    /**
     * @brief Checks is Timer has been started and running
     * 
     * @return "true" if successful, "false" if not initialised and/or stopped
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
    */
    bool isActive()
    { 
        assert(m_xTimerHandler);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

        return (bool)xTimerIsTimerActive(m_xTimerHandler);
    }

#if (INCLUDE_xTimerPendFunctionCall == 1)
    /**
     * @brief Async call for function (from ISR or not)
     * 
     * @param xFunctionToPend The function to execute
     * @param pvParameter1 Pointer to the value of the callback function's first parameter.
     * @param ulParameter2 The value of the callback function's second parameter.
     * @param xMsToWait How much time to wait in milliseconds for free space in Async Queue
     * 
     * @return "true" if successful, "false" if not added to call subsystem
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
     * @note 3. If called from ISR executed as fast as possible on exit !
     * @note 4. xMsToWait is used NOT in ISR and might be omitted
     * 
     * @code{cpp}
     * ...
     * void AsyncMagic([[maybe_unused]] void *pvParameter1, [[maybe_unused]] uint32_t ulParameter2 )
     * {
     *   // some actions
     *   if (reinterpret_cast<int>(pvParameter1) == 42) {
     *     castWaffle();
     *   }
     * }
     * ...
     * OSTimer::AsyncCall(AsyncMagic);
     * ...
     * ...
     * auto spell = int{42};
     * OSTimer::AsyncCall(AsyncMagic, &spell);
     * ...
     * @endcode
    */
    static bool asyncCall(PendedFunction_t xFunctionToPend,
                            void* pvParameter1 = nullptr,
                            uint32_t ulParameter2 = 0u,
                            size_t xMsToWait = portMAX_DELAY_MS)
    {
        assert(xFunctionToPend);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (xFunctionToPend == nullptr) {
            return false;
        }

#if (__cplusplus >= 201703L)
        return (bool)execIsrFunc([&]() -> BaseType_t {
            return xTimerPendFunctionCall(xFunctionToPend,
                                            pvParameter1, ulParameter2,
                                            pdMS_TO_TICKS(xMsToWait));
        }, [&](auto status, auto yieldFunc) -> BaseType_t {
            auto res = xTimerPendFunctionCallFromISR(xFunctionToPend,
                                                    pvParameter1, ulParameter2,
                                                    status);
            yieldFunc(status);
            return res;
        });
#else
    BaseType_t res = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      res = xTimerPendFunctionCall(xFunctionToPend,
                                            pvParameter1, ulParameter2,
                                            pdMS_TO_TICKS(xMsToWait));
    } else {
      BaseType_t xHigherPriorityStatus = pdFALSE;
      res = xTimerPendFunctionCallFromISR(xFunctionToPend,
                                                    pvParameter1, ulParameter2,
                                                    &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return (bool)res;
#endif
    }
#endif
};
#endif // configUSE_TIMERS

// - - - - - - - - - - - - - - - - - - - - - - - -


// clang-format on

#endif // __cplusplus

#endif // _RTOS_HELPER_TIMER_HPP
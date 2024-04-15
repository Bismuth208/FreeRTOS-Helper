/**
 * @file rtos_helper_queue.hpp
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

#ifndef _RTOS_HELPER_QUEUE_HPP
#define _RTOS_HELPER_QUEUE_HPP

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/queue.h"
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
 * @brief Template class for Queue
 *
 * @code{cpp}
 * // Creation of Queue object with 128 elements of type uint32_t
 * OSQueue <128, uint32_t>TxQueue;
 * ...
 * {
 *     ...
 *     // Call an actual OS Queue creation
 *     TxQueue.init();
 *     ...
 * }
 * @endcode
 */
template <size_t QueueSize, class T>
class OSQueue
{
private:
    // Amount of items in Queue
    size_t m_xQueueSize = 0u;

    // An OS object handler.
    QueueHandle_t m_xQueueHandler = nullptr;

    // Status flag showing if @ref init() was done with success
    bool m_initialized = false;

#if (configSUPPORT_STATIC_ALLOCATION == 1)
    // Lines below are OS specific data in RAM created and located at compile time.
    StaticQueue_t m_xControlBlock;
    T m_xStorage[QueueSize];
#endif

    bool _receive(T* val, size_t xMsToWait)
    {
        assert(m_xQueueHandler);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

#if (__cplusplus >= 201703L)
        return (bool)execIsrFunc([&]() -> BaseType_t {
            return xQueueReceive(m_xQueueHandler, reinterpret_cast<void*>(val), pdMS_TO_TICKS(xMsToWait));
        }, [&](auto status, auto yieldFunc) -> BaseType_t {
            auto res = xQueueReceiveFromISR(m_xQueueHandler, reinterpret_cast<void*>(val), status);
            yieldFunc(status);
            return res;
        });
#else
    BaseType_t res = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      res = xQueueReceive(m_xQueueHandler, reinterpret_cast<void*>(val), pdMS_TO_TICKS(xMsToWait));
    } else {
      BaseType_t xHigherPriorityStatus = pdFALSE;
      res = xQueueReceiveFromISR(m_xQueueHandler, reinterpret_cast<void*>(val), &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return (bool)res;
#endif
    }

    bool _send(const T* val, size_t xMsToWait)
    {
        assert(m_xQueueHandler);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

#if (__cplusplus >= 201703L)
        return (bool)execIsrFunc([&]() -> BaseType_t {
            return xQueueSend(m_xQueueHandler, reinterpret_cast<const void*>(val), pdMS_TO_TICKS(xMsToWait));
        }, [&](auto status, auto yieldFunc) -> BaseType_t {
            auto res = xQueueSendFromISR(m_xQueueHandler, reinterpret_cast<const void*>(val), status);
            yieldFunc(status);
            return res;
        });
#else
    BaseType_t res = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      res = xQueueSend(m_xQueueHandler, reinterpret_cast<const void*>(val), pdMS_TO_TICKS(xMsToWait));
    } else {
      BaseType_t xHigherPriorityStatus = pdFALSE;
      res = xQueueSendFromISR(m_xQueueHandler, reinterpret_cast<const void*>(val), &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return (bool)res;
#endif
    }

    bool _peek(T* val, size_t xMsToWait)
    {
        assert(m_xQueueHandler);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

#if (__cplusplus >= 201703L)
        return (bool)execIsrFunc([&]() -> BaseType_t {
            return xQueuePeek(m_xQueueHandler, reinterpret_cast<void*>(val), pdMS_TO_TICKS(xMsToWait));
        }, [&](auto status, auto yieldFunc) -> BaseType_t {
            return xQueuePeekFromISR(m_xQueueHandler, reinterpret_cast<void*>(val));
        });
#else
    BaseType_t res = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      res = xQueuePeek(m_xQueueHandler, reinterpret_cast<void*>(val), pdMS_TO_TICKS(xMsToWait));
    } else {
      res = xQueuePeekFromISR(m_xQueueHandler, reinterpret_cast<void*>(val));
    }

    return (bool)res;
#endif
    }

public:
    OSQueue() : m_xQueueSize(QueueSize){};

    ~OSQueue()
    {
        assert(m_xQueueHandler);

        vQueueDelete(m_xQueueHandler);
        m_xQueueHandler = nullptr;
        m_initialized = false;
    };

    /**
     * @brief Create software Queue with OS functions
     * 
     * @return "true" if successful, "false" if not initialised
     * 
     * @note 1. This method is NOT thread-safe
     * @note 2. This method is NOT an ISR safe
    */
    bool init()
    {
        assert(m_xQueueSize != 0u);

#if (configSUPPORT_STATIC_ALLOCATION == 1)
        m_xQueueHandler = xQueueCreateStatic(
                            m_xQueueSize, sizeof(T), reinterpret_cast<uint8_t*>(m_xStorage),
                            &m_xControlBlock);
#else

        m_xQueueHandler = xQueueCreate(m_xQueueSize, sizeof(T));
#endif

        assert(m_xQueueHandler);
        if (m_xQueueHandler != nullptr) {
            m_initialized = true;
        }

        return m_initialized;
    }

    /**
     * @brief Get an OS Queue handler for direct manipulation
     * 
     * @retval Pointer to the OS type of the RAW handler.
     * 
     * @note 1. It's possible ONLY when @ref init() was DONE!
     * @note 2. Be careful, all what you will do, you'll do on your own risk !
    */
    QueueHandle_t getHandler()
    {
        return m_xQueueHandler;
    }

    /**
     * @brief Sets the size of the Queue
     * 
     * @param newSize New size of the Queue in elements
     * 
     * @return "true" if successful, "false" IF IT WAS initialised
     * 
     * @note It's possible ONLY when @ref init() was NOT DONE!
    */
    bool setSize(size_t newSize)
    {
        assert(m_xQueueHandler == nullptr);
        assert(m_initialized == false);
        assert(newSize != 0u);
        if (m_initialized) {
            return false;
        }

        m_xQueueSize = newSize;
        return true;
    }

    /**
     * @brief Get an item from the Queue
     * 
     * @param val Reference to the data where new item will be copied/sended from the Queue
     * @param xMsToWait How much time to wait in milliseconds for free space in Queue
     * 
     * @return "true" if it's sended,"false" if not initialised and/or: is empty, timeout reached
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
    */
    bool receive(T& val, size_t xMsToWait = portMAX_DELAY_MS)
    {
        return _receive(&val, xMsToWait);
    }

    /**
     * @brief Get an item from the Queue
     * 
     * @param val Pointer to the data where new item will be copied/sended from the Queue
     * @param xMsToWait How much time to wait in milliseconds for free space in Queue
     * 
     * @return "true" if it's sended,"false" if not initialised and/or: is empty, timeout reached
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
    */
    bool receive(T* val, size_t xMsToWait = portMAX_DELAY_MS)
    {
        return _receive(val, xMsToWait);
    }

    /**
     * @brief Send an item data to the Queue
     * 
     * @param val Reference to the data which will be copied/sended to the Queue
     * @param xMsToWait How much time to wait in milliseconds for free space in Queue
     * 
     * @return "true" if it's sended,"false" if not initialised and/or: no free space, timeout reached
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
    */
    bool send(const T& val, size_t xMsToWait = portMAX_DELAY_MS)
    {
        return _send(&val, xMsToWait);
    }

    /**
     * @brief Send an item data to the Queue
     * 
     * @param val Pointer to the data which will be copied/sended to the Queue
     * @param xMsToWait How much time to wait in milliseconds for free space in Queue
     * 
     * @return "true" if it's sended,"false" if not initialised and/or: no free space, timeout reached
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
    */
    bool send(const T* val, size_t xMsToWait = portMAX_DELAY_MS)
    {
        return _send(val, xMsToWait);
    }

    /**
     * @brief Get an item from the Queue without removing it from the Queue
     * 
     * @param val Reference to the data where new item will be copied/sended from the Queue
     * @param xMsToWait How much time to wait in milliseconds for an item in Queue
     * 
     * @return "true" if it's sended,"false" if not initialised and/or: no item in, timeout reached
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
    */
    bool peek(T& val, size_t xMsToWait = portMAX_DELAY_MS)
    {
        return _peek(&val, xMsToWait);
    }

    /**
     * @brief Get an item from the Queue without removing it from Queue
     * 
     * @param val Pointer to the data which will be copied/sended from the Queue
     * @param xMsToWait How much time to wait in milliseconds for an item in Queue
     * 
     * @return "true" if it's sended,"false" if not initialised and/or: no item in, timeout reached
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe
    */
    bool peek(T* val, size_t xMsToWait = portMAX_DELAY_MS)
    {
        return _peek(val, xMsToWait);
    }

    /**
     * @brief Get status flag if Queue is free
     * 
     * @return "true" if it's Empty, "false" if not initialised and/or no free space
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe (?)
    */
    bool isEmpty(void)
    {
        assert(m_xQueueHandler);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return true;
        }

        bool res = (uxQueueSpacesAvailable(m_xQueueHandler) == m_xQueueSize) ? true : false;
        return res;
    }

    /**
     * @brief Get amount of free items in Queue
     * 
     * @retval How much space is left or "-1" if not initialized
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe (?)
    */
    int32_t getFreeSpace(void)
    {
        assert(m_xQueueHandler);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return -1;
        }

        int32_t res = (int32_t) uxQueueSpacesAvailable(m_xQueueHandler);
        return res;  
    }

    /**
     * @brief Clear all pending items in Queue
     * 
     * @return "true" if successful, "false" if not initialised
     * 
     * @note 1. This method is thread-safe
     * @note 2. This method is an ISR safe (?)
    */
    bool fflush(void)
    {
        assert(m_xQueueHandler);
        assert(m_initialized == true);
        assert(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING);
        if (!m_initialized) {
            return false;
        }

        bool res = (bool)xQueueReset(m_xQueueHandler);
        return res;
    }
};

// - - - - - - - - - - - - - - - - - - - - - - - -


// clang-format on

#endif // __cplusplus

#endif // _RTOS_HELPER_QUEUE_HPP
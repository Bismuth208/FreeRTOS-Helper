/**
 * Helpful API to make use of FreeRTOS a little easier.
 * Supports both ESP-IDF and Arduino IDE.
 *
 * Author: Alexandr Antonov (@Bismuth208)
 * Licence: MIT
 *
 * Minimal FreeRTOS version: v10.4.3
 *
 * (*) support for RP2040 is not tested
 *
 * TODO:
 *  - Add Semaphore class;
 *  - Add xPortGetCoreID for multi-core systems;
 *  - Add more examples and howto;
 *  - Add EventGroups class;
 *  - Improve Task notifications;
 *  - Add RP2040 support;
 */

#ifndef _FREERTOS_HELPER_HPP
#define _FREERTOS_HELPER_HPP

// - - - - - - - - - - - - - - - - - - - - - - - -
#include <stdint.h>
#include <type_traits>

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#endif

// - - - - - - - - - - - - - - - - - - - - - - - -
#ifdef RP2040
#ifndef xPortInIsrContext()
#define xPortInIsrContext() portCHECK_IF_IN_ISR()

#ifdef portNOP()
#undef portNOP()
#define portNOP() __asm__ __volatile__("nop");
#endif
#endif

#if ((configNUM_CORES > 1) && (configUSE_CORE_AFFINITY == 1))

#define xTaskCreatePinnedToCore(taskCode, name, stackDepth, parameters,        \
                                priority, createdTask, coreAffinityMask)       \
  xTaskCreateAffinitySet(taskCode, name, stackDepth, parameters, priority,     \
                         1 << coreAffinityMask, createdTask)

#define xTaskCreateStaticPinnedToCore(taskCode, name, stackDepth, parameters,  \
                                      priority, stackBuffer, taskBuffer,       \
                                      coreAffinityMask)                        \
  xTaskCreateStaticAffinitySet(taskCode, name, stackDepth, parameters,         \
                               priority, stackBuffer, taskBuffer,              \
                               1 << coreAffinityMask)

#define OS_MCU_ENABLE_MULTICORE_SUPPORT
#endif
#endif

#if (defined(ESP32) || defined(ESP_PLATFORM))
// nop() issue fix
#ifndef XT_NOP
#define XT_NOP() __asm__ __volatile__("nop");
// #warning "Due to some reason XT_NOP() is not implemented !"
#endif

#define OS_MCU_ENABLE_MULTICORE_SUPPORT
#endif // ESP32

// - - - - - - - - - - - - - - - - - - - - - - - -

#ifdef OS_MCU_ENABLE_MULTICORE_SUPPORT
typedef enum {
  OS_MCU_CORE_0 = 0UL, // "Main Core" in ESP32 or Pi Pico
  OS_MCU_CORE_1,       // "App core"
  OS_MCU_CORE_NONE     // No core specified
} os_mcu_core_num_t;
#endif // OS_MCU_ENABLE_MULTICORE_SUPPORT

// - - - - - - - - - - - - - - - - - - - - - - - -

/**
 * @brief Template class for Task creation and manipulation
 *
 * @code{cpp}
 * // Create task on CPU0 ( PRO_CPU for ESP32) and provide 4096 words for it.
 * Task <4096>AppMainTask((TaskFunction_t) vAppMainTask, OS_MCU_CORE_0);
 * @endcode
 */
template <size_t StackSizeInWords> class Task {
public:
  TaskHandle_t m_xTask = NULL;
  const uint32_t m_ulStackSizeWords = 0UL;

#if configSUPPORT_STATIC_ALLOCATION
  StaticTask_t m_xTaskControlBlock;
  StackType_t m_xStack[StackSizeInWords];
#endif

  Task(void (*pxTaskFunc)(void *),
#ifdef OS_MCU_ENABLE_MULTICORE_SUPPORT
       os_mcu_core_num_t ePinnedCore = OS_MCU_CORE_NONE,
#endif
       UBaseType_t uxPriority = 1UL, void *const pvArgs = NULL,
       const char *const pcFuncName = "\0")
      : m_ulStackSizeWords(StackSizeInWords) {
#ifdef OS_MCU_ENABLE_MULTICORE_SUPPORT
#if configSUPPORT_STATIC_ALLOCATION
    if (ePinnedCore < OS_MCU_CORE_NONE) {
      m_xTask = xTaskCreateStaticPinnedToCore(
          static_cast<TaskFunction_t>(pxTaskFunc), pcFuncName,
          m_ulStackSizeWords, pvArgs, uxPriority, m_xStack,
          &m_xTaskControlBlock, (BaseType_t)ePinnedCore);
    } else {
      m_xTask = xTaskCreateStatic(static_cast<TaskFunction_t>(pxTaskFunc),
                                  pcFuncName, m_ulStackSizeWords, pvArgs,
                                  uxPriority, m_xStack, &m_xTaskControlBlock);
    }
#else
    if (ePinnedCore < OS_MCU_CORE_NONE) {
      xTaskCreatePinnedToCore(static_cast<TaskFunction_t>(pxTaskFunc),
                              pcFuncName, m_ulStackSizeWords, pvArgs,
                              uxPriority, &m_xTask, (BaseType_t)ePinnedCore);
    } else {
      xTaskCreate(static_cast<TaskFunction_t>(pxTaskFunc), pcFuncName,
                  m_ulStackSizeWords, pvArgs, uxPriority, &m_xTask);
    }
#endif // configSUPPORT_STATIC_ALLOCATION
#else
#if configSUPPORT_STATIC_ALLOCATION
    m_xTask = xTaskCreateStatic(static_cast<TaskFunction_t>(pxTaskFunc),
                                pcFuncName, m_ulStackSizeWords, pvArgs,
                                uxPriority, m_xStack, &m_xTaskControlBlock);
#else
    xTaskCreate(static_cast<TaskFunction_t>(pxTaskFunc), pcFuncName,
                m_ulStackSizeWords, pvArgs, uxPriority, &m_xTask);
#endif
#endif // OS_MCU_ENABLE_MULTICORE_SUPPORT
  }

#if INCLUDE_vTaskDelete
  ~Task() {
    // if (m_xTask != NULL) {
    vTaskDelete(m_xTask);
    m_xTask = NULL;
    // }
  }
#else
// WARNING!
#error "Memory leak! Please, enable vTaskDelete"
  ~Task() = default;
#endif

#if INCLUDE_vTaskSuspend
  /**
   * @brief If for some reason Task must be stopped
   */
  void stop(void) { vTaskSuspend(m_xTask); }
#endif

#if INCLUDE_vTaskResume
  /**
   * @brief If for some reason Task must be started
   */
  void start(void) {
    if (xPortInIsrContext() == pdFALSE) {
      vTaskResume(m_xTask);
    } else {
      BaseType_t xHigherPriorityStatus = xTaskResumeFromISR(m_xTask);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }
  }
#endif

#if configUSE_TASK_NOTIFICATIONS
  /**
   * @brief This method allow to unblock Task.
   *        Lock/unlock mechanism similar to binary semaphore,
   *        but according to docs for FreeRTOS it's way more lightweight.
   *
   * @note This method can be used in any task, even by task itself!
   */
  void emitSignal(void) {
    BaseType_t xHigherPriorityStatus = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      xTaskNotifyGive(m_xTask);
    } else {
      vTaskNotifyGiveFromISR(m_xTask, &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }
  }
#endif

#if configUSE_TASK_NOTIFICATIONS
  /**
   * @brief This method allow to block Task.
   *        Lock/unlock mechanism similar to binary semaphore,
   *        but according to docs for FreeRTOS it's way more lightweight.
   *
   * @note This is must be used inside Task what need to be blocked!
   */
  void waitSignal(TickType_t xTicksToWait = portMAX_DELAY) {
    do {
      portNOP();
    } while (ulTaskNotifyTake(pdFALSE, xTicksToWait) == pdFALSE);
  }
#endif

#if INCLUDE_vTaskDelay
  /**
   * @brief Yet another way to wait and pause(block) Task
   *
   * @code{cpp}
   *  // Block Task for 500 ms.
   *  Task<0>::delay(500);
   * @endcode
   */
  static void delay(TickType_t xTimerPeriodInMs = 1UL) {
    vTaskDelay(pdMS_TO_TICKS(xTimerPeriodInMs));
  }
#endif
};

// - - - - - - - - - - - - - - - - - - - - - - - -

/**
 * @brief Template class for Queue
 *
 * @code{cpp}
 * // Creation of Queue with 128 elements of type uint32_t
 * Queue <128, uint32_t>TxQueue;
 * @endcode
 */
template <size_t QueueSize, class T> class Queue {
public:
  const QueueHandle_t m_xQueueHandler = NULL;
  const size_t m_xQueueSize = 0UL;

#if configSUPPORT_STATIC_ALLOCATION
  StaticQueue_t m_xControlBlock;
  T m_xStorage[QueueSize]; // this is ok, thx to template

  Queue()
      : m_xQueueHandler(xQueueCreateStatic(
            QueueSize, sizeof(T), reinterpret_cast<uint8_t *>(m_xStorage),
            &m_xControlBlock)),
        m_xQueueSize(QueueSize){};
#else
  Queue()
      : m_xQueueHandler(xQueueCreate(QueueSize, sizeof(T))),
        m_xQueueSize(QueueSize){};
#endif

  ~Queue() {
    // if (m_xQueueHandler != NULL) {
    vQueueDelete(m_xQueueHandler);
    // m_xQueueHandler = NULL;
    // }
  };

  BaseType_t _receive(T *val, TickType_t xTicksToWait) {
    BaseType_t res = pdFALSE;
    BaseType_t xHigherPriorityStatus = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      res = xQueueReceive(m_xQueueHandler, val, xTicksToWait);
    } else {
      res = xQueueReceiveFromISR(m_xQueueHandler, val, &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return res;
  }

  BaseType_t receive(T val, TickType_t xTicksToWait = portMAX_DELAY) {
    return _receive(&val, xTicksToWait);
  }

  BaseType_t receive(T *val, TickType_t xTicksToWait = portMAX_DELAY) {
    return _receive(val, xTicksToWait);
  }

  BaseType_t _send(const T *val, TickType_t xTicksToWait = portMAX_DELAY) {
    BaseType_t res = pdFALSE;
    BaseType_t xHigherPriorityStatus = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      res = xQueueSend(m_xQueueHandler, val, xTicksToWait);
    } else {
      res = xQueueSendFromISR(m_xQueueHandler, val, &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return res;
  }

  BaseType_t send(T val, TickType_t xTicksToWait = portMAX_DELAY) {
    return _send(&val, xTicksToWait);
  }

  BaseType_t send(T *val, TickType_t xTicksToWait = portMAX_DELAY) {
    return _send(val, xTicksToWait);
  }

  BaseType_t isEmpty(void) {
    return (uxQueueSpacesAvailable(m_xQueueHandler) == m_xQueueSize) ? pdTRUE
                                                                     : pdFALSE;
  }

  BaseType_t fflush(void) { return xQueueReset(m_xQueueHandler); }
};

// - - - - - - - - - - - - - - - - - - - - - - - -

/**
 * @brief Implementation of Mutex Class over Semaphore
 *
 * @code{cpp}
 * Mutex spiMutex;  // creation of SPI mutex
 *
 * spiMutex.lock(); // blocks resource for other tasks
 * ... some actions ...
 * spiMutex.unlock(); // unblocks resource for other tasks
 * @endcode
 *
 * @note Do not use Mutexes inside ISR context!
 */
#if configUSE_MUTEXES
class Mutex {
public:
  const SemaphoreHandle_t m_xMutex = NULL;

#if configSUPPORT_STATIC_ALLOCATION
  StaticSemaphore_t m_xMutexControlBlock;

  Mutex() : m_xMutex(xSemaphoreCreateMutexStatic(&m_xMutexControlBlock)) {}
#else
  Mutex() : m_xMutex(xSemaphoreCreateMutex()) {}
#endif

  ~Mutex() {
    // if (m_xMutex != NULL) {
    vSemaphoreDelete(m_xMutex);
    // m_xMutex = NULL
    // }
  };

  BaseType_t lock(TickType_t xTicksToWait = portMAX_DELAY) {
    return xSemaphoreTake(m_xMutex, xTicksToWait);
  }

  BaseType_t unlock(void) { return xSemaphoreGive(m_xMutex); }
};
#endif // configUSE_MUTEXES

// - - - - - - - - - - - - - - - - - - - - - - - -

/**
 * @brief Implementation of Counter Class over Counting Semaphore
 *
 * @code{cpp}
 * // Creation:
 * Counter btnPressCounter;
 *
 * // In one Task, ISR or any callback:
 * ...
 * if (btn_read(SOME_BTN_NUM) == 1) {
 *   btnPressCounter.give();
 * }
 * ...
 *
 * // Meanwhile in other Task:
 * ...
 * while (btnPressCounter.take(0UL) == pdTRUE) {
 *   blink_ok_led();
 * }
 * ...
 * @endcode
 */
#if configUSE_COUNTING_SEMAPHORES
template <size_t MaxCount> class Counter {
public:
  const SemaphoreHandle_t m_xConunter = NULL;

#if configSUPPORT_STATIC_ALLOCATION
  StaticSemaphore_t m_xSemaphoreControlBlock;

  Counter()
      : m_xConunter(xSemaphoreCreateCountingStatic(MaxCount, 0,
                                                   &m_xSemaphoreControlBlock)) {
  }
#else
  Counter() : m_xConunter(xSemaphoreCreateCounting(MaxCount, 0)) {}
#endif

  ~Counter() {
    // if (m_xConunter != NULL) {
    vSemaphoreDelete(m_xConunter);
    // m_xConunter = NULL;
    // }
  };

  BaseType_t take(TickType_t xTicksToWait = portMAX_DELAY) {
    BaseType_t res = pdFALSE;
    BaseType_t xHigherPriorityStatus = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      res = xSemaphoreTake(m_xConunter, xTicksToWait);
    } else {
      res = xSemaphoreTakeFromISR(m_xConunter, &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return res;
  }

  BaseType_t give(void) {
    BaseType_t res = pdFALSE;
    BaseType_t xHigherPriorityStatus = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      res = xSemaphoreGive(m_xConunter);
    } else {
      res = xSemaphoreGiveFromISR(m_xConunter, &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return res;
  }

  void fflush(void) {
    BaseType_t xHigherPriorityStatus = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      while (xSemaphoreTake(m_xConunter, 1UL))
        ;
    } else {
      // This part is entirely wrong. Have to rewrite it someday...
      while (xSemaphoreTakeFromISR(m_xConunter, &xHigherPriorityStatus))
        ;

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }
  }
};
#endif // configUSE_COUNTING_SEMAPHORES

// - - - - - - - - - - - - - - - - - - - - - - - -

/**
 * @brief Template class for software Timer
 *
 * @code{cpp}
 * // creation of "one shot" timer
 * Timer wifiOffTimer;
 *
 * ...
 * if (xSomeCondition == pdTRUE) {
 *   if (wifiOffTimer.isActive() == pdFALSE) {
 *     // Shut down WiFi after 500ms.
 *     wifiOffTimer.start(500);
 *   }
 * }
 * ...
 * @endcode
 */
#if configUSE_TIMERS
class Timer {
public:
  TimerHandle_t m_xTimer = NULL;

#if configSUPPORT_STATIC_ALLOCATION
  StaticTimer_t m_xTimerControlBlock;
#endif

  Timer(void (*pxCallbackFunction)(TimerHandle_t),
        const UBaseType_t uxAutoReload = pdFALSE, void *const pvTimerID = NULL,
        const char *const pcTimerName = "\0")
#if configSUPPORT_STATIC_ALLOCATION
      : m_xTimer(xTimerCreateStatic(
            pcTimerName, 1, uxAutoReload, pvTimerID,
            static_cast<TimerCallbackFunction_t>(pxCallbackFunction),
            &m_xTimerControlBlock))
#else
      : m_xTimer(xTimerCreate(
            pcTimerName, 1, uxAutoReload, pvTimerID,
            static_cast<TimerCallbackFunction_t>(pxCallbackFunction)))
#endif
            {
                // nothing here yet
            };

  ~Timer() {
    // if (m_xTimer != NULL) {
    xTimerDelete(m_xTimer, 0UL); // delete immediately
    m_xTimer = NULL;
    // }
  };

  BaseType_t start(TickType_t xTimerPeriodInMs = 0UL) {
    BaseType_t res = pdFALSE;
    BaseType_t xHigherPriorityStatus = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      xTimerChangePeriod(m_xTimer, pdMS_TO_TICKS(xTimerPeriodInMs), 0UL);
      res = xTimerStart(m_xTimer, 0UL);
    } else {
      xTimerChangePeriodFromISR(m_xTimer, pdMS_TO_TICKS(xTimerPeriodInMs),
                                &xHigherPriorityStatus);
      res = xTimerStartFromISR(m_xTimer, &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return res;
  }

  BaseType_t stop() {
    BaseType_t res = pdFALSE;
    BaseType_t xHigherPriorityStatus = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      res = xTimerStop(m_xTimer, 0UL);
    } else {
      res = xTimerStopFromISR(m_xTimer, &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return res;
  }

  BaseType_t isActive() { return xTimerIsTimerActive(m_xTimer); }

  BaseType_t restart(TickType_t xTimerPeriodInMs = 0UL) {
    BaseType_t res = pdFALSE;
    BaseType_t xHigherPriorityStatus = pdFALSE;

    if (xPortInIsrContext() == pdFALSE) {
      res = xTimerReset(m_xTimer, pdMS_TO_TICKS(xTimerPeriodInMs));
    } else {
      res = xTimerResetFromISR(m_xTimer, &xHigherPriorityStatus);

      if (pdTRUE == xHigherPriorityStatus) {
        portYIELD_FROM_ISR();
      }
    }

    return res;
  }
};
#endif // configUSE_TIMERS

// - - - - - - - - - - - - - - - - - - - - - - - -

#endif /* _FREERTOS_HELPER_HPP */

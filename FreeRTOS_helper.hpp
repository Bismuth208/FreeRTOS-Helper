/*
 * Helpfull API to make use of FreeRTOS a little easier
 *
 * Author: Alexandr Antonov (@Bismuth208)
 *
 * Licence: MIT
 */

#ifndef FREERTOS_HELPER_HPP
#define FREERTOS_HELPER_HPP

// - - - - - - - - - - - - - - - - - - - - - - - -

#include <Arduino.h>

// - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * As more and more multicore microcontrollers oppear,
 * this one, no more belong to ESP32 only !
 */
#ifdef ESP32 // || RP2040
#  define OS_MCU_ENABLE_MULTICORE_SUPPORT
#endif


#ifdef OS_MCU_ENABLE_MULTICORE_SUPPORT
typedef enum {
  OS_MCU_CORE_0 = 0UL,  // "Main Core" in ESP32 or PiPico
  OS_MCU_CORE_1,        // "App core"
  OS_MCU_CORE_NONE      // No core specified
} os_mcu_core_num_t;
#endif // OS_MCU_ENABLE_MULTICORE_SUPPORT

// - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Template class for Task creation and manipulation
 *
 * Examples:
 * 
 * // Create task on CPU0 ( ESP32 like) and provide 4096 words for it in stack
 * Task <4096>AppMainTask((TaskFunction_t) vAppMainTask, OS_MCU_CORE_0);
 *
 */
template<size_t StackSizeInWords>
class Task
{
public:
  TaskHandle_t m_xTask = NULL;
  const uint32_t m_ulStackSizeWords = 0UL;

#if configSUPPORT_STATIC_ALLOCATION
  StaticTask_t m_xTaskControlBlock;
  StackType_t m_xStack[StackSizeInWords];
#endif

  Task(TaskFunction_t pxTaskFunc,
#ifdef OS_MCU_ENABLE_MULTICORE_SUPPORT
    os_mcu_core_num_t ePinnedCore = OS_MCU_CORE_NONE,
#endif
    UBaseType_t uxPriority = 1UL,
    void * const pvArgs = NULL,
    const char * const pcFuncName = "\0"
  ) : m_ulStackSizeWords(StackSizeInWords)
  {
#ifdef OS_MCU_ENABLE_MULTICORE_SUPPORT
#  if configSUPPORT_STATIC_ALLOCATION
    if (ePinnedCore < OS_MCU_CORE_NONE) {
      m_xTask = xTaskCreateStaticPinnedToCore( pxTaskFunc, pcFuncName, m_ulStackSizeWords, pvArgs, uxPriority, m_xStack, &m_xTaskControlBlock, (BaseType_t) ePinnedCore );
    } else {
      m_xTask = xTaskCreateStatic( pxTaskFunc, pcFuncName, m_ulStackSizeWords, pvArgs, uxPriority, m_xStack, &m_xTaskControlBlock );
    }
#  else
    if (ePinnedCore < OS_MCU_CORE_NONE) {
      xTaskCreatePinnedToCore( pxTaskFunc, pcFuncName, m_ulStackSizeWords, pvArgs, uxPriority, &m_xTask, (BaseType_t) ePinnedCore );
    } else {
      xTaskCreate( pxTaskFunc, pcFuncName, m_ulStackSizeWords, pvArgs, uxPriority, &m_xTask );
    }
#  endif // configSUPPORT_STATIC_ALLOCATION
#else
#  if configSUPPORT_STATIC_ALLOCATION
    m_xTask = xTaskCreateStatic( pxTaskFunc, pcFuncName, m_ulStackSizeWords, pvArgs, uxPriority, m_xStack, &m_xTaskControlBlock );
#  else
    xTaskCreate( pxTaskFunc, pcFuncName, m_ulStackSizeWords, pvArgs, uxPriority, &m_xTask );
#  endif
#endif // OS_MCU_ENABLE_MULTICORE_SUPPORT
  }

  /*
   * If for some reason Task must be stopped
   */
  void stop(void) {
    vTaskSuspend(m_xTask);
  }

  /*
   * If for some reason Task must be started
   */
  void start(void) {
    vTaskResume(m_xTask);
  }

  /*
   * This method allow to unblock Task.
   * Lock/unlock mechanism similar to binary semaphore,
   * but according to datasheet of FreeRTOS way more lightweight.
   *
   * NOTE!
   * This method can be used in any task, even by task itself.
   */
  void emitSignal(void) {
    xTaskNotifyGive(m_xTask);
  }

  /*
   * This method allow to block Task.
   * Lock/unlock mechanism similar to binary semaphore,
   * but according to datasheet of FreeRTOS way more lightweight.
   *
   * NOTE!
   * This is must be used inside Task what need to be blocked!
   */
  void waitSignal(TickType_t xTicksToWait = portMAX_DELAY) {
    ulTaskNotifyTake(pdFALSE, xTicksToWait);
  }
};

// - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Template class for Queue
 *
 * Examples:
 * 
 * // creation of Queue for 128 uint32_t elements in it
 * Queue <128, uint32_t>TxQueue;
 *
 */
template<size_t QueueSize, class T>
class Queue
{
public:
  const QueueHandle_t m_xQueueHandler = NULL;
  const size_t m_xQueueSize = 0UL;

#if configSUPPORT_STATIC_ALLOCATION
  StaticQueue_t m_xControlBlock;
  T m_xStorage[QueueSize]; // this is ok, thx to template

  Queue() : m_xQueueHandler( xQueueCreateStatic(QueueSize, sizeof(T), static_cast<uint8_t*>(m_xStorage), &m_xControlBlock) ), m_xQueueSize(QueueSize) {};
#else
  Queue() : m_xQueueHandler( xQueueCreate(QueueSize, sizeof(T)) ), m_xQueueSize(QueueSize) {};
#endif

  BaseType_t receive(T *val, TickType_t xTicksToWait = portMAX_DELAY) {
    return xQueueReceive(m_xQueueHandler, val, xTicksToWait);
  }

  BaseType_t send(const T *val, TickType_t xTicksToWait = portMAX_DELAY) {
    return xQueueSend(m_xQueueHandler, val, xTicksToWait);
  }

  BaseType_t isEmpty(void) {
    return (uxQueueSpacesAvailable(m_xQueueHandler) == m_xQueueSize) ? pdTRUE : pdFALSE;
  }

  BaseType_t fflush(void) {
    return xQueueReset(m_xQueueHandler);
  }
};

// - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Class for Mutex
 *
 * Examples:
 *
 * Mutex spiMutex;  // creation of SPI mutex
 *
 * spiMutex.lock(); // blocks resource for other tasks 
 * ... some actions ...
 * spiMutex.unlock(); // unblocks resource for other tasks
 */
class Mutex
{
public:
  const SemaphoreHandle_t m_xMutex = NULL;

#if configSUPPORT_STATIC_ALLOCATION
  StaticSemaphore_t m_xMutexControlBlock;

  Mutex() : m_xMutex ( xSemaphoreCreateMutexStatic(&m_xMutexControlBlock) ) {}
#else
  Mutex() : m_xMutex ( xSemaphoreCreateMutex() ) {}
#endif

  BaseType_t lock(TickType_t xTicksToWait = portMAX_DELAY) {
    return xSemaphoreTake(m_xMutex, xTicksToWait);
  }

  BaseType_t unlock(void) {
    return xSemaphoreGive(m_xMutex);
  }
};

// - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Class for Counting Semaphore
 *
 * Examples:
 *
 * Counter btnPressCounter;  // creation
 *
 * if (btn_read(SOME_BTN_NUM) == 1) {
 *   btnPressCounter.give();
 * }
 *
 * Meanwhile in other Task:
 * while (btnPressCounter.take(0UL) == pdTRUE) {
 *   blink_ok_led();
 * }
 */
template<size_t MaxCount>
class Counter
{
public:
  const SemaphoreHandle_t m_xConunter = NULL;

#if configSUPPORT_STATIC_ALLOCATION
  StaticSemaphore_t m_xSemaphoreControlBlock;

  Counter() : m_xConunter( xSemaphoreCreateCounting(MaxCount, 0, &m_xSemaphoreControlBlock) ) {}
#else
  Counter() : m_xConunter( xSemaphoreCreateCounting(MaxCount, 0) ) {}
#endif

  BaseType_t take(TickType_t xTicksToWait = portMAX_DELAY) {
    return xSemaphoreTake(m_xConunter, xTicksToWait);
  }

  BaseType_t give(void) {
    return xSemaphoreGive(m_xConunter);
  }

  void fflush(void) {
    while (xSemaphoreTake(m_xConunter, 1));
  }
};

// - - - - - - - - - - - - - - - - - - - - - - - -

/*
 * Template class for software Timer
 *
 * Examples:
 * 
 * // creation of "one shot" timer
 * Timer wifiOffTimer;
 *
 * if (something == pdTRUE) {
 *   if (wifiOffTimer.isActive() == pdFALSE) {
 *     wifiOffTimer.start(500); // shut down WiFi after 500ms.
 *   }
 * }
 */
class Timer
{
public:
  TimerHandle_t m_xTimer = NULL;

#if configSUPPORT_STATIC_ALLOCATION
  StaticTimer_t m_xTimerControlBlock;
#endif

  Timer( TimerCallbackFunction_t pxCallbackFunction,
    const UBaseType_t uxAutoReload = pdFALSE,
    void *const pvTimerID = NULL,
    const char *const pcTimerName = "\0" )
#if configSUPPORT_STATIC_ALLOCATION
  : m_xTimer( xTimerCreateStatic(pcTimerName, 1, uxAutoReload, pvTimerID, pxCallbackFunction, &m_xTimerControlBlock) )
#else
  : m_xTimer( xTimerCreate(pcTimerName, 1, uxAutoReload, pvTimerID, pxCallbackFunction) )
#endif
  {
    // nothing here yet
  };

  BaseType_t start(TickType_t xTimerPeriodInMs = 0UL) {
    TickType_t xTiks = xTimerPeriodInMs / portTICK_PERIOD_MS;
    
    xTimerChangePeriod(m_xTimer, xTiks, 0);
    return xTimerStart(m_xTimer, 0);
  }

  BaseType_t stop() {
    return xTimerStop(m_xTimer, 0);
  }

  BaseType_t isActive() {
    return xTimerIsTimerActive(m_xTimer);
  }
  
#if 0
  BaseType_t restart(TickType_t xTimerPeriodInMs = 0) {
    return xTimerReset(m_xTimer, (xTimerPeriodInMs * portTICK_PERIOD_MS));
  }
#endif
};

// - - - - - - - - - - - - - - - - - - - - - - - -

#endif /* FREERTOS_HELPER_HPP */

#ifndef FREERTOS_HELPER_HPP
#define FREERTOS_HELPER_HPP

// - - - - - - - - -- - - - - - - - - - - - - - -

#include <Arduino.h>

// - - - - - - - - -- - - - - - - - - - - - - - -

template<size_t StackSizeInWords>
class Task
{
public:
  TaskHandle_t m_xTask = NULL;
  const uint32_t m_ulStackSizeWords = 0;

#if configSUPPORT_STATIC_ALLOCATION
  StaticTask_t m_xTaskControlBlock;
  StackType_t m_xStack[StackSizeInWords];
#endif

  Task(TaskFunction_t pxTaskFunc,
#ifdef ESP32
    uint32_t ulPinnedCore = 2,
#endif
    UBaseType_t uxPriority = 1,
    void * const pvArgs = NULL,
    const char * const pcFuncName = "\0"
  ) : m_ulStackSizeWords(StackSizeInWords)
  {
#ifdef ESP32
#if configSUPPORT_STATIC_ALLOCATION
    if (ulPinnedCore < 2) {
      m_xTask = xTaskCreateStaticPinnedToCore( pxTaskFunc, pcFuncName, m_ulStackSizeWords, pvArgs, uxPriority, m_xStack, &m_xTaskControlBlock, (BaseType_t) ulPinnedCore );
    } else {
      m_xTask = xTaskCreateStatic( pxTaskFunc, pcFuncName, m_ulStackSizeWords, pvArgs, uxPriority, m_xStack, &m_xTaskControlBlock );
    }
#else
    if (ulPinnedCore < 2) {
      xTaskCreatePinnedToCore( pxTaskFunc, pcFuncName, m_ulStackSizeWords, pvArgs, uxPriority, &m_xTask, ulPinnedCore );
    } else {
      xTaskCreate( pxTaskFunc, pcFuncName, m_ulStackSizeWords, pvArgs, uxPriority, &m_xTask );
    }
#endif // configSUPPORT_STATIC_ALLOCATION
#else
#if configSUPPORT_STATIC_ALLOCATION
    m_xTask = xTaskCreateStatic( pxTaskFunc, pcFuncName, m_ulStackSizeWords, pvArgs, uxPriority, m_xStack, &m_xTaskControlBlock );
#else
    xTaskCreate( pxTaskFunc, pcFuncName, m_ulStackSizeWords, pvArgs, uxPriority, &m_xTask );
#endif
#endif // ESP32
  }

  void stop(void) {
    vTaskSuspend(m_xTask);
  }

  void start(void) {
    vTaskResume(m_xTask);
  }

  void emitSignal(void) {
    xTaskNotifyGive(m_xTask);
  }

  void waitSignal(TickType_t xTicksToWait = portMAX_DELAY) {
    do {
      // taskYIELD(); // TODO: add something here
    } while (ulTaskNotifyTake(pdFALSE, xTicksToWait) == 0UL);
  }
};

// - - - - - - - - -- - - - - - - - - - - - - - -

template<size_t QueueSize, class T>
class Queue
{
public:
  const QueueHandle_t m_xQueueHandler = NULL;
  const size_t m_xQueueSize = 0;

#if configSUPPORT_STATIC_ALLOCATION
  StaticQueue_t m_xControlBlock;
  T m_xStorage[m_xQueueSize]; // this is ok, thx to template

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

// - - - - - - - - -- - - - - - - - - - - - - - -

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

// - - - - - - - - -- - - - - - - - - - - - - - -

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

// - - - - - - - - -- - - - - - - - - - - - - - -

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

  BaseType_t start(TickType_t xTimerPeriodInMs = 0) {
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

// - - - - - - - - -- - - - - - - - - - - - - - -

#endif /* FREERTOS_HELPER_HPP */

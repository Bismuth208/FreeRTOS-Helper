#ifndef FREERTOS_HELPER_HPP
#define FREERTOS_HELPER_HPP

// - - - - - - - - -- - - - - - - - - - - - - - -

#include "Arduino.h"

// - - - - - - - - -- - - - - - - - - - - - - - -

class Task
{
public:
  TaskHandle_t m_xTask = NULL;

  Task(TaskFunction_t pxTaskFunc,
    uint32_t ulStackSizeWords = configMINIMAL_STACK_SIZE,
    UBaseType_t uxPriority = 1,
#ifdef ESP32
    uint32_t ulPinnedCore = 2,
#endif
    void * const pvArgs = NULL,
    const char * const pcFuncName = "\0"
  )
  {
    if (ulPinnedCore > 1) {
      xTaskCreate( pxTaskFunc, pcFuncName, ulStackSizeWords, pvArgs, uxPriority, &m_xTask );
    } else {
      xTaskCreatePinnedToCore( pxTaskFunc, pcFuncName, ulStackSizeWords, pvArgs, uxPriority, &m_xTask, ulPinnedCore );
    }
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

  UBaseType_t waitSignal(TickType_t xTicksToWait = portMAX_DELAY) {
    return ulTaskNotifyTake(pdFALSE, xTicksToWait);
  }
};

// - - - - - - - - -- - - - - - - - - - - - - - -

template<size_t QueueSize, class T>
class Queue
{
public:
  const QueueHandle_t m_xQueueHandler = NULL;
  const size_t m_xQueueSize = 0;

  Queue() : m_xQueueHandler(xQueueCreate(QueueSize, sizeof(T))), m_xQueueSize(QueueSize) {};

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

  Mutex() : m_xMutex (xSemaphoreCreateMutex()) {}

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

  Counter() : m_xConunter(xSemaphoreCreateCounting(MaxCount, 0)) {}

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
  TimerHandle_t m_xTimerHandler = NULL;

  Timer( TimerCallbackFunction_t pxCallbackFunction,
    const UBaseType_t uxAutoReload = pdFALSE,
    void *const pvTimerID = NULL,
    const char *const pcTimerName = "\0" )
  {
    m_xTimerHandler = xTimerCreate(pcTimerName, 1, uxAutoReload, pvTimerID, pxCallbackFunction);
  };

  BaseType_t start(TickType_t xTimerPeriodInMs = 0) {
    TickType_t xTiks = xTimerPeriodInMs / portTICK_PERIOD_MS;
    
    xTimerChangePeriod(m_xTimerHandler, xTiks, 0);
    return xTimerStart(m_xTimerHandler, 0);
  }

  BaseType_t stop() {
    return xTimerStop(m_xTimerHandler, 0);
  }
  
#if 0
  BaseType_t restart(TickType_t xTimerPeriodInMs = 0) {
    return xTimerReset(m_xTimerHandler, (xTimerPeriodInMs * portTICK_PERIOD_MS));
  }
#endif
};

// - - - - - - - - -- - - - - - - - - - - - - - -

#endif /* FREERTOS_HELPER_HPP */

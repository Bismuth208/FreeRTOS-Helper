/**
 * @file rtos_helper_core.hpp
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

#ifndef _RTOS_HELPER_CORE_HPP
#define _RTOS_HELPER_CORE_HPP

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

/* -------------------------------------------------------------- */
/* -------------------- Definitions ------------------------ */
/* -------------------------------------------------------------- */

// Even if this is stupid
// This will work with different tick period, tho...
#define portMAX_DELAY_MS (portMAX_DELAY * portTICK_PERIOD_MS)


// - - - - - - - - - - - - - - - - - - - - - - - -
#ifdef RP2040
#ifndef xPortInIsrContext()
#define xPortInIsrContext() portCHECK_IF_IN_ISR()

#ifdef portNOP()
#undef portNOP()
#define portNOP() __asm__ __volatile__("nop");
#endif // portNOP
#endif // xPortInIsrContext

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
#endif // XT_NOP

#define OS_MCU_ENABLE_MULTICORE_SUPPORT
#endif // ESP32

// - - - - - - - - - - - - - - - - - - - - - - - -

#ifdef OS_MCU_ENABLE_MULTICORE_SUPPORT
typedef enum {
  OS_MCU_CORE_0 = 0UL, // "Main Core" in ESP32 or Pi Pico
  OS_MCU_CORE_1,       // "App core"
  OS_MCU_CORE_NONE     // No core specified
} os_mcu_core_num_t;
#else
typedef enum {
  OS_MCU_CORE_NONE     // No core specified. In cse of single core MCU
} os_mcu_core_num_t;
#endif // OS_MCU_ENABLE_MULTICORE_SUPPORT

// - - - - - - - - - - - - - - - - - - - - - - - -

/* -------------------------------------------------------------- */
/* -------------------- Global functions ------------------------ */
/* -------------------------------------------------------------- */

// If compiler version is newer or equal to C++17
// then use some advanced features
#if (__cplusplus >= 201703L)

// It will yield ONLY if it requred by status !
const auto yieldFunc = [&](BaseType_t* status) {portYIELD_FROM_ISR(*status);};


// Generic lambda
// Execute "a" only if context is not in ISR, "b" if yes
const auto execIsrFunc = [&](auto a, auto b)
{
    BaseType_t xHigherPriorityStatus = pdFALSE;
    return (xPortIsInsideInterrupt() == pdFALSE) ? a() : b(&xHigherPriorityStatus, yieldFunc);
};

#endif // __cplusplus >= 201703L

// - - - - - - - - - - - - - - - - - - - - - - - -

// clang-format on

#endif // __cplusplus

#endif // _RTOS_HELPER_CORE_HPP
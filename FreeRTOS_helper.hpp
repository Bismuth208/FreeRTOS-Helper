/**
 * @file FreeRTOS_helper.h
 * 
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
 */

#ifndef _FREERTOS_HELPER_HPP
#define _FREERTOS_HELPER_HPP


#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "freertos/FreeRTOS.h"
#include "freertos/FreeRTOSConfig.h"
#endif


#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#ifdef __cplusplus

#include "helpers/rtos_helper_core.hpp"
#include "helpers/rtos_helper_task.hpp"
#include "helpers/rtos_helper_queue.hpp"
#include "helpers/rtos_helper_mutex.hpp"
#include "helpers/rtos_helper_counter.hpp"
#include "helpers/rtos_helper_timer.hpp"

// clang-format off

/* -------------------------------------------------------------- */
/* -------------------- Definitions ------------------------ */
/* -------------------------------------------------------------- */

// - - - - - - - - - - - - - - - - - - - - - - - -

/* -------------------------------------------------------------- */
/* -------------------- Global functions ------------------------ */
/* -------------------------------------------------------------- */



// - - - - - - - - - - - - - - - - - - - - - - - -


// clang-format on

#endif // __cplusplus

#endif // _FREERTOS_HELPER_HPP

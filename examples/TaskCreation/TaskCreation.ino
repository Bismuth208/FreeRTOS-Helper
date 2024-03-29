#include "FreeRTOS_helper.hpp"

// Declaration of Task code
void vAppMainTask(void* pvArg);

// Create task with 768 words for stack.
// But be careful, size of stack depends on your MCU
// AND things what you will be implementing!
// Too much stack is ok, but will waste free RAM.
// Too low value will cause "Stack overflow" and firmware will fail.
OSTask <768> AppMainTask(vAppMainTask, "AppMainTask");

// This is usual procedure
void setup()
{
  Serial.begin(115200);

  AppMainTask.init();

  // This is way of delayed start of Task
  // And also safe way to use multithreading
  AppMainTask.emitSignal();
}

// Most of the time this routine will not be needed
void loop()
{
  // so, just switch to another task
  OSTask<0>::yield();
}

// This is Task's regular function similar to loop()
// BUT there is at least two general rules:
//   - no any return !
//   - every created task MUST contain endless loop
void vAppMainTask([[maybe_unused]] void* pvArg)
{
  // In this example passed argument is not used
  // as at creation moment we provide nothing

  // As Serial is used in this Task, we need to wait
  // until it will be initialised in setup()
  // Because usage of not initialised object - IS NOT SAFE
  AppMainTask.waitSignal();

  for (;;) {
    Serial.print("Hello from: ");
    Serial.println(AppMainTask.getName());

    // This is regular delay for FreeRTOS task
    // Every Task, if possible, must be blocked for somehow:
    //  - delay
    //  - mutex lock\unlock
    //  - queue wait
    //  - e.t.c 
    OSTask<0>::delay(500); // for 500ms.
  }
}

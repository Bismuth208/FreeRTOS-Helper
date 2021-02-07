#include "FreeRTOS_helper.hpp"

// Create task with 2048 words for stack.
// But be carefull, size of stack depends on your microconteller
// AND things what you will be implementing!
// Too much stack is ok, but will waste free RAM.
// Too low value will cause "Stack oveflow" and firmware will fail.
Task <2048> AppMainTask(vAppMainTask);

// This is usual procedure
void setup()
{
  Serial.begin(9600);

  // This is way of delayed start of Task
  // And also safe way to use multithreading
  AppMainTask.emitSignal();
}

// Most of the time this routine will not be needed
void loop()
{
  // so, just switch to another task
  taskYIELD();
}

// This is Task's regular function simmilar to loop()
// BUT there is at least two general rules:
//   - no any return !
//   - every created task MUST contain endless loop
void vAppMainTask(void *pvArg)
{
  // In this example passed argument is not used
  // as at creation moment we provide nothing
  (void) pvArg;

  // As Serial is used in this Task, we need to wait
  // until it will be inited in setup()
  // Because usage of not initialised object - IS NOT SAFE
  AppMainTask.waitSignal();

  for (;;) {
    Serial.println("Hello from vAppMainTask !");

    // This is regualar delay for FreeRTOS task
    // Every Task, if possible, must be blocked for somehow:
    //  - delay
    //  - mutex lock\unlock
    //  - queue wait
    //  - e.t.c 
    Task<0>::delay(500); // for 500ms.
  }
}

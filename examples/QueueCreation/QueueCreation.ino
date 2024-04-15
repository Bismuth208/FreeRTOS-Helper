#include "FreeRTOS_helper.hpp"

// Declaration of Task code
void vAppMainTask(void* pvArg);

// Create task with 768 words for stack.
// But be careful, size of stack depends on your MCU
// AND things what you will be implementing!
// Too much stack is ok, but will waste free RAM.
// Too low value will cause "Stack overflow" and firmware will fail.
OSTask <768> AppMainTask(vAppMainTask, "AppMainTask");

// Create Queue for 5 elements of type bool
OSQueue <5, bool>AppQueue;

// This is usual procedure
void setup()
{
  Serial.begin(115200);

  AppQueue.init();
  AppMainTask.init();
}

// Most of the time this routine will not be needed
void loop()
{
  static bool testSwitch = false;

  testSwitch = !testSwitch;
  AppQueue.send(testSwitch);

  // Just switch to another task for 500ms
  OSTask<0>::delay(500);
}

// This is Task's regular function similar to loop()
// BUT there is at least two general rules:
//   - no any return !
//   - every created task MUST contain endless loop
void vAppMainTask([[maybe_unused]] void* pvArg)
{
  // In this example passed argument is not used
  // as at creation moment we provide nothing

  for (;;) {
    bool status = false;

    // This task will be blocked here until something apper in Queue
    if (AppQueue.receive(status)) {
      Serial.println( status ? "Bep!" : "Bop!");
    }
  }
}

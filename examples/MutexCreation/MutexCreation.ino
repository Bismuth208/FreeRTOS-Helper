#include <assert.h>

#include "FreeRTOS_helper.hpp"

// Declaration of Task code
void vAppMainTask(void* pvArg);

// Create multiple Tasks with 768 words for stack.
// Both Tasks use the same code.
// But be careful, size of stack depends on your MCU
// AND things what you will be implementing!
// Too much stack is ok, but will waste free RAM.
// Too low value will cause "Stack overflow" and firmware will fail.
OSTask <768> AppFistTask(vAppMainTask, "AppFistTask");
OSTask <768> AppSecondTask(vAppMainTask, "AppSecondTask");

OSMutex serialPrintMutex;

// This is usual procedure
void setup()
{
  Serial.begin(115200);

  // At first, create Sync primitive as Mutex
  serialPrintMutex.init();

  // Pass task object to the task on it's creation as an argument
  AppFistTask.setArg(reinterpret_cast< void* >(&AppFistTask));
  AppSecondTask.setArg(reinterpret_cast< void* >(&AppSecondTask));

  // Create and initialize tasks
  AppFistTask.init();
  AppSecondTask.init();
}

// Most of the time this routine will not be needed
void loop()
{
  // so, just switch to another task
  OSTask<0>::yield();
}

void printMessage(const char* msg)
{
  serialPrintMutex.lock();

  Serial.print("Hello from: ");
  Serial.println(msg);

  serialPrintMutex.unlock();
}

// This is Task's regular function similar to loop()
// BUT there is at least two general rules:
//   - no any return !
//   - every created task MUST contain endless loop
void vAppMainTask([[maybe_unused]] void* pvArg)
{
  assert(pvArg != nullptr);

  auto AppTask = reinterpret_cast< OSTask<0>* >(pvArg);

  for (;;) {
    printMessage(AppTask->getName());

    // This is regular delay for FreeRTOS task
    // Every Task, if possible, must be blocked for somehow:
    //  - delay
    //  - mutex lock\unlock
    //  - queue wait
    //  - e.t.c 
    OSTask<0>::delay(500); // for 500ms.
  }
}

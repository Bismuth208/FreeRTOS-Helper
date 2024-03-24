#include "FreeRTOS_helper.hpp"

// Declaration of Task code
void vAppTimerFunc(void* pvArg);

// Create a software Timer.
// This timer is created with 'autoload" flag.
// It will call provided function every specified time on start.
OSTimer AppTimer(vAppTimerFunc, "AppTimer", true);

// This is usual procedure
void setup()
{
  Serial.begin(115200);

  AppTimer.init();

  // This is way to launch timer and call it's function every 500 milliseconds
  AppTimer.start(500);
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
void vAppTimerFunc([[maybe_unused]] void* pvArg)
{
  // In this example passed argument is not used
  // as at creation moment we provide nothing

  Serial.print("Hello from: ");
  Serial.println(AppTimer.getName());
}

# FreeRTOS Helper
***

Simple and flexible wrapper library for those who came from Arduino world and want something more, but don't know where to start.
Even embedded programers can find clever use cases of C++ with *FreeRTOS*!

Every task is now a simple object!
```
...
#include "FreeRTOS_helper.hpp"
...
OSTask <2048u> AppMainTask(vAppMainTask, "AppMainTask");
```
And boom!
You just created your first task!

But don't forget to describe *vAppMainTask* as regular function like this:
```
void vAppMainTask(void *pvArg)
{
  for (;;) {
    // your code here
  }
}
```

And finish it with initialization:
```
void setup()
{
  AppMainTask.init();
}
```

What? Still not impressed?
But, what about support for multi-core Tasks creation for ESP32 and/or RP2040 (Pi Pico in future)?
So, just add extra params to describe which core you want to assign for Task:
```
OSTask <4096u>AppMainTask(vAppMainTask, "AppMainTask", nullptr, SomeTaskPriority, OS_MCU_CORE_0);
```
Yes, it's that simple!

And no more long and misleading names for different actions!
If you need to lock Mutex just type:
```
yourMutexName.lock();
```
***
#### But there is much more!
Implemented Classes:
 - Task;
 - Mutex;
 - Timer;
 - Queue;
 - Counter Semaphore;

 TODO:
 - Add Semaphore class;
 - Add xPortGetCoreID for multi-core systems;
 - Add more examples and howto;
 - Add EventGroups class;
 - Add & check RP2040 support;
***
In case if you're familiar with FreeRTOS and it's memory models.
> *Static* objects creation supported as well as *Dynamic*, but not both simultaneously!
> If both methods are enabled, then only *Static* creation will be used!
> Select one of options for allocation model in your *FreeRTOSConfig.h*:
>   - configSUPPORT_STATIC_ALLOCATION;
>   - configSUPPORT_DYNAMIC_ALLOCATION;

# FreeRTOS Helper
***

Simple and flexible wrapper library for those who came from Arduino world and want something more, but don't know where to start.
Even embedded programers can find clever use cases of C++ with *FreeRTOS*!

Every task is now a simple object!
```
Task <4096>AppMainTask((TaskFunction_t) vAppMainTask);
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

What? Still not impressed?
But, what about support for multicore Tasks creation for ESP32 and RP2040 (Pi Pico in future)?
So, just add extra param to describe which core you want to assign for Task:
```
Task <4096>AppMainTask((TaskFunction_t) vAppMainTask, OS_MCU_CORE_0);
```
Yes, it's that simple!

And no more long and misleading names for diffrent actions!
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

***
In case if you're familiar with FreeRTOS and it's memory models.
> *Static* objects creation supported as well as *Dynamic*, but not both simultaneously!
> Select one of options for allocation model in your *FreeRTOSConfig.h*:
>   - configSUPPORT_STATIC_ALLOCATION;
>   - configSUPPORT_DYNAMIC_ALLOCATION;

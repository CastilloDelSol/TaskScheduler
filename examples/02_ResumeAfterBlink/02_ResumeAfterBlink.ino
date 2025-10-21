// 02_AdvamcedBlink.ino

#include <Arduino.h>
#include "TaskScheduler.h"

#define NUM_TASKS 2

// Constants
static const uint16_t ON_MS     = 100;   // LED on-time
static const uint16_t PERIOD_MS = 1000;  // full blink period

TaskScheduler<NUM_TASKS> tm;

// Task Handles
uint8_t hOn  = IScheduler::INVALID_TASK_ID;
uint8_t hOff = IScheduler::INVALID_TASK_ID;

void ledOnTask()
{
  digitalWrite(LED_BUILTIN, HIGH);

  if (hOff != IScheduler::INVALID_TASK_ID)
  {
    tm.vTaskResumeAfter(hOff, ON_MS);
  }

  tm.vTaskDelayUntil(PERIOD_MS);
}

void ledOffTask()
{
  digitalWrite(LED_BUILTIN, LOW);
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  hOff = tm.xTaskCreate(ledOffTask, /*delayMs=*/0, /*prio=*/2);
  hOn  = tm.xTaskCreate(ledOnTask,  /*delayMs=*/0, /*prio=*/1);

  tm.vTaskSuspend(hOff);
}

void loop()
{
  tm.run();
}

// 01_Blink.ino

#include <Arduino.h>
#include "TaskScheduler.h"

#define NUM_TASKS 1

TaskScheduler<NUM_TASKS> tm;

uint8_t hLed = IScheduler::INVALID_TASK_ID;

void ledTask()
{ 
  static const uint16_t INTERVAL = 500;

  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));

  tm.vTaskDelayUntil(INTERVAL);
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  hLed = tm.xTaskCreate(ledTask, 0, 0);
}

void loop()
{
  tm.run();
}
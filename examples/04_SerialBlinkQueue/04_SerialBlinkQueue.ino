#include <Arduino.h>
#include "TaskScheduler.h"
#include "TaskQueue.h"

// ===== Config =====
#define NUM_TASKS 3
#define QUEUE_SIZE 8

// ===== Scheduler & Queue =====
TaskScheduler<NUM_TASKS> tm;
struct Cmd
{
  enum : uint8_t
  {
    SetDuty,
    SetPeriod,
    Status
  } type;
  uint16_t v; // duty in %, or period in ms (depending on type)
};

TinyQueue<Cmd, QUEUE_SIZE> q;

// ===== Task handles =====
uint8_t hOn = IScheduler::INVALID_TASK_ID;
uint8_t hOff = IScheduler::INVALID_TASK_ID;
uint8_t hCtl = IScheduler::INVALID_TASK_ID;

// ===== LED OFF one-shot =====
void ledOffTask()
{
  digitalWrite(LED_BUILTIN, LOW);
  // no delay => auto-suspend until armed by ledOnTask
}

// ===== LED ON pacer & command consumer =====
void ledOnTask()
{
  // Local state (was global): defaults
  static uint16_t period_ms = 1000; // full cycle
  static uint8_t duty_pct = 50;     // 0..100

  // 1) Drain all pending config commands (non-blocking)
  Cmd c;
  while (q.xQueueReceive(c, /*block=*/true))
  {
    switch (c.type)
    {
    case Cmd::SetDuty:
      duty_pct = (c.v > 100) ? 100 : (uint8_t)c.v;
      Serial.print(F("OK duty="));
      Serial.print(duty_pct);
      Serial.println(F("%"));
      break;
    case Cmd::SetPeriod:
      if (c.v < 10)
        c.v = 10;
      if (c.v > 60000)
        c.v = 60000;
      period_ms = c.v;
      Serial.print(F("OK period="));
      Serial.print(period_ms);
      Serial.println(F(" ms"));
      break;
    case Cmd::Status:
      Serial.print(F("period="));
      Serial.print(period_ms);
      Serial.print(F(" ms, duty="));
      Serial.print(duty_pct);
      Serial.println(F("%"));
      break;
    }
  }

  // 2) Register as waiter so future sends wake us immediately (no polling)
  //    (This doesn't pop; it just tells TinyQueue who to notify.)
  //(void)q.xQueueReceive(c, /*block=*/true);

  // 3) Run this cycle with current settings
  const uint16_t on_ms = (uint32_t)period_ms * duty_pct / 100u;

  if (duty_pct == 0)
  {
    digitalWrite(LED_BUILTIN, LOW);
  }
  else if (duty_pct >= 100)
  {
    digitalWrite(LED_BUILTIN, HIGH);
  }
  else
  {
    digitalWrite(LED_BUILTIN, HIGH);
    if (hOff != IScheduler::INVALID_TASK_ID)
      tm.vTaskResumeAfter(hOff, on_ms);
  }

  // 4) Keep a drift-free cadence
  tm.vTaskDelayUntil(period_ms);
}

// ===== Serial control: parse lines -> enqueue commands =====
void controlTask()
{
  static const uint16_t POLL_MS = 5;
  static char buf[48];
  static uint8_t len = 0;

  while (Serial.available() > 0)
  {
    char c = (char)Serial.read();
    if (c == '\r')
      continue;
    if (c == '\n')
    {
      buf[len] = '\0';
      len = 0;

      Cmd cmd;
      if (strncmp(buf, "duty ", 5) == 0)
      {
        long v = strtol(buf + 5, nullptr, 10);
        cmd.type = Cmd::SetDuty;
        cmd.v = (v < 0) ? 0 : (v > 100 ? 100 : (uint16_t)v);
        if (!q.xQueueSend(cmd))
          Serial.println(F("ERR queue full"));
      }
      else if (strncmp(buf, "period ", 7) == 0)
      {
        long v = strtol(buf + 7, nullptr, 10);
        if (v < 0)
          v = 0;
        cmd.type = Cmd::SetPeriod;
        cmd.v = (uint16_t)v;
        if (!q.xQueueSend(cmd))
          Serial.println(F("ERR queue full"));
      }
      else if (strcmp(buf, "status") == 0)
      {
        cmd.type = Cmd::Status;
        cmd.v = 0;
        if (!q.xQueueSend(cmd))
          Serial.println(F("ERR queue full"));
      }
      else if (strcmp(buf, "help") == 0)
      {
        Serial.println(F("Commands:"));
        Serial.println(F("  duty <0..100>        - set duty cycle %"));
        Serial.println(F("  period <10..60000>   - set period in ms"));
        Serial.println(F("  status               - show current settings"));
        Serial.println(F("  help                 - this message"));
      }
      else if (buf[0] != '\0')
      {
        Serial.println(F("ERR (try: help)"));
      }
    }
    else if (len + 1 < sizeof(buf))
    {
      buf[len++] = c;
    }
    else
    {
      len = 0; // overflow -> reset
    }
  }

  tm.vTaskDelayUntil(POLL_MS);
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  while (!Serial)
  {
  }
  Serial.println(F("Blink_DutyCycle_Queue"));
  Serial.println(F("Commands: duty <0..100>, period <10..60000>, status"));

  // Priorities: OFF > ON > control (OFF edge happens promptly)
  hOff = tm.xTaskCreate(ledOffTask, 0, /*prio=*/3);
  hOn = tm.xTaskCreate(ledOnTask, 0, /*prio=*/2);
  hCtl = tm.xTaskCreate(controlTask, 0, /*prio=*/1);

  // OFF starts disabled; ON arms it when needed
  tm.vTaskSuspend(hOff);
}

void loop()
{
  tm.run();
}

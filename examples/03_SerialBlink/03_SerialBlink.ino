#include <Arduino.h>
#include "TaskScheduler.h"

// ===== Config =====
#define NUM_TASKS 3

// default timing (can be changed at runtime via Serial)
static uint16_t g_period_ms   = 1000;  // full cycle
static uint8_t  g_duty_pct    = 50;    // 0..100

// bounds for sanity
static const uint16_t MIN_PERIOD_MS = 10;
static const uint16_t MAX_PERIOD_MS = 60000;

// ===== Scheduler =====
TaskScheduler<NUM_TASKS> tm;

uint8_t hOn   = IScheduler::INVALID_TASK_ID;
uint8_t hOff  = IScheduler::INVALID_TASK_ID;
uint8_t hCtl  = IScheduler::INVALID_TASK_ID;

// ===== Helpers =====
static inline uint16_t clamp_u16(uint16_t v, uint16_t lo, uint16_t hi) {
  return (v < lo) ? lo : (v > hi) ? hi : v;
}
static inline uint8_t clamp_u8(uint8_t v, uint8_t lo, uint8_t hi) {
  return (v < lo) ? lo : (v > hi) ? hi : v;
}

// ===== Tasks =====

// ON pacer: runs once per period, sets LED HIGH if duty>0, arms OFF after on-time
void ledOnTask() {
  // compute on-time from the latest settings
  const uint16_t period = g_period_ms;
  const uint8_t  duty   = g_duty_pct;

  uint32_t on_ms = (uint32_t)period * duty / 100u;  // safe intermediate

  if (duty == 0) {
    // always OFF this cycle
    digitalWrite(LED_BUILTIN, LOW);
  } else if (duty >= 100) {
    // always ON this cycle
    digitalWrite(LED_BUILTIN, HIGH);
  } else {
    // normal PWM-ish cycle
    digitalWrite(LED_BUILTIN, HIGH);
    if (hOff != IScheduler::INVALID_TASK_ID) {
      tm.vTaskResumeAfter(hOff, (uint16_t)on_ms);
    }
  }

  // drift-free period anchored to this task's release
  tm.vTaskDelayUntil(period);
}

// OFF one-shot: disables LED, then auto-suspends (no delay set)
void ledOffTask() {
  digitalWrite(LED_BUILTIN, LOW);
  // no delay -> auto-disabled until next arm from ledOnTask()
}

// Control task: read Serial, parse "duty X" or "period X"
void controlTask() {
  static const uint16_t POLL_MS = 5;
  static char buf[48];
  static uint8_t len = 0;

  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;           // normalize CRLF
    if (c == '\n') {
      buf[len] = '\0';
      len = 0;

      // parse commands
      if (strncmp(buf, "duty ", 5) == 0) {
        long v = strtol(buf + 5, nullptr, 10);
        g_duty_pct = clamp_u8((v < 0) ? 0 : (v > 255 ? 255 : (uint8_t)v), 0, 100);
        Serial.print(F("OK duty="));
        Serial.print(g_duty_pct);
        Serial.println(F("%"));
      } else if (strncmp(buf, "period ", 7) == 0) {
        long v = strtol(buf + 7, nullptr, 10);
        if (v < 0) v = 0;
        g_period_ms = clamp_u16((uint16_t)v, MIN_PERIOD_MS, MAX_PERIOD_MS);
        Serial.print(F("OK period="));
        Serial.print(g_period_ms);
        Serial.println(F(" ms"));
      } else if (strcmp(buf, "status") == 0) {
        Serial.print(F("period="));
        Serial.print(g_period_ms);
        Serial.print(F(" ms, duty="));
        Serial.print(g_duty_pct);
        Serial.println(F("%"));
      } else if (strcmp(buf, "help") == 0) {
        Serial.println(F("Commands:"));
        Serial.println(F("  duty <0..100>"));
        Serial.println(F("  period <10..60000>   (ms)"));
        Serial.println(F("  status"));
      } else if (buf[0] != '\0') {
        Serial.println(F("ERR (try: help)"));
      }
    } else if (len + 1 < sizeof(buf)) {
      buf[len++] = c;
    } else {
      // overflow: reset
      len = 0;
    }
  }

  tm.vTaskDelayUntil(POLL_MS);
}

// ===== Arduino =====
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  while (!Serial) { /* boards with native USB */ }
  Serial.println(F("Blink_DutyCycle_Serial"));
  Serial.println(F("Type: 'duty 25', 'period 2000', 'status', 'help'"));

  // priorities: OFF > ON > control, so OFF edge happens promptly
  hOff = tm.xTaskCreate(ledOffTask,   0, /*prio=*/3);
  hOn  = tm.xTaskCreate(ledOnTask,    0, /*prio=*/2);
  hCtl = tm.xTaskCreate(controlTask,  0, /*prio=*/1);

  // OFF starts disabled; ON will arm it when needed
  tm.vTaskSuspend(hOff);
}

void loop() {
  tm.run();
}

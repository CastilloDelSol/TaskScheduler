# TaskScheduler — Tiny Cooperative Scheduler for Arduino

A lightweight, header-only task scheduler for small microcontrollers.  
Implements simple **RTOS-like primitives** (tasks, priorities, delays, notifications, queues) — but runs entirely cooperatively, without interrupts or stack switching.

---

## ✨ Features
- **Header-only** — just include `TaskScheduler.h`
- **Cooperative** — runs in `loop()`, no ISR or threading
- **Prioritized tasks** (`vTaskPrioritySet`, sorted run order)
- **Delays & periodic execution** (`vTaskDelay`, `vTaskDelayUntil`)
- **Suspend/Resume** support
- **Task notifications** (binary or counting semaphore style)
- **Tiny fixed-size containers** (no dynamic allocation)
- **Wrap-safe timing** using `millis()`
- **Designed for small MCUs (AVR, ESP, etc.)**

---

## 🧩 Example
```cpp
#include <Arduino.h>
#include "TaskScheduler.h"

TaskScheduler<2> tm;

uint8_t hBlink;
uint8_t hMessage;

void blinkTask() {
  static bool on = false;
  digitalWrite(LED_BUILTIN, on);
  on = !on;
  tm.vTaskDelayUntil(500);
}

void messageTask() {
  Serial.println("Hello from TaskScheduler!");
  tm.vTaskDelayUntil(1000);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  hBlink = tm.xTaskCreate(blinkTask, 0, 1);
  hMessage = tm.xTaskCreate(messageTask, 0, 2);
}

void loop() {
  tm.run();
}
```

---

## 🛠️ How It Works
- Each task is a simple `void()` function.
- Tasks **yield** back to the scheduler via delay/yield calls.
- Scheduler runs tasks that are **due**, in **priority order**.
- If a task finishes without rescheduling itself → it suspends automatically.

---

## 📁 Included Headers
| File | Description |
|------|--------------|
| `TaskScheduler.h` | Main cooperative scheduler (template-based) |
| `Task.h` | Task record + metadata |
| `TaskMeta.h` | Compact flags (in-use, enable, priority) |
| `TaskKey.h` | Packed task index/priority key |
| `BoolArray32.h` | Bitmask helper for up to 32 tasks |
| `FixedQueue.h` | Ring buffer FIFO (tiny queue) |
| `FixedVector.h` | Fixed-size container with sort |
| `TaskQueue.h` | Queue with built-in task wakeups |
| `IScheduler.h` | Interface base for queue callbacks |

---

## ⚙️ Configuration
```cpp
TaskScheduler<NUM_TASKS> tm(quantumMs);
```
- `NUM_TASKS`: max number of tasks (≤ 32)
- `quantumMs`: tick quantum in milliseconds (default 1 ms)

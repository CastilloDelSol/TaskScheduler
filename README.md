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

## 🧩 Quick Example
```cpp
#include <Arduino.h>
#include "TaskScheduler.h"

#define NUM_TASKS 2

TaskScheduler<NUM_TASKS> tm;

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

## 🧠 Example Collection

### 1️⃣ `01_Blink.ino`
Minimal example: blink the onboard LED using a scheduled task.

### 2️⃣ `02_ResumeAfterBlink.ino`
Demonstrates `vTaskResumeAfter()` to restart a paused task after a delay.

### 3️⃣ `03_SerialBlink.ino`
Shows running two tasks: one blinking an LED and another printing serial messages with independent timing.

### 4️⃣ `04_SerialBlinkQueue.ino`
Advanced example combining **TaskScheduler** with a **TinyQueue** to pass data between producer and consumer tasks.

---

## 🛠️ How It Works
- Each task is a simple `void()` function.  
- Tasks **cooperatively yield** back to the scheduler using one of:
  - `vTaskDelay(ms)` — schedule next run after *ms*  
  - `vTaskDelayUntil(period)` — maintain a fixed periodic rate relative to the previous release time  
  - `vTaskYieldNextTick()` — yield until the next scheduler tick (cheap throttle)  
- The scheduler runs all tasks that are **due**, in **priority order**.  
- If a task finishes without setting a next wake time → it is **automatically suspended** until resumed manually.  

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

---

## 📜 License
MIT License © 2025 Tim Sonnenburg

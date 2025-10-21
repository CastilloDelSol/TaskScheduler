#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include "IScheduler.h"
#include "Task.h"
#include "FixedVector.h"
#include "TaskKey.h"
#include "BoolArray32.h"

/**
 * @file TaskScheduler.h
 * @brief Tiny cooperative scheduler with RTOS-like APIs: suspend/resume, priorities, and task notifications.
 *
 * Usage:
 *   TaskScheduler<8> sched;     // 1 ms tick quantum by default
 *
 * Notes:
 * - Time base is millis(); wrap-safe comparisons used.
 * - Each task runs at most once per tick (per-tick ran mask).
 * - If a task does not set a next wake, it auto-suspends (safe default).
 */
template <uint8_t N>
class TaskScheduler : public IScheduler {
private:
    // ---- storage ----
    Task t[N];
    FixedVector<TaskKey, N> order; ///< priority-sorted (desc)

    // ---- timing ----
    uint32_t nextDue  = UINT32_MAX; ///< earliest enabled wake
    uint32_t lastTick = 0;          ///< last processed tick (ms)
    uint32_t tickNow  = 0;          ///< scheduler's current tick time (ms)
    uint8_t  tickMs   = 1;          ///< cadence quantum (ms)

    // ---- current task context ----
    uint8_t  currentId;
    uint32_t currentAnchor = 0;     ///< release time of current run
    bool     overrideSet  = false;  ///< task requested a next wake
    uint32_t overrideNext = 0;      ///< requested next wake time

    // ---- same-tick cascade control ----
    bool        cascadePending = false;
    BoolArray32 ranMask;            ///< "ran once this tick" flags

    // ---- direct-to-task notifications (per task) ----
    static constexpr uint8_t MAX_NOTIFY_COUNT = UINT8_MAX;
    uint8_t notifyCnt[N];           ///< pending notify count per task (saturating)

    // ---- helpers ----
    static inline bool earlier(uint32_t a, uint32_t b)
    {
        return (int32_t)(a - b) < 0; // wrap-safe a<b
    }

    inline uint8_t findPosByIdx(uint8_t idx) const
    {
        for (uint8_t k = 0; k < order.size(); ++k)
            if (order[k].idx() == idx) return k;
        return IScheduler::INVALID_TASK_ID;
    }

    inline bool isTaskIdValid(uint8_t id) const { return (id < N) && t[id].meta.inUse(); }
    inline bool hasCurrentTask() const { return currentId != IScheduler::INVALID_TASK_ID; }

public:
    /**
     * @brief Construct a scheduler.
     * @param quantumMs Tick quantum in milliseconds (default 1 ms).
     */
    explicit TaskScheduler(uint8_t quantumMs = 1)
        : tickMs(quantumMs) {
        static_assert(N <= 32, "ranMask uses 32 bits; increase if N>32");
        IScheduler::Instance() = this;
        for (uint8_t i = 0; i < N; ++i) { t[i].markFree(); notifyCnt[i] = 0; }
        lastTick  = millis();           // bind phase to now
        tickNow   = lastTick;
        currentId = IScheduler::INVALID_TASK_ID;
    }

    /**
     * @brief Create a task.
     * @param cb      Task function to call once per dispatch.
     * @param delayMs Initial delay before first run (ms).
     * @param prio    Priority (higher runs first).
     * @return Handle (0..N-1) or INVALID_TASK_ID on failure.
     */
    inline uint8_t xTaskCreate(TaskFunction cb, uint32_t delayMs = 0, uint8_t prio = 3) {
        if (!cb) return IScheduler::INVALID_TASK_ID;
        for (uint8_t i = 0; i < N; ++i) if (!t[i].meta.inUse()) {
            t[i].cb   = cb;
            t[i].next = tickNow + delayMs;   // align to scheduler time
            t[i].meta.clear();
            t[i].meta.setInUse(true);
            t[i].meta.setEnabled(true);
            t[i].meta.setPrio(prio);
            notifyCnt[i] = 0;

            order.push_back(TaskKey(t[i].meta.prio(), i));
            order.sort_descending();

            if (t[i].ready() && earlier(t[i].next, nextDue)) nextDue = t[i].next;
            return i;
        }
        return IScheduler::INVALID_TASK_ID;
    }

    /**
     * @brief Delete a task by handle.
     */
    inline void vTaskDelete(uint8_t h) {
        if (!isTaskIdValid(h)) return;
        t[h].markFree();
        notifyCnt[h] = 0;
        const uint8_t pos = findPosByIdx(h);
        if (pos != IScheduler::INVALID_TASK_ID) order.erase(pos);
    }

    /**
     * @brief Suspend (disable) a task. Use for explicit pausing.
     */
    inline void vTaskSuspend(uint8_t h) {
        if (!isTaskIdValid(h)) return;
        t[h].meta.disable();
    }

    /**
     * @brief Resume a task immediately (same tick).
     */
    inline void vTaskResume(uint8_t h) {
        if (!isTaskIdValid(h)) return;
        t[h].next = tickNow;
        t[h].meta.enable();
        cascadePending = true;
        if (earlier(t[h].next, nextDue)) nextDue = t[h].next;
    }

    /**
     * @brief Resume a task after @p ms from now. If ms==0, runs same tick.
     */
    inline void vTaskResumeAfter(uint8_t h, uint32_t ms) {
        if (!isTaskIdValid(h)) return;
        t[h].next = tickNow + ms;
        t[h].meta.enable();
        if (ms == 0) {
            cascadePending = true;
            if (earlier(t[h].next, nextDue)) nextDue = t[h].next;
        } else if (t[h].ready() && earlier(t[h].next, nextDue)) {
            nextDue = t[h].next;
        }
    }

    /**
     * @brief Increment the notify counter of a task and wake it this tick.
     *        Saturates at 255.
     */
    inline void vTaskNotifyGive(uint8_t h) override {
        if (!isTaskIdValid(h)) return;
        if (notifyCnt[h] != MAX_NOTIFY_COUNT) ++notifyCnt[h];
        t[h].meta.enable();
        t[h].next = tickNow;
        cascadePending = true;
        if (earlier(t[h].next, nextDue)) nextDue = t[h].next;
    }

    /**
     * @brief For the *current* task: take one or all pending notifies.
     * @param clearOnExit true to take all (counting semaphore), false to take one (binary).
     * @return Number taken (0 if none).
     */
    inline uint8_t ulTaskNotifyTake(bool clearOnExit) {
        if (!hasCurrentTask()) return 0;
        uint8_t &c = notifyCnt[currentId];
        if (c == 0) return 0;
        if (clearOnExit) { uint8_t n = c; c = 0; return n; }
        --c; return 1;
    }

    /**
     * @brief Get current task handle during its callback.
     */
    inline uint8_t  xTaskGetCurrentTaskHandle() const override { return currentId; }

    /**
     * @brief Get scheduler tick count (ms).
     */
    inline uint32_t xTaskGetTickCount() const { return tickNow; }

    /**
     * @brief Delay current task by @p ms from now.
     */
    inline void vTaskDelay(uint32_t ms) {
        if (!hasCurrentTask()) return;
        overrideSet  = true;
        overrideNext = tickNow + ms;
    }

    /**
     * @brief Yield current task until next tick (cheap throttle).
     */
    inline void vTaskYieldNextTick() {
        if (!hasCurrentTask()) return;
        overrideSet  = true;
        overrideNext = tickNow; // same-tick re-run prevented by ranMask
    }

    /**
     * @brief Period-locked delay: schedule next wake at the first
     *        future multiple of @p period from this run's release time.
     *        If @p period==0, yields to next tick.
     */
    inline void vTaskDelayUntil(uint32_t period) {
        if (!hasCurrentTask()) return;

        if (period == 0) {
            // yield to next tick
            overrideSet  = true;
            overrideNext = tickNow;
            return;
        }

        const uint32_t anchor = currentAnchor;
        const uint32_t late   = (uint32_t)(tickNow - anchor);

        if (late < period) {
            overrideSet  = true;
            overrideNext = anchor + period;
            return;
        }

        // ONE divide only when genuinely late
        const uint32_t k = (late / period) + 1;     // 32/32 divide
        overrideSet  = true;
        overrideNext = anchor + k * period;
    }

    /**
     * @brief Change a task's priority.
     */
    inline void vTaskPrioritySet(uint8_t h, uint8_t prio) {
        if (!isTaskIdValid(h)) return;
        t[h].meta.setPrio(prio);
        order.clear();
        for (uint8_t i = 0; i < N; ++i)
            if (t[i].meta.inUse()) order.push_back(TaskKey(t[i].meta.prio(), i));
        order.sort_descending();
    }

    /**
     * @brief Get priority of a task by handle.
     */
    inline uint8_t uxTaskPriorityGet(uint8_t h) const {
        return isTaskIdValid(h) ? t[h].meta.prio() : 0;
    }

    /**
     * @brief Get priority of the *current* task.
     */
    inline uint8_t uxTaskPriorityGet() const {
        return hasCurrentTask() ? t[currentId].meta.prio() : 0;
    }

    /**
     * @brief Run one scheduler tick based on current millis().
     */
    inline void run() { run(millis()); }

    /**
     * @brief Run one scheduler tick using a supplied timestamp (ms).
     * @param now Timestamp in ms; must be monotonic modulo 32-bit wrap.
     */
    inline void run(uint32_t now) {
        // cadence gate (wrap-safe with unsigned subtract)
        if ((uint32_t)(now - lastTick) < tickMs) return;
        lastTick += tickMs;
        tickNow   = lastTick;

        // fast path: nothing due yet
        if (earlier(tickNow, nextDue)) return;

        // per-tick init
        uint32_t newNextDue = UINT32_MAX;
        ranMask.clear();
        cascadePending = false;

        // one draining pass + bounded cascades (≤ N) for same-tick wakes
        uint8_t passes = 0;
        do {
            bool anyRanThisPass = false;

            for (uint8_t k = 0; k < order.size(); ++k) {
                const uint8_t idx = order[k].idx();
                Task& x = t[idx];
                if (!x.ready()) continue;

                // once per tick
                if (ranMask.get(idx)) continue;

                // not yet time -> track earliest
                if (earlier(tickNow, x.next)) {
                    if (earlier(x.next, newNextDue)) newNextDue = x.next;
                    continue;
                }

                // due/overdue
                currentId     = idx;
                currentAnchor = x.next;
                overrideSet   = false;

                TaskFunction cb = x.cb;
                cb();  // the only call site

                currentId = IScheduler::INVALID_TASK_ID;
                ranMask.set(idx);
                anyRanThisPass = true;

                if (overrideSet) {
                    x.next = overrideNext;
                    x.meta.enable();
                    if (earlier(x.next, newNextDue)) newNextDue = x.next;
                } else {
                    // no delay set -> auto-suspend (safe default)
                    x.meta.disable();
                }
            }

            // run another pass only if someone requested same-tick wake and at least one ran
            cascadePending = cascadePending && anyRanThisPass;
        } while (cascadePending && (++passes < N));

        nextDue = newNextDue; // may be UINT32_MAX if none enabled
    }
};

#endif // TASK_SCHEDULER_H
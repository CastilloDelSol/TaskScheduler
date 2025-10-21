#ifndef TASK_H
#define TASK_H

#include "TaskMeta.h"

using TaskFunction = void (*)(void);

// ---- task record ----
struct Task {
    uint32_t next;    // absolute next wake (ms)
    TaskFunction cb;      // callback
    TaskMeta meta;    // flags + prio (1 byte)

    Task() : next(0), cb(nullptr) {}

    inline bool ready() const { return meta.inUse() && meta.enabled() && cb; }

    inline void markFree()
    {
        meta.clear();
        cb = nullptr;
    }
};

#endif // TASK_H
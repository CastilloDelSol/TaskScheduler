#ifndef TASK_KEY_H
#define TASK_KEY_H

#include <stdint.h>

// ====== Packed task key (prio:3 | idx:5) ======
struct TaskKey {
private:
    uint8_t v; // [prio:3 | idx:5]

public:
    TaskKey() : v(0) {}
    TaskKey(uint8_t prio, uint8_t idx) { set(prio, idx); }

    inline void set(uint8_t prio, uint8_t idx) { v = (uint8_t)((prio << 5) | (idx & 0x1F)); }
    inline uint8_t prio() const { return (uint8_t)(v >> 5); }
    inline uint8_t idx()  const { return (uint8_t)(v & 0x1F); }

    // comparisons for FixedVector::sort_descending
    inline bool operator<(const TaskKey& other) const { return v < other.v; }
    inline bool operator==(const TaskKey& other) const { return v == other.v; }
};

#endif // TASK_KEY_H
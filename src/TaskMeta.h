#ifndef TASK_META_H
#define TASK_META_H

#include <stdint.h>

// ---- compact metadata wrapper (1 byte) ----
class TaskMeta {
private:
    uint8_t v;
    static constexpr uint8_t INUSE      = 0x01;
    static constexpr uint8_t EN         = 0x02;
    static constexpr uint8_t PRIO_MASK  = 0x1C; // bits 2..4
    static constexpr uint8_t PRIO_SHIFT = 2;

public:
    TaskMeta() : v(0) {}

    // lifecycle
    inline void clear() { v = 0; }
    inline bool empty() const { return v == 0; }

    // in-use
    inline void setInUse(bool on = true)
    {
        if (on) v |= INUSE;
        else v &= ~INUSE;
    }
    inline bool inUse() const { return v & INUSE; }

    // enabled
    inline void enable() { v |= EN; }
    inline void disable() { v &= ~EN;}
    inline bool enabled() const { return v & EN; }

    inline void setEnabled(bool on)
    {
        if (on) enable();
        else disable();
    }

    // priority
    inline void setPrio(uint8_t p) { v = (v & ~PRIO_MASK) | ((p & 7) << PRIO_SHIFT); }
    inline uint8_t prio() const { return (v & PRIO_MASK) >> PRIO_SHIFT; }
};

#endif // TASK_META_H
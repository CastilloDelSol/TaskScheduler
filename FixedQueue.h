#ifndef FIXED_QUEUE_H
#define FIXED_QUEUE_H

#include <stdint.h>

// ====== FixedQueue: tiny ring buffer FIFO (with fail-safe inc) ======
template<typename T, uint8_t CAP>
class FixedQueue {
public:
    FixedQueue() : head(0), tail(0), count(0) {}

    bool     empty()    const { return count == 0; }
    bool     full()     const { return count == CAP; }
    uint8_t  size()     const { return count; }
    uint8_t  capacity() const { return CAP; }

    void clear()
    {
        head = 0;
        tail = 0;
        count = 0;
    }

    bool try_push(const T& v)
    {
        if (full()) return false;
        buf[head] = v;
        head = inc(head);
        ++count;
        return true;
    }

    bool try_pop(T& out)
    {
        if (empty()) return false;
        out = buf[tail];
        tail = inc(tail);
        --count;
        return true;
    }

    bool peek(T& out) const
    {
        if (empty()) return false;
        out = buf[tail];
        return true;
    }

    bool push_overwrite(const T& v)
    {
        if (full())
        {
            buf[head] = v;
            head = inc(head);
            tail = inc(tail);  // drop oldest
            // count stays CAP
            return true;
        }
        return try_push(v);
    }

protected:
    // Efficient wrap-around: mask if CAP is power of two, else modulo
    static uint8_t inc(uint8_t i)
    {
        if ((CAP & (CAP - 1)) == 0)
        {
            // CAP is power of two → bitmask
            return (uint8_t)((i + 1) & (CAP - 1));
        }
        else
        {
            // Fallback for arbitrary CAP
            ++i;
            if (i == CAP) i = 0;
            return i;
        }
    }

    T        buf[CAP];
    uint8_t  head;
    uint8_t  tail;
    uint8_t  count;
};

#endif // FIXED_QUEUE_H
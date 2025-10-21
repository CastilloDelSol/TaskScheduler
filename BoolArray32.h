#ifndef BOOL_ARRAY32_H
#define BOOL_ARRAY32_H

#include <stdint.h>

struct BoolArray32 {
private:
    uint32_t v;

public:
    BoolArray32() : v(0) {}

    inline void clear() { v = 0; }
    inline void set(uint8_t idx) { v |= (1u << idx); }
    inline bool get(uint8_t idx) const { return (v >> idx) & 1u; }
};

#endif // BOOL_ARRAY32_H
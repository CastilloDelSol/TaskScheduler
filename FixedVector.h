#ifndef FIXED_VECTOR_H
#define FIXED_VECTOR_H

#include <stdint.h>

// ====== tiny fixed-capacity vector (with explicit sort) ======
template<typename T, uint8_t CAP>
struct FixedVector {
    T data[CAP];
    uint8_t count = 0;
    static constexpr uint8_t INVALID_INDEX = 0xFF;

    inline uint8_t size() const { return count; }
    inline bool empty() const { return count == 0; }
    inline bool full()  const { return count == CAP; }
    inline T& operator[](uint8_t i) { return data[i]; }
    inline const T& operator[](uint8_t i) const { return data[i]; }
    inline void clear() { count = 0; }

    inline void push_back(const T& v)
    {
        if (count < CAP)
        {
            data[count++] = v;
        }
    }

    inline void erase(uint8_t pos)
    {
        if (pos >= count) return;
        for (uint8_t i = pos + 1; i < count; ++i) data[i - 1] = data[i];
        --count;
    }

    inline uint8_t index_of(const T& v) const
    {
        for (uint8_t i = 0; i < count; ++i) if (data[i] == v) return i;

        return INVALID_INDEX;
    }

    // Insertion sort: descending order
    inline void sort_descending()
    {
        for (uint8_t i = 1; i < count; ++i)
        {
            T key = data[i];
            int8_t j = (int8_t)i - 1;

            while (j >= 0 && data[j] < key)
            {
                data[j + 1] = data[j];
                --j;
            }

            data[j + 1] = key;
        }
    }
};

#endif // FIXED_VECTOR_H
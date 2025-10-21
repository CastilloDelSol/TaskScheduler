#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <stdint.h>
#include "FixedQueue.h"
#include "IScheduler.h"

template<typename T, uint8_t CAP>
class TinyQueue : public FixedQueue<T, CAP> {
private:
    using Base = FixedQueue<T, CAP>;
    uint8_t waiter;

public:
    TinyQueue() : waiter(IScheduler::INVALID_TASK_ID) {}

    // Producer: push + auto-wake waiter (same tick)
    bool xQueueSend(const T& v)
    {
        if (!Base::try_push(v)) return false;

        IScheduler* sch = IScheduler::Instance();
        
        if (sch && waiter != IScheduler::INVALID_TASK_ID)
        {
            sch->vTaskNotifyGive(waiter);
        }

        return true;
    }

    // Consumer: non-blocking by default; if block=true, register as waiter when empty
    bool xQueueReceive(T& out, bool block = false)
    {
        IScheduler* sch = IScheduler::Instance();

        if (Base::try_pop(out))
        {
            if (sch)
            {
                waiter = sch->xTaskGetCurrentTaskHandle(); // keep me as wake target
            }  

            return true;
        }

        if (block && sch)
        {
            waiter = sch->xTaskGetCurrentTaskHandle();           // register as waiter
        }

        return false;
    }
};

#endif // TASK_QUEUE_H
#ifndef ISCHEDULER_H
#define ISCHEDULER_H

#include <stdint.h>

struct IScheduler {
    static constexpr uint8_t INVALID_TASK_ID = UINT8_MAX;

    virtual ~IScheduler() {}

    // pure virtual interface
    virtual void    vTaskNotifyGive(uint8_t h) = 0;
    virtual uint8_t xTaskGetCurrentTaskHandle() const = 0;

    // header-only friendly singleton pointer
    static IScheduler*& Instance() 
    {
        static IScheduler* p = nullptr;
        return p;
    }
};

#endif // ISCHEDULER_H
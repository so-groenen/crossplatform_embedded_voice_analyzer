#ifndef RTOS_H
#define RTOS_H


#include <stdint.h>
#if defined(__cplusplus)
extern "C" {
#endif

typedef void (*task)(void*);
typedef uint32_t handle_t;

void rtos_create_task(task fn, void* arg, handle_t* handle);
void rtos_start();


#if defined(__cplusplus)
}
#endif
#endif //RTOS_H
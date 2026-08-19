#include "rtos/rtos.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#define MAX_TASKS 8
static size_t n_tasks = 0;
static task tasks[MAX_TASKS];
static void* args[MAX_TASKS];
 

void rtos_create_task(task fn, void* arg, handle_t* handle)
{
    tasks[n_tasks] = fn;
    args[n_tasks]  = arg;
    if(handle)
        *handle = n_tasks;
    n_tasks++;
    assert(n_tasks <= MAX_TASKS);
}
void rtos_start()
{
    puts("Starting schedueler");
    for(size_t n = 0; n < n_tasks; n++)
    {
        task new_task = tasks[n];
        new_task(args[n]);
    }
}
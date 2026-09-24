#ifndef QUEUES_HPP
#define QUEUES_HPP

#include "pico/util/queue.h"



template<typename T>
class Queue 
{
    queue_t m_queue{};
public:
    explicit Queue(size_t N)
    {
        queue_init(&m_queue, sizeof(T), N);
    }
    explicit Queue(size_t N, uint spin_lock)
    {
        queue_init_with_spinlock(&m_queue, sizeof(T), N, spin_lock);
    }
    void remove_blocking(T& state)
    {
        queue_remove_blocking(&m_queue,reinterpret_cast<void*>(state));
    }
    void add_blocking(const T& state)
    {
        queue_add_blocking(&m_queue,reinterpret_cast<const void*>(state));
    }
    void peek_blocking(T& state)
    {
        queue_peek_blocking(&m_queue,reinterpret_cast<void*>(state));
    }


    bool is_empty()
    {
        return queue_is_empty(&m_queue);
    }
    bool is_full()
    {
        return queue_is_full(&m_queue);
    }


    inline bool try_add(const T& state)
    {
        return queue_try_add(&m_queue, reinterpret_cast<const void*>(&state));
    }
    inline bool try_remove(T& state)
    {
        return queue_try_remove(&m_queue, reinterpret_cast<void*>(&state));
    }
    bool try_peek(T& state)
    {
        return queue_try_peek(&m_queue, reinterpret_cast<void*>(state));
    }

    ~Queue()
    {
        queue_free(&m_queue);
    }
};


#endif 
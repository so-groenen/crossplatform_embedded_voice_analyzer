#ifndef NVIC_HPP
#define NVIC_HPP
 

#include <array>
#include <cstddef>
 
using InterruptFn = void (*)(void);

inline void default_handler()
{
}
 

template<size_t N_interrupts>
class NVIC 
{
    ;
    std::array<InterruptFn, N_interrupts> m_vectors{};
public:
    NVIC()
    {
        for(auto& fn : m_vectors)
        {
            fn = default_handler;
        }
    }
    void set_vector(size_t n, InterruptFn fn)
    {
        m_vectors[n] = fn;
    }

    void interrupt(size_t n)
    {
        auto isr = m_vectors[n];
        isr(); 
    }
};
  

#endif 
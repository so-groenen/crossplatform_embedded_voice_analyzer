#ifndef EMB_HAL_HEAPLESS_HPP
#define EMB_HAL_HEAPLESS_HPP



#include <cstddef>
#include <utility>


namespace embedded_hal 
{

namespace heapless
{
    template <typename T, size_t Capacity>
    class Vector 
    {
    private:
        std::array<T, Capacity> m_data{};
        size_t m_size{0};

    public:
        // Adds an element to the end of the vector
        bool push_back(const T& value) 
        {
            if (m_size >= Capacity) 
            {
                return false;
            }
            m_data[m_size] = value;
            m_size++;
            return true;
        }

        // Move version of push_back
        bool push_back(T&& value) 
        {
            if (m_size >= Capacity) 
            {
                return false;
            }
            m_data[m_size] = std::move(value);
            m_size++;
            return true;
        }

        // Getters for status
        size_t size() const { return m_size; }
        size_t capacity() const { return Capacity; }
        bool empty() const { return m_size == 0; }

        // Element access
        T& operator[](size_t index) { return m_data[index]; }
        const T& operator[](size_t index) const { return m_data[index]; }
    }; 






        }
}
#endif
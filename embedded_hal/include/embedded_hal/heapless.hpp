#ifndef EMB_HAL_HEAPLESS_HPP
#define EMB_HAL_HEAPLESS_HPP



#include <cstddef>
#include <cstdio>
#include <span>
#include <type_traits>
#include <utility>
#include <concepts>

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
        T* data()
        {
            return m_data.data();
        }
        // Getters for status
        size_t size() const { return m_size; }
        size_t capacity() const { return Capacity; }
        bool empty() const { return m_size == 0; }

        // Element access
        T& operator[](size_t index) { return m_data[index]; }
        const T& operator[](size_t index) const { return m_data[index]; }
    }; 


    template <typename T, size_t Capacity>
    requires std::default_initializable<T>
    class RingBuffer 
    {
    private:
        std::array<T, Capacity> m_data{};
        size_t m_head{};
        size_t m_tail{};
        bool m_full{};
    public:
        explicit RingBuffer();

        bool full() const 
        {
            return m_full;
        }
        bool empty() const 
        {
            return (!m_full && (m_head == m_tail));
        }
        T* data()
        {
            return m_data.data();
        }
        size_t size() const 
        {
            if (m_full)
                return Capacity;
            if (m_head >= m_tail) 
                return m_head - m_tail;
            
            return Capacity + (m_head - m_tail);
        }
        void clear()
        {
            m_head = 0;
            m_tail = 0;
            m_full = false;
        }
        const T* peek() const
        {
            if (empty()) 
                return nullptr;
            
            return &m_data[m_tail];
        }
        T pop() 
        {
            if (empty())
                return T{};
            
            T item = m_data[m_tail];
            m_full = false;
            m_tail = (m_tail + 1) % Capacity;
            return item;
        }
        void push(const T& value) 
        {
            m_data[m_head] = value;
            if (m_full)
            {
                m_tail = (m_tail + 1) % Capacity;
            }
            m_head = (m_head + 1) % Capacity;
            m_full = (m_head == m_tail);
        }
        bool try_push(const T& value) 
        {
            if (m_full)
            {
                return false;
            }
            m_data[m_head] = value;
            m_head = (m_head + 1) % Capacity;
            m_full = (m_head == m_tail);
            return true;
        }
 

        // Element access
        T& operator[](size_t index) { return m_data[index]; }
        const T& operator[](size_t index) const { return m_data[index]; }
    }; 






    template <size_t MAX_CHAR>
    class WriteBuffer
    {
    private:
        std::array<char, MAX_CHAR> m_buffer{};
        size_t m_written{};
    public:
        // Uses variadic templates and forward-arguments to pass cleanly to snprintf
        template <typename... Args>
        bool write(const char* format, Args... args) 
        {
    
            int result = std::snprintf(m_buffer.data(), m_buffer.size(), format, args...);

            if(result < 0)
            {
                return false; 
            }

            m_written = result;
            return true;
        }

        std::span<const char> get() const 
        {
            return std::span{m_buffer}.first(m_written + 1);
        }

        void reset() 
        {
            m_written = 0;
            m_buffer[0] = '\0';
        }
    };






    }
}
#endif
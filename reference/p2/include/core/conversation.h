// reference/p2/include/core/conversation.h
//
// INSTRUCTOR REFERENCE for P2. Used only when you configure with
//   cmake -DUSE_P2_REFERENCE=ON ..
// Otherwise your own include/core/conversation.h is used.

#pragma once
#include "core/message.h"
#include <cstddef>

class Conversation {
public:
    Conversation() noexcept;
    ~Conversation();

    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    void append(Message m);

    std::size_t    size() const noexcept;
    std::size_t    capacity() const noexcept;
    const Message& at(std::size_t i) const;   // throws std::out_of_range

    const Message* begin() const noexcept { return data_; }
    const Message* end()   const noexcept { return data_ + size_; }

private:
    void reallocate(std::size_t new_cap);
    void swap(Conversation& other) noexcept;

    Message*    data_     = nullptr;
    std::size_t size_     = 0;
    std::size_t capacity_ = 0;
};

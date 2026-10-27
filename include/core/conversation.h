// include/core/conversation.h
// A growable array of Message objects. This is the ONLY place in the
// codebase where raw new[] / delete[] are used; std::vector is not allowed.

#pragma once
#include "core/message.h"
#include <cstddef>

class Conversation {
public:
    // Empty conversation: size() == 0, no allocation yet.
    Conversation();

    // Releases all owned Message storage. No effect if already empty
    // (e.g. moved-from).
    ~Conversation();

    // Deep copy: allocates its own buffer and copies every Message.
    // this->begin() != other.begin() afterward (unless both are empty).
    Conversation(const Conversation& other);
    Conversation& operator=(const Conversation& other);

    // Steals other's buffer -- no per-element copying. Afterward, other
    // is valid and empty (data_ == nullptr, size_ == capacity_ == 0).
    Conversation(Conversation&& other) noexcept;
    Conversation& operator=(Conversation&& other) noexcept;

    // Appends m, growing the backing array if needed. Amortized O(1):
    // capacity doubles on every reallocation (see docs/design-log-p2.md).
    void append(Message m);

    // Number of messages currently stored.
    std::size_t size() const noexcept;

    // Bounds-checked access. Throws std::out_of_range if i >= size().
    const Message& at(std::size_t i) const;

    // Range-for iteration, oldest message first. begin() == end() when
    // size() == 0 (both nullptr if nothing has been allocated yet).
    const Message* begin() const noexcept;
    const Message* end()   const noexcept;

private:
    Message*    data_     = nullptr;
    std::size_t size_     = 0;
    std::size_t capacity_ = 0;
};

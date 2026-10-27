// src/conversation.cpp
// Growable array of Message. Only file in the project that uses raw
// new[] / delete[].

#include "core/conversation.h"
#include <stdexcept>
#include <string>
#include <utility>

Conversation::Conversation() = default;

Conversation::~Conversation() {
    delete[] data_;          // delete[] nullptr is a no-op, so a moved-from
    data_ = nullptr;         // object is safe to destroy.
}

// ---- copy ---------------------------------------------------------------

Conversation::Conversation(const Conversation& other) {
    if (other.size_ == 0) return;   // an empty copy owns nothing

    // Allocate exactly size_ slots: a copy does not inherit slack capacity.
    Message* fresh = new Message[other.size_];
    try {
        for (std::size_t i = 0; i < other.size_; ++i) {
            fresh[i] = other.data_[i];   // std::string copy may throw
        }
    } catch (...) {
        delete[] fresh;
        throw;
    }
    data_     = fresh;
    size_     = other.size_;
    capacity_ = other.size_;
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this == &other) return *this;
    // Copy-and-swap: build the copy first (may throw, leaving *this
    // untouched), then steal it. The temporary's destructor frees our old
    // buffer.
    Conversation tmp(other);
    *this = std::move(tmp);
    return *this;
}

// ---- move ---------------------------------------------------------------

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_     = nullptr;
    other.size_     = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this == &other) return *this;
    delete[] data_;
    data_     = other.data_;
    size_     = other.size_;
    capacity_ = other.capacity_;
    other.data_     = nullptr;
    other.size_     = 0;
    other.capacity_ = 0;
    return *this;
}

// ---- append / growth ----------------------------------------------------

void Conversation::append(Message m) {
    if (size_ == capacity_) {
        // Growth factor 2: 0 -> 4 -> 8 -> 16 -> ... (see design log).
        const std::size_t new_capacity = capacity_ == 0 ? 4 : capacity_ * 2;
        Message* fresh = new Message[new_capacity];
        // Message's move assignment is noexcept (it moves a std::string),
        // so this loop cannot throw and the old buffer stays intact until
        // every element has been moved over.
        for (std::size_t i = 0; i < size_; ++i) {
            fresh[i] = std::move(data_[i]);
        }
        delete[] data_;
        data_     = fresh;
        capacity_ = new_capacity;
    }
    data_[size_] = std::move(m);
    ++size_;
}

// ---- access -------------------------------------------------------------

std::size_t Conversation::size() const noexcept { return size_; }

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) {
        throw std::out_of_range("Conversation::at: index " + std::to_string(i) +
                                " >= size " + std::to_string(size_));
    }
    return data_[i];
}

const Message* Conversation::begin() const noexcept { return data_; }
const Message* Conversation::end()   const noexcept { return data_ + size_; }

// reference/p2/src/conversation.cpp — INSTRUCTOR REFERENCE for P2.
//
// Growable array, growth factor 2, first allocation of 2 slots.

#include "core/conversation.h"

#include <stdexcept>
#include <utility>

Conversation::Conversation() noexcept = default;

Conversation::~Conversation() { delete[] data_; }

Conversation::Conversation(const Conversation& other)
    : data_(other.capacity_ ? new Message[other.capacity_] : nullptr),
      size_(0),
      capacity_(other.capacity_) {
    try {
        for (std::size_t i = 0; i < other.size_; ++i) data_[i] = other.data_[i];
    } catch (...) {
        delete[] data_;            // a Message copy threw: don't leak the buffer
        throw;
    }
    size_ = other.size_;
}

Conversation& Conversation::operator=(const Conversation& other) {
    if (this != &other) {
        Conversation tmp(other);   // copy-and-swap: strong exception guarantee
        swap(tmp);
    }
    return *this;
}

Conversation::Conversation(Conversation&& other) noexcept
    : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
    other.data_ = nullptr;
    other.size_ = 0;
    other.capacity_ = 0;
}

Conversation& Conversation::operator=(Conversation&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_ = other.data_;
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }
    return *this;
}

void Conversation::swap(Conversation& other) noexcept {
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
}

void Conversation::reallocate(std::size_t new_cap) {
    Message* fresh = new Message[new_cap];
    for (std::size_t i = 0; i < size_; ++i) fresh[i] = std::move(data_[i]);
    delete[] data_;
    data_ = fresh;
    capacity_ = new_cap;
}

void Conversation::append(Message m) {
    if (size_ == capacity_) reallocate(capacity_ == 0 ? 2 : capacity_ * 2);
    data_[size_++] = std::move(m);
}

std::size_t Conversation::size() const noexcept { return size_; }
std::size_t Conversation::capacity() const noexcept { return capacity_; }

const Message& Conversation::at(std::size_t i) const {
    if (i >= size_) throw std::out_of_range("Conversation::at: index out of range");
    return data_[i];
}

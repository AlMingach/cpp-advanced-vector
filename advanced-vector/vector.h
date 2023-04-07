#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept
    {
        this->Swap(other);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            this->Swap(rhs);
        }
        return *this;
    }
    
    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.Size())
    {
        size_t new_size = other.Size();
        std::uninitialized_copy_n(other.data_.GetAddress(), new_size, data_.GetAddress());
        size_ = new_size;
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(other.size_)
    {
        other.size_ = 0;
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                size_t copy_count = std::min(rhs.size_, size_);
                size_t i = 0;
                for (; i < copy_count; ++i) {
                    data_[i] = rhs.data_[i];
                }

                if (size_ >= rhs.size_) {
                    std::destroy_n(data_.GetAddress() + i, size_ - rhs.size_);
                    size_ = rhs.size_;
                }

                else {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + i, rhs.size_ - size_, data_.GetAddress() + i);
                    size_ = rhs.size_;
                }
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            this->Swap(rhs);
        }
        return *this;
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
        size_ = 0;
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    void Resize(size_t new_size) {
        if (new_size > size_) {
            if (new_size > data_.Capacity()) {
                this->Reserve(new_size);
            }
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_ - 1];
    }

    template <typename Value>
    void PushBack(Value&& value) {
        this->EmplaceBack(std::forward<Value>(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (pos == end()) {
            return &this->EmplaceBack(std::forward<Args>(args)...);
        }
        size_t position = static_cast<size_t>(pos - begin());
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + position) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {

                try {
                    std::uninitialized_move_n(data_.GetAddress(), position, new_data.GetAddress());
                }
                catch (...) {
                    std::destroy_at(new_data.GetAddress() + position);
                    throw;
                }

                try {
                    std::uninitialized_move_n(data_.GetAddress() + position, size_ - position, new_data.GetAddress() + position + 1);
                }
                catch (...) {
                    std::destroy_n(new_data.GetAddress(), position + 1);
                    throw;
                }

            }
            else {

                try {
                    std::uninitialized_copy_n(data_.GetAddress(), position, new_data.GetAddress());
                }
                catch (...) {
                    std::destroy_at(new_data.GetAddress() + position);
                    throw;
                }

                try {
                    std::uninitialized_copy_n(data_.GetAddress() + position, size_ - position, new_data.GetAddress() + position + 1);
                }
                catch (...) {
                    std::destroy_n(new_data.GetAddress(), position + 1);
                    throw;
                }

            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            T tmp(T(std::forward<Args>(args)...));
            new (end()) T(std::move(data_[size_ - 1]));
            std::move_backward(data_.GetAddress() + position, end() - 1, end());
            data_[position] = std::move(tmp);
        }
        ++size_;
        return begin() + position;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return this->Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return this->Emplace(pos, std::move(value));
    }

    void PopBack() /* noexcept */ {
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        size_t position = static_cast<size_t>(pos - begin());
        std::move(data_.GetAddress() + position + 1, end(), data_.GetAddress() + position);
        std::destroy_at(end() - 1);
        --size_;
        return begin() + position;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    const T& operator[](size_t index) const noexcept {
        return data_[index];
    }

    T& operator[](size_t index) noexcept {
        return data_[index];
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
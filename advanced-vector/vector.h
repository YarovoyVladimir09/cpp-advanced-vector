#pragma once

#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>
#include <iterator>

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
    RawMemory(RawMemory&& other) noexcept  {
        Swap(other);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        Swap(rhs);
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
            : data_(other.size_)
            , size_(other.size_)  //
    {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(other.data_.GetAddress(), size_, data_.GetAddress());
        } else {
            std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
        }
    }
    Vector(Vector&& other)
            : data_(std::move(other.data_))
            , size_(std::move(other.size_)){
        other.size_=0;
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept{
        return data_.GetAddress();
    }
    iterator end() noexcept{
        return data_.GetAddress()+size_;
    }
    const_iterator begin() const noexcept{
        return data_.GetAddress();
    }
    const_iterator end() const noexcept{
        return data_.GetAddress()+size_;
    }
    const_iterator cbegin() const noexcept{
        return begin();
    }
    const_iterator cend() const noexcept{
        return end();
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args){
        if(size_ == 0){
            EmplaceBack(std::move(args)...);
            return end()-1;
        }
        auto iter_distance = std::distance( cbegin(), pos);
        if(size_ == Capacity()){

            RawMemory<T> new_data( size_ == 0 ? 1 : size_ * 2);
            // Конструируем элементы в new_data, копируя их из data_
            new (new_data.GetAddress() + iter_distance) T (std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {

                std::uninitialized_move_n(data_.GetAddress(), iter_distance,
                                          new_data.GetAddress());
                std::uninitialized_move_n(data_.GetAddress() + iter_distance,
                                          size_ - iter_distance, new_data.GetAddress() + iter_distance + 1);
            } else {
               // new (new_data.GetAddress() + iter_distance) T (args...);
                std::uninitialized_copy_n(data_.GetAddress(), iter_distance,
                                          new_data.GetAddress());
                std::uninitialized_copy_n(data_.GetAddress() + iter_distance,
                                          size_ - iter_distance, new_data.GetAddress() + iter_distance + 1);
            }
            // Разрушаем элементы в data_
            std::destroy_n(data_.GetAddress(), size_);
            // Избавляемся от старой сырой памяти, обменивая её на новую
            data_.Swap(new_data);
            ++size_;
            return data_.GetAddress() + iter_distance;
        } else{
            T buffer(std::forward<Args>(args)...);
            new (data_.GetAddress() + size_) T (std::move(*(end()-1)));
            std::move_backward(&data_[iter_distance],end()-1, end());
            data_[iter_distance] = std::move(buffer);
            ++size_;
            return &data_[iter_distance];
        }
    }
    iterator Erase(const_iterator pos){
        auto iter_distance = std::distance( cbegin(), pos);
        std::move(&data_[iter_distance+1],end(),&data_[iter_distance]);
        std::destroy_n(end()-1,1);
        --size_;
        return const_cast<iterator>(pos);
    }
    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos, std::move(value));
    }

    Vector& operator=(const Vector& rhs){
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                if(rhs.size_ >= size_){
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_,
                         data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_,
                                              data_.GetAddress() + size_);

                }else  {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_,
                         data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_ , size_ - rhs.size_ );
                }
            }
            size_ = rhs.size_;
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept{
        Swap(rhs);
        return *this;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        // Конструируем элементы в new_data, копируя их из data_
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        // Разрушаем элементы в data_
        std::destroy_n(data_.GetAddress(), size_);
        // Избавляемся от старой сырой памяти, обменивая её на новую
        data_.Swap(new_data);
    }

    void Swap(Vector& other) noexcept{
        data_.Swap(other.data_);
        std::swap(size_,other.size_);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    void Resize(size_t new_size){
        if(new_size > size_){
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);

        } else if(new_size < size_){
            std::destroy_n(data_.GetAddress() + new_size , size_ - new_size);
        } else {
            return;
        }
        size_ = new_size;
    }

    void PushBack(const T& value){
        if (size_ == Capacity()) {
            RawMemory<T> new_data( size_ == 0 ? 1 : size_ * 2);
            // Конструируем элементы в new_data, копируя их из data_
            new (new_data.GetAddress() + size_) T (value);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            // Разрушаем элементы в data_
            std::destroy_n(data_.GetAddress(), size_);
            // Избавляемся от старой сырой памяти, обменивая её на новую
            data_.Swap(new_data);
        } else {
            //data_[size_] = value;
            new (data_.GetAddress() + size_) T (value);
        }
        ++size_;
    }

    void PushBack(T&& value){
        if (size_ == Capacity()) {
            RawMemory<T> new_data( size_ == 0 ? 1 : size_ * 2);
            // Конструируем элементы в new_data, копируя их из data_
            new (new_data.GetAddress() + size_) T (std::move(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            // Разрушаем элементы в data_
            std::destroy_n(data_.GetAddress(), size_);
            // Избавляемся от старой сырой памяти, обменивая её на новую
            data_.Swap(new_data);
        } else {
            new (data_.GetAddress() + size_) T (std::move(value));
        }
        ++size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args){
        if (size_ == Capacity()) {
            RawMemory<T> new_data( size_ == 0 ? 1 : size_ * 2);
            // Конструируем элементы в new_data, копируя их из data_
            new (new_data.GetAddress() + size_) T (std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            // Разрушаем элементы в data_
            std::destroy_n(data_.GetAddress(), size_);
            // Избавляемся от старой сырой памяти, обменивая её на новую
            data_.Swap(new_data);
        } else {
            new (data_.GetAddress() + size_) T (std::forward<Args>(args)...);
        }
        ++size_;

        return data_[size_-1];
    }

    void PopBack() {
        std::destroy_n(data_.GetAddress() + size_ - 1, 1);
        --size_;
    }



private:
    RawMemory<T> data_;
    size_t size_ = 0;

    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }
};
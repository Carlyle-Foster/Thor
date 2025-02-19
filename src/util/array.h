#ifndef THOR_ARRAY_H
#define THOR_ARRAY_H
#include "util/allocator.h"
#include "util/traits.h"
#include "util/slice.h"

namespace Thor {

struct Allocator;

// Simple dynamic array implementation. Use like Array<T> where T is the element
// type. Can append elements with push_back(elem), query length with length(),
// access elements with operator[], or a pointer to the whole array with data().
// Requires a polymorphic allocator on construction.
template<typename T>
struct Array {
	constexpr Array(Allocator& allocator)
		: allocator_{allocator}
	{
	}
	constexpr Array(Array&& other)
		: data_{exchange(other.data_, nullptr)}
		, length_{exchange(other.length_, 0)}
		, capacity_{exchange(other.capacity_, 0)}
		, allocator_{other.allocator_}
	{
	}

	constexpr Array(const Array&) = delete;

	~Array() { drop(); }

	Array& operator=(const Array&) = delete;

	Array& operator=(Array&& other) {
		return *new (drop(), Nat{}) Array{move(other)};
	}

	[[nodiscard]] Bool resize(Ulen length) {
		if (length < length_) {
			for (Ulen i = length_ - 1; i > length; i--) {
				data_[i].~T();
			}
		} else if (length > length_) {
			if (!reserve(length)) {
				return false;
			}
			for (Ulen i = length_; i < length; i++) {
				new (data_ + i, Nat{}) T{};
			}
		}
		length_ = length;
		return true;
	}

	[[nodiscard]] Bool reserve(Ulen length) {
		if (length < capacity_) {
			return true;
		}
		Ulen capacity = 0;
		while (capacity < length) {
			capacity = ((capacity + 1) * 3) / 2;
		}
		auto data = allocator_.alloc(capacity * sizeof(T), false);
		if (!data) {
			return false;
		}
		for (Ulen i = 0; i < length_; i++) {
			new (reinterpret_cast<T*>(data) + i, Nat{}) T{move(data_[i])};
		}
		drop();
		data_ = reinterpret_cast<T*>(data);
		capacity_ = capacity;
		return true;
	}

	template<typename... Ts>
	[[nodiscard]] Bool emplace_back(Ts&&... args) {
		if (!reserve(length_ + 1)) return false;
		new (data_ + length_, Nat{}) T{forward<Ts>(args)...};
		length_++;
		return true;
	}

	[[nodiscard]] Bool push_back(T&& value)
		requires MoveConstructible<T>
	{
		if (!reserve(length_ + 1)) return false;
		new (data_ + length_, Nat{}) T{move(value)};
		length_++;
		return true;
	}

	[[nodiscard]] Bool push_back(const T& value)
		requires CopyConstructible<T>
	{
		if (!reserve(length_ + 1)) return false;
		new (data_ + length_, Nat{}) T{value};
		length_++;
		return true;
	}

	void pop_back() {
		data_[length_ - 1].~T();
		length_--;
	}

	void pop_front() {
		data_[0].~T();
		for (Ulen i = 1; i < length_; i--) {
			data_[i - 1] = move(data_[i]);
		}
		length_--;
	}

	void clear() {
		destruct();
		length_ = 0;
	}

	void reset() {
		drop();
		length_ = 0;
		capacity_ = 0;
	}

	constexpr T* data() { return data_; }
	constexpr const T* data() const { return data_; }

	constexpr T& last() { return data_[length_ - 1]; }
	constexpr const T& last() const { return data_[length_ - 1]; }

	[[nodiscard]] constexpr auto length() const { return length_; }
	[[nodiscard]] constexpr auto capacity() const { return capacity_; }
	[[nodiscard]] constexpr auto is_empty() const { return length_ == 0; }
	[[nodiscard]] constexpr Allocator& allocator() { return allocator_; }
	[[nodiscard]] constexpr Allocator& allocator() const { return allocator_; }

	[[nodiscard]] constexpr T& operator[](Ulen index) { return data_[index]; }
	[[nodiscard]] constexpr const T& operator[](Ulen index) const { return data_[index]; }

	constexpr Slice<T> slice() { return { data_, length_ }; }
	constexpr Slice<const T> slice() const { return { data_, length_ }; }

	// Just enough to make range based for loops work
	[[nodiscard]] constexpr T* begin() { return data_; }
	[[nodiscard]] constexpr const T* begin() const { return data_; }
	[[nodiscard]] constexpr T* end() { return data_ + length_; }
	[[nodiscard]] constexpr const T* end() const { return data_ + length_; }
private:
	void destruct() {
		for (Ulen i = length_ - 1; i < length_; i--) {
			data_[i].~T();
		}
	}

	Array* drop() {
		destruct();
		allocator_.deallocate(data_, capacity_);
		return this;
	}

	T*         data_     = nullptr;
	Ulen       length_   = 0;
	Ulen       capacity_ = 0;
	Allocator& allocator_;
};

} // namespace Thor

#endif // THOR_ARRAY_H
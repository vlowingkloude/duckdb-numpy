//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/common/types/string_type.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/assert.hpp"
#include "duckdb/common/constants.hpp"
#include "duckdb/common/helper.hpp"

#include <cstring>

namespace duckdb {

struct string_t {
	friend struct StringComparisonOperators;
	friend class StringSegment;

public:
	static constexpr idx_t PREFIX_BYTES = 4 * sizeof(char);
	static constexpr idx_t INLINE_BYTES = 12 * sizeof(char);
	static constexpr idx_t HEADER_SIZE = sizeof(uint32_t) + PREFIX_BYTES;
#ifndef DUCKDB_DEBUG_NO_INLINE
	static constexpr idx_t PREFIX_LENGTH = PREFIX_BYTES;
	static constexpr idx_t INLINE_LENGTH = INLINE_BYTES;
#else
	static constexpr idx_t PREFIX_LENGTH = 0;
	static constexpr idx_t INLINE_LENGTH = 0;
#endif

	string_t() = default;
	explicit string_t(uint32_t len) {
		value.inlined.length = len;
	}
	string_t(const char *data, uint32_t len) {
		value.inlined.length = len;
		D_ASSERT(data || GetSize() == 0);
		if (IsInlined()) {
			// zero initialize the prefix first
			// this makes sure that strings with length smaller than 4 still have an equal prefix
			memset(value.inlined.inlined, 0, INLINE_BYTES);
			if (GetSize() == 0) {
				return;
			}
			// small string: inlined
			memcpy(value.inlined.inlined, data, GetSize());
		} else {
			// large string: store pointer
#ifndef DUCKDB_DEBUG_NO_INLINE
			memcpy(value.pointer.prefix, data, PREFIX_LENGTH);
#else
			memset(value.pointer.prefix, 0, PREFIX_BYTES);
#endif
			value.pointer.ptr = (char *)data;
		}
	}
	string_t(const char *data) : string_t(data, strlen(data)) { // NOLINT: Allow implicit conversion from `const char*`
	}
	string_t(const string &value)
	    : string_t(value.c_str(), value.size()) { // NOLINT: Allow implicit conversion from `const char*`
	}

	bool IsInlined() const {
		return GetSize() <= INLINE_LENGTH;
	}

	//! this is unsafe since the string will not be terminated at the end
	const char *GetDataUnsafe() const {
		return IsInlined() ? (const char *)value.inlined.inlined : value.pointer.ptr;
	}

	char *GetDataWriteable() const {
		return IsInlined() ? (char *)value.inlined.inlined : value.pointer.ptr;
	}

	const char *GetPrefix() const {
		return value.pointer.prefix;
	}

	idx_t GetSize() const {
		return value.inlined.length;
	}

	string GetString() const {
		return string(GetDataUnsafe(), GetSize());
	}

	explicit operator string() const {
		return GetString();
	}

	void SetPointer(char *new_ptr) {
		D_ASSERT(!IsInlined());
		value.pointer.ptr = new_ptr;
	}

	void Finalize() {
		// set trailing NULL byte
		if (GetSize() <= INLINE_LENGTH) {
			// fill prefix with zeros if the length is smaller than the prefix length
			for (idx_t i = GetSize(); i < INLINE_BYTES; i++) {
				value.inlined.inlined[i] = '\0';
			}
		} else {
			// copy the data into the prefix
#ifndef DUCKDB_DEBUG_NO_INLINE
			auto dataptr = (char *)GetDataUnsafe();
			memcpy(value.pointer.prefix, dataptr, PREFIX_LENGTH);
#else
			memset(value.pointer.prefix, 0, PREFIX_BYTES);
#endif
		}
	}

	void Verify() const;
	void VerifyNull() const;

	struct StringComparisonOperators {
		static inline bool Equals(const string_t &a, const string_t &b) {
#ifdef DUCKDB_DEBUG_NO_INLINE
			if (a.GetSize() != b.GetSize())
				return false;
			return (memcmp(a.GetDataUnsafe(), b.GetDataUnsafe(), a.GetSize()) == 0);
#endif
			uint64_t A = Load<uint64_t>((const_data_ptr_t)&a);
			uint64_t B = Load<uint64_t>((const_data_ptr_t)&b);
			if (A != B) {
				// Either length or prefix are different -> not equal
				return false;
			}
			// they have the same length and same prefix!
			A = Load<uint64_t>((const_data_ptr_t)&a + 8u);
			B = Load<uint64_t>((const_data_ptr_t)&b + 8u);
			if (A == B) {
				// either they are both inlined (so compare equal) or point to the same string (so compare equal)
				return true;
			}
			if (!a.IsInlined()) {
				// 'long' strings of the same length -> compare pointed value
				if (memcmp(a.value.pointer.ptr, b.value.pointer.ptr, a.GetSize()) == 0) {
					return true;
				}
			}
			// either they are short string of same length but different content
			//     or they point to string with different content
			//     either way, they can't represent the same underlying string
			return false;
		}
		// compare up to shared length. if still the same, compare lengths
		static bool GreaterThan(const string_t &left, const string_t &right) {
			const uint32_t left_length = left.GetSize();
			const uint32_t right_length = right.GetSize();
			const uint32_t min_length = std::min<uint32_t>(left_length, right_length);

#ifndef DUCKDB_DEBUG_NO_INLINE
			uint32_t A = Load<uint32_t>((const_data_ptr_t)left.GetPrefix());
			uint32_t B = Load<uint32_t>((const_data_ptr_t)right.GetPrefix());

			// Utility to move 0xa1b2c3d4 into 0xd4c3b2a1, basically inverting the order byte-a-byte
			auto bswap = [](uint32_t v) -> uint32_t {
				uint32_t t1 = (v >> 16u) | (v << 16u);
				uint32_t t2 = t1 & 0x00ff00ff;
				uint32_t t3 = t1 & 0xff00ff00;
				return (t2 << 8u) | (t3 >> 8u);
			};

			// Check on prefix -----
			// We dont' need to mask since:
			//	if the prefix is greater(after bswap), it will stay greater regardless of the extra bytes
			// 	if the prefix is smaller(after bswap), it will stay smaller regardless of the extra bytes
			//	if the prefix is equal, the extra bytes are guaranteed to be /0 for the shorter one

			if (A != B)
				return bswap(A) > bswap(B);
#endif
			auto memcmp_res = memcmp(left.GetDataUnsafe(), right.GetDataUnsafe(), min_length);
			return memcmp_res > 0 || (memcmp_res == 0 && left_length > right_length);
		}
	};

	bool operator==(const string_t &r) const {
		return StringComparisonOperators::Equals(*this, r);
	}

	bool operator>(const string_t &r) const {
		return StringComparisonOperators::GreaterThan(*this, r);
	}
	bool operator<(const string_t &r) const {
		return r > *this;
	}

private:
	union {
		struct {
			uint32_t length;
			char prefix[4];
			char *ptr;
		} pointer;
		struct {
			uint32_t length;
			char inlined[12];
		} inlined;
	} value;
};

} // namespace duckdb

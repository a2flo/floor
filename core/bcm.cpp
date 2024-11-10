// BCM (v1.51) - A BWT-based file compressor
//
// Written and placed in the public domain by Ilya Muravyov
// C++ modernization and improvements by Florian Ziesche
//
// Also uses sais-lite (v2.4.1) by Yuta Mori (license further down below).

#include "bcm.hpp"
#include "logger.hpp"
#include <cassert>
#include <iterator>
#include <limits>
#include <array>

namespace bcm {
namespace saisxx_private { // via sais-lite
static uint32_t saisxx_bwt(const std::span<const uint8_t>& input, std::span<uint8_t> U, std::span<int> A);
} // namespace saisxx_private

template <uint32_t rate> struct counter_t {
	uint16_t p { 1u << 15u /* 0.5 */ };

	constexpr void update(const uint16_t mask) {
		const auto p_xor_shift = (p ^ mask) >> rate;
		p += (mask ? p_xor_shift : -p_xor_shift);
	}
};

static constexpr auto bcm_init_counter_2(auto& dst) {
	static constexpr const auto counter_2_init = []() constexpr {
		std::array<std::array<std::array<counter_t<6>, 17>, 256>, 2> arr;
		for (uint32_t i = 0u; i < 2u; ++i) {
			for (uint32_t j = 0u; j < 256u; ++j) {
				for (uint32_t k = 0u; k <= 16u; ++k) {
					arr[i][j][k].p = uint16_t((k << 12u) - (k == 16u));
				}
			}
		}
		return arr;
	}();
	__builtin_memcpy(&dst, counter_2_init.data(), sizeof(dst));
}

struct bcm_encoder {
	uint32_t low { 0u };
	uint32_t high { ~0u };
	uint32_t code { 0u };
	uint32_t run { 0u };
	uint32_t c1 { 0u };
	uint32_t c2 { 0u };
	std::span<uint8_t> output;
	counter_t<2> counter_0[256];
	counter_t<4> counter_1[256][256];
	counter_t<6> counter_2[2][256][17];

	explicit bcm_encoder(std::span<uint8_t> output_) : output(output_) {
		bcm_init_counter_2(counter_2);
	}

	void flush() {
#pragma unroll
		for (uint32_t i = 0u; i < 4u; ++i) {
			output[i] = uint8_t(low >> 24u);
			low <<= 8u;
		}
		output = output.subspan(4);
	}

	template <uint32_t P_LOG> void encode_bit(const uint32_t mask, const uint32_t p, uint32_t& out_idx) {
		const auto mid = uint32_t(low + ((uint64_t(high - low) * p) >> P_LOG));
		if (mask) {
			high = mid;
		} else {
			low = mid + 1;
		}

		// renormalize
		while ((low ^ high) < (1u << 24u)) {
			output[out_idx++] = uint8_t(low >> 24u);
			low <<= 8u;
			high = (high << 8u) + 255u;
		}
	}

	void put32(const uint32_t x) {
		uint32_t out_idx = 0;
		for (uint32_t i = 1u << 31u; i > 0u; i >>= 1u) {
			encode_bit<1>(x & i, 1, out_idx); // p=0.5
		}
		output = output.subspan(out_idx);
	}

	void put(int c) {
		const auto f = (run > 2u ? 1u : 0u);
		auto& counter_1_c1_ref = counter_1[c1];
		auto& counter_1_c2_ref = counter_1[c2];
		auto& counter_2_ref = counter_2[f];

		uint32_t ctx = 1;
		uint32_t out_idx = 0;
#pragma clang loop unroll_count(8)
		for (int i = 128; i > 0; i >>= 1) {
			const auto p0 = int32_t(counter_0[ctx].p);
			const auto p1 = int32_t(counter_1_c1_ref[ctx].p);
			const auto p2 = int32_t(counter_1_c2_ref[ctx].p);
			const auto p = (((p0 + p1) * 7) + p2 + p2) >> 4;

			// SSE with linear interpolation
			const auto j = uint32_t(p >> 12);
			const auto x1 = int32_t(counter_2_ref[ctx][j].p);
			const auto x2 = int32_t(counter_2_ref[ctx][j + 1].p);
			const auto ssep = x1 + (((x2 - x1) * (p & 4095)) >> 12);
			const auto encode_p = uint32_t(p + ssep + ssep + ssep);

			const auto bit_set = ((c & i) != 0);
			const auto mask = (bit_set ? uint16_t(0xFFFFu) : uint16_t(0u));
			encode_bit<18>(mask, encode_p, out_idx);
			counter_0[ctx].update(mask);
			counter_1_c1_ref[ctx].update(mask);
			counter_2_ref[ctx][j].update(mask);
			counter_2_ref[ctx][j + 1].update(mask);
			ctx += ctx + (bit_set ? 1u : 0u);
		}
		output = output.subspan(out_idx);

		c2 = c1;
		c1 = ctx - 256u;
		run = (c1 == c2 ? run + 1u : 0u);
	}
};

struct bcm_decoder {
	uint32_t low { 0u };
	uint32_t high { ~0u };
	uint32_t code { 0u };
	uint32_t run { 0u };
	uint32_t c1 { 0u };
	uint32_t c2 { 0u };
	std::span<const uint8_t> input;
	counter_t<2> counter_0[256];
	counter_t<4> counter_1[256][256];
	counter_t<6> counter_2[2][256][17];

	explicit bcm_decoder(std::span<const uint8_t> input_) : input(input_) {
		bcm_init_counter_2(counter_2);
#pragma unroll
		for (uint32_t i = 0; i < 4; ++i) {
			code = (code << 8u) + input[i];
		}
		input = input.subspan(4);
	}

	template <uint32_t P_LOG> uint32_t decode_bit(const uint32_t p, uint32_t& in_idx) {
		const auto mid = uint32_t(low + ((uint64_t(high - low) * p) >> P_LOG));
		const auto bit = (code <= mid ? 1u : 0u);
		if (bit) {
			high = mid;
		} else {
			low = mid + 1;
		}

		// renormalize
		while ((low ^ high) < (1u << 24u)) {
			low <<= 8u;
			high = (high << 8u) + 255u;
			code = (code << 8u) + input[in_idx++];
		}
		return bit;
	}

	uint32_t get32() {
		uint32_t x = 0, in_idx = 0;
		for (uint32_t i = 0; i < 32; ++i) {
			x += x + decode_bit<1>(1, in_idx); // p=0.5
		}
		input = input.subspan(in_idx);
		return x;
	}

	uint8_t get() {
		const auto f = (run > 2u ? 1u : 0u);
		auto& counter_1_c1_ref = counter_1[c1];
		auto& counter_1_c2_ref = counter_1[c2];
		auto& counter_2_ref = counter_2[f];

		uint32_t ctx = 1;
		uint32_t in_idx = 0;
		while (ctx < 256) {
			const auto p0 = int32_t(counter_0[ctx].p);
			const auto p1 = int32_t(counter_1_c1_ref[ctx].p);
			const auto p2 = int32_t(counter_1_c2_ref[ctx].p);
			const auto p = (((p0 + p1) * 7) + p2 + p2) >> 4;

			// SSE with linear interpolation
			const auto j = uint32_t(p >> 12);
			const auto x1 = int32_t(counter_2_ref[ctx][j].p);
			const auto x2 = int32_t(counter_2_ref[ctx][j + 1].p);
			const auto ssep = x1 + (((x2 - x1) * (p & 4095)) >> 12);

			const auto bit_set = decode_bit<18>(uint32_t(p + ssep + ssep + ssep), in_idx);
			const auto mask = (bit_set ? uint16_t(0xFFFFu) : uint16_t(0u));
			counter_0[ctx].update(mask);
			counter_1_c1_ref[ctx].update(mask);
			counter_2_ref[ctx][j].update(mask);
			counter_2_ref[ctx][j + 1].update(mask);
			ctx += ctx + (bit_set ? 1u : 0u);
		}
		input = input.subspan(in_idx);

		c2 = c1;
		c1 = ctx - 256u;
		run = (c1 == c2 ? run + 1u : 0u);
		return uint8_t(c1);
	}
};

struct bcm_crc {
	uint32_t tab[256];
	uint32_t crc { ~0u };
	std::span<uint8_t> output;

	explicit bcm_crc(std::span<uint8_t> output_) : output(output_) {
#pragma clang loop vectorize(enable)
		for (uint32_t i = 0u; i < 256u; ++i) {
			uint32_t r = i;
#pragma unroll
			for (uint32_t j = 0; j < 8; ++j) {
				r = (r >> 1u) ^ (0xEDB88320u & (r & 1u ? ~0u : 0u));
			}
			tab[i] = r;
		}
	}

	uint32_t operator()() const {
		return crc ^ ~0u;
	}

	void update(const std::span<const uint8_t> buf) {
		for (const auto& val : buf) {
			crc = (crc >> 8u) ^ tab[(crc ^ val) & 255u];
		}
	}

	void put(uint32_t c) {
		crc = (crc >> 8u) ^ tab[(crc ^ c) & 255u];
		output[0] = uint8_t(c);
		output = output.subspan(1);
	}
};

size_t bcm_compress(const std::span<const uint8_t> input, std::span<uint8_t> output) {
	if (input.empty() || input.size_bytes() >= 0x7FFF'FFFFu) {
		log_error("BCM: invalid or unsupported input size $'", input.size_bytes());
		return 0u;
	}

	const auto input_size = uint32_t(input.size_bytes());
	auto buf = make_unique<uint8_t[]>(input_size);
	auto SA = make_unique<int[]>(input_size);
	const auto pidx = saisxx_private::saisxx_bwt(input, { buf.get(), input_size }, { SA.get(), input_size });

	bcm_crc crc { span<uint8_t> {} /* not used as output here */ };
	crc.update(input);
	bcm_encoder cm { output };
	cm.put32(input_size); // Block size
	cm.put32(pidx); // BWT index
	for (uint32_t i = 0; i < input_size; ++i) {
		cm.put(buf[i]);
	}
	cm.put32(0); // EOF
	cm.put32(crc()); // CRC32
	cm.flush();

	assert(cm.output.size() <= output.size_bytes());
	return output.size_bytes() - cm.output.size();
}

std::vector<uint8_t> bcm_decompress(const std::span<const uint8_t> input) {
	std::vector<uint8_t> ret(bcm_decoder(input).get32() /* block size */);
	if (const auto decomp_size = bcm_decompress(input, ret); decomp_size && *decomp_size > 0) {
		return ret;
	}
	return {};
}

std::optional<size_t> bcm_decompress(const std::span<const uint8_t> input, std::span<uint8_t> output) {
	bcm_decoder cm { input };
	bcm_crc crc { output };
	const auto block_size = cm.get32();
	if (block_size == 0) {
		return 0;
	}

	const auto idx = cm.get32();
	if (idx < 1 || idx > block_size) {
		log_error("BCM: corrupt input (unexpected BWT index $' with data length $')", idx, block_size);
		return {};
	}

	// inverse BW-transform
	auto ptr = make_unique<uint32_t[]>(block_size);
	uint32_t cnt[257];
	__builtin_memset(cnt, 0, sizeof(cnt));
	if (block_size >= (1u << 24u)) {
		// 5*N
		auto buf = make_unique<uint8_t[]>(block_size);
		for (uint32_t i = 0; i < block_size; ++i) {
			++cnt[(buf[i] = cm.get()) + 1];
		}
		for (uint32_t i = 1; i < 256; ++i) {
			cnt[i] += cnt[i - 1];
		}

		for (uint32_t i = 0; i < idx; ++i) {
			ptr[cnt[buf[i]]++] = i;
		}
		for (uint32_t i = idx + 1; i <= block_size; ++i) {
			ptr[cnt[buf[i - 1]]++] = i;
		}

		for (uint32_t p = idx; p;) {
			p = ptr[p - 1];
			crc.put(buf[p - (p >= idx)]);
		}
	} else {
		// 4*N
		for (uint32_t i = 0; i < block_size; ++i) {
			++cnt[(ptr[i] = cm.get()) + 1];
		}
		for (uint32_t i = 1; i < 256; ++i) {
			cnt[i] += cnt[i - 1];
		}

		for (uint32_t i = 0; i < idx; ++i) {
			ptr[cnt[ptr[i] & 255u]++] |= i << 8u;
		}
		for (uint32_t i = idx + 1; i <= block_size; ++i) {
			ptr[cnt[ptr[i - 1] & 255u]++] |= i << 8u;
		}

		for (uint32_t p = idx; p;) {
			p = ptr[p - 1] >> 8u;
			crc.put(ptr[p - (p >= idx)]);
		}
	}
	if (cm.get32() != 0) {
		log_error("BCM: invalid EOF");
		return {};
	}

	const auto crc_expected = cm.get32();
	if (const auto crc_computed = crc(); crc_expected != crc_computed) {
		log_error("BCM: CRC error (got $, expected $)", crc_computed, crc_expected);
		return {};
	}
	return block_size;
}

/*
 * sais.hxx for sais-lite
 * Copyright (c) 2008-2010 Yuta Mori All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

namespace saisxx_private {
// find the start or end of each bucket
template <typename string_type, typename bucket_type, typename index_type>
void get_counts(const string_type T, bucket_type C, index_type n, index_type k) {
	index_type i;
	for (i = 0; i < k; ++i) {
		C[i] = 0;
	}
	for (i = 0; i < n; ++i) {
		++C[T[i]];
	}
}
template <typename bucketC_type, typename bucketB_type, typename index_type>
void get_buckets(const bucketC_type C, bucketB_type B, index_type k, bool end) {
	index_type i, sum = 0;
	if (end) {
		for (i = 0; i < k; ++i) {
			sum += C[i];
			B[i] = sum;
		}
	} else {
		for (i = 0; i < k; ++i) {
			sum += C[i];
			B[i] = sum - C[i];
		}
	}
}
template <typename string_type, typename sarray_type, typename bucketC_type, typename bucketB_type, typename index_type>
void lms_sort_1(string_type T, sarray_type SA, bucketC_type C, bucketB_type B, index_type n, index_type k,
				bool recount) {
	sarray_type b;
	index_type i, j;
	typename std::iterator_traits<string_type>::value_type c0, c1;

	// compute SAl
	if (recount) {
		get_counts(T, C, n, k);
	}
	get_buckets(C, B, k, false); // find starts of buckets
	j = n - 1;
	b = SA + B[c1 = T[j]];
	--j;
	*b++ = (T[j] < c1) ? ~j : j;
	for (i = 0; i < n; ++i) {
		if (0 < (j = SA[i])) {
			assert(T[j] >= T[j + 1]);
			if ((c0 = T[j]) != c1) {
				B[c1] = index_type(b - SA);
				b = SA + B[c1 = c0];
			}
			assert(i < (b - SA));
			--j;
			*b++ = (T[j] < c1) ? ~j : j;
			SA[i] = 0;
		} else if (j < 0) {
			SA[i] = ~j;
		}
	}
	// compute SAs
	if (recount) {
		get_counts(T, C, n, k);
	}
	get_buckets(C, B, k, true); // find ends of buckets
	for (i = n - 1, b = SA + B[c1 = 0]; 0 <= i; --i) {
		if (0 < (j = SA[i])) {
			assert(T[j] <= T[j + 1]);
			if ((c0 = T[j]) != c1) {
				B[c1] = index_type(b - SA);
				b = SA + B[c1 = c0];
			}
			assert((b - SA) <= i);
			--j;
			*--b = (T[j] > c1) ? ~(j + 1) : j;
			SA[i] = 0;
		}
	}
}
template <typename string_type, typename sarray_type, typename index_type>
index_type lms_post_proc_1(string_type T, sarray_type SA, index_type n, index_type m) {
	index_type i, j, p, q, plen, qlen, name;
	typename std::iterator_traits<string_type>::value_type c0, c1;
	bool diff = false;

	// compact all the sorted substrings into the first m items of SA 2*m must be not larger than n (provable)
	assert(0 < n);
	for (i = 0; (p = SA[i]) < 0; ++i) {
		SA[i] = ~p;
		assert((i + 1) < n);
	}
	if (i < m) {
		for (j = i, ++i;; ++i) {
			assert(i < n);
			if ((p = SA[i]) < 0) {
				SA[j++] = ~p;
				SA[i] = 0;
				if (j == m) {
					break;
				}
			}
		}
	}

	// store the length of all substrings
	i = n - 1;
	j = n - 1;
	c0 = T[n - 1];
	do {
		c1 = c0;
	} while ((0 <= --i) && ((c0 = T[i]) >= c1));
	for (; 0 <= i;) {
		do {
			c1 = c0;
		} while ((0 <= --i) && ((c0 = T[i]) <= c1));
		if (0 <= i) {
			SA[m + ((i + 1) >> 1)] = j - i;
			j = i + 1;
			do {
				c1 = c0;
			} while ((0 <= --i) && ((c0 = T[i]) >= c1));
		}
	}

	// find the lexicographic names of all substrings
	for (i = 0, name = 0, q = n, qlen = 0; i < m; ++i) {
		p = SA[i], plen = SA[m + (p >> 1)], diff = true;
		if ((plen == qlen) && ((q + plen) < n)) {
			for (j = 0; (j < plen) && (T[p + j] == T[q + j]); ++j) {}
			if (j == plen) {
				diff = false;
			}
		}
		if (diff) {
			++name, q = p, qlen = plen;
		}
		SA[m + (p >> 1)] = name;
	}

	return name;
}
template <typename string_type, typename sarray_type, typename bucketC_type, typename bucketB_type,
		  typename bucketD_type, typename index_type>
void lms_sort_2(string_type T, sarray_type SA, bucketC_type C, bucketB_type B, bucketD_type D, index_type n,
				index_type k) {
	sarray_type b;
	index_type i, j, t, d;
	typename std::iterator_traits<string_type>::value_type c0, c1;

	// compute SAl
	get_buckets(C, B, k, false); // find starts of buckets
	j = n - 1;
	b = SA + B[c1 = T[j]];
	--j;
	t = (T[j] < c1);
	j += n;
	*b++ = (t & 1) ? ~j : j;
	for (i = 0, d = 0; i < n; ++i) {
		if (0 < (j = SA[i])) {
			if (n <= j) {
				d += 1;
				j -= n;
			}
			assert(T[j] >= T[j + 1]);
			if ((c0 = T[j]) != c1) {
				B[c1] = index_type(b - SA);
				b = SA + B[c1 = c0];
			}
			assert(i < (b - SA));
			--j;
			t = c0;
			t = (t << 1) | (T[j] < c1);
			if (D[t] != d) {
				j += n;
				D[t] = d;
			}
			*b++ = (t & 1) ? ~j : j;
			SA[i] = 0;
		} else if (j < 0) {
			SA[i] = ~j;
		}
	}
	for (i = n - 1; 0 <= i; --i) {
		if (0 < SA[i]) {
			if (SA[i] < n) {
				SA[i] += n;
				for (j = i - 1; SA[j] < n; --j) {}
				SA[j] -= n;
				i = j;
			}
		}
	}

	// compute SAs
	get_buckets(C, B, k, true); // find ends of buckets
	for (i = n - 1, d += 1, b = SA + B[c1 = 0]; 0 <= i; --i) {
		if (0 < (j = SA[i])) {
			if (n <= j) {
				d += 1;
				j -= n;
			}
			assert(T[j] <= T[j + 1]);
			if ((c0 = T[j]) != c1) {
				B[c1] = index_type(b - SA);
				b = SA + B[c1 = c0];
			}
			assert((b - SA) <= i);
			--j;
			t = c0;
			t = (t << 1) | (T[j] > c1);
			if (D[t] != d) {
				j += n;
				D[t] = d;
			}
			*--b = (t & 1) ? ~(j + 1) : j;
			SA[i] = 0;
		}
	}
}
template <typename sarray_type, typename index_type>
index_type lms_post_proc_2(sarray_type SA, index_type n, index_type m) {
	index_type i, j, d, name;

	// compact all the sorted LMS substrings into the first m items of SA
	assert(0 < n);
	for (i = 0, name = 0; (j = SA[i]) < 0; ++i) {
		j = ~j;
		if (n <= j) {
			name += 1;
		}
		SA[i] = j;
		assert((i + 1) < n);
	}
	if (i < m) {
		for (d = i, ++i;; ++i) {
			assert(i < n);
			if ((j = SA[i]) < 0) {
				j = ~j;
				if (n <= j) {
					name += 1;
				}
				SA[d++] = j;
				SA[i] = 0;
				if (d == m) {
					break;
				}
			}
		}
	}
	if (name < m) {
		// store the lexicographic names
		for (i = m - 1, d = name + 1; 0 <= i; --i) {
			if (n <= (j = SA[i])) {
				j -= n;
				--d;
			}
			SA[m + (j >> 1)] = d;
		}
	} else {
		// unset flags
		for (i = 0; i < m; ++i) {
			if (n <= (j = SA[i])) {
				j -= n;
				SA[i] = j;
			}
		}
	}

	return name;
}
// compute SA and BWT
template <typename string_type, typename sarray_type, typename bucketC_type, typename bucketB_type, typename index_type>
void induce_sa(string_type T, sarray_type SA, bucketC_type C, bucketB_type B, index_type n, index_type k,
			   bool recount) {
	sarray_type b;
	index_type i, j;
	typename std::iterator_traits<string_type>::value_type c0, c1;
	// compute SAl
	if (recount) {
		get_counts(T, C, n, k);
	}
	get_buckets(C, B, k, false); // find starts of buckets
	b = SA + B[c1 = T[j = n - 1]];
	*b++ = ((0 < j) && (T[j - 1] < c1)) ? ~j : j;
	for (i = 0; i < n; ++i) {
		j = SA[i], SA[i] = ~j;
		if (0 < j) {
			if ((c0 = T[--j]) != c1) {
				B[c1] = index_type(b - SA);
				b = SA + B[c1 = c0];
			}
			*b++ = ((0 < j) && (T[j - 1] < c1)) ? ~j : j;
		}
	}
	// compute SAs
	if (recount) {
		get_counts(T, C, n, k);
	}
	get_buckets(C, B, k, true); // find ends of buckets
	for (i = n - 1, b = SA + B[c1 = 0]; 0 <= i; --i) {
		if (0 < (j = SA[i])) {
			if ((c0 = T[--j]) != c1) {
				B[c1] = index_type(b - SA);
				b = SA + B[c1 = c0];
			}
			*--b = ((j == 0) || (T[j - 1] > c1)) ? ~j : j;
		} else {
			SA[i] = ~j;
		}
	}
}
template <typename string_type, typename sarray_type, typename bucketC_type, typename bucketB_type, typename index_type>
uint32_t compute_bwt(string_type T, sarray_type SA, bucketC_type C, bucketB_type B, index_type n, index_type k,
					 bool recount) {
	sarray_type b;
	index_type i, j;
	uint32_t pidx = ~0u;
	typename std::iterator_traits<string_type>::value_type c0, c1;
	// compute SAl
	if (recount) {
		get_counts(T, C, n, k);
	}
	get_buckets(C, B, k, false); // find starts of buckets
	b = SA + B[c1 = T[j = n - 1]];
	*b++ = ((0 < j) && (T[j - 1] < c1)) ? ~j : j;
	for (i = 0; i < n; ++i) {
		if (0 < (j = SA[i])) {
			SA[i] = ~((index_type)(c0 = T[--j]));
			if (c0 != c1) {
				B[c1] = index_type(b - SA);
				b = SA + B[c1 = c0];
			}
			*b++ = ((0 < j) && (T[j - 1] < c1)) ? ~j : j;
		} else if (j != 0) {
			SA[i] = ~j;
		}
	}
	// compute SAs
	if (recount) {
		get_counts(T, C, n, k);
	}
	get_buckets(C, B, k, true); // find ends of buckets
	for (i = n - 1, b = SA + B[c1 = 0]; 0 <= i; --i) {
		if (0 < (j = SA[i])) {
			SA[i] = (c0 = T[--j]);
			if (c0 != c1) {
				B[c1] = index_type(b - SA);
				b = SA + B[c1 = c0];
			}
			*--b = ((0 < j) && (T[j - 1] > c1)) ? ~((index_type)T[j - 1]) : j;
		} else if (j != 0) {
			SA[i] = ~j;
		} else {
			pidx = uint32_t(i);
		}
	}
	return pidx;
}
template <typename string_type, typename sarray_type, typename bucketC_type, typename bucketB_type, typename index_type>
std::pair<index_type, index_type> stage1_sort(string_type T, sarray_type SA, bucketC_type C, bucketB_type B,
											  index_type n, index_type k, unsigned flags) {
	using char_type = typename std::iterator_traits<string_type>::value_type;
	sarray_type b;
	index_type i, j, name, m;
	char_type c0, c1;
	get_counts(T, C, n, k);
	get_buckets(C, B, k, true); // find ends of buckets
	for (i = 0; i < n; ++i) {
		SA[i] = 0;
	}
	b = SA + n - 1;
	i = n - 1;
	j = n;
	m = 0;
	c0 = T[n - 1];
	do {
		c1 = c0;
	} while ((0 <= --i) && ((c0 = T[i]) >= c1));
	for (; 0 <= i;) {
		do {
			c1 = c0;
		} while ((0 <= --i) && ((c0 = T[i]) <= c1));
		if (0 <= i) {
			*b = j;
			b = SA + --B[c1];
			j = i;
			++m;
			assert(B[c1] != (n - 1));
			do {
				c1 = c0;
			} while ((0 <= --i) && ((c0 = T[i]) >= c1));
		}
	}
	SA[n - 1] = 0;

	if (1 < m) {
		if (flags & (16u | 32u)) {
			assert((j + 1) < n);
			++B[T[j + 1]];
			if (flags & 16u) {
				auto D = make_unique<index_type[]>(uint32_t(k) * 2u);
				for (i = 0, j = 0; i < k; ++i) {
					j += C[i];
					if (B[i] != j) {
						assert(SA[B[i]] != 0);
						SA[B[i]] += n;
					}
					D[uint32_t(i)] = D[uint32_t(i + k)] = 0;
				}
				lms_sort_2(T, SA, C, B, D.get(), n, k);
			} else {
				bucketB_type D = B - k * 2;
				for (i = 0, j = 0; i < k; ++i) {
					j += C[i];
					if (B[i] != j) {
						assert(SA[B[i]] != 0);
						SA[B[i]] += n;
					}
					D[i] = D[i + k] = 0;
				}
				lms_sort_2(T, SA, C, B, D, n, k);
			}
			name = lms_post_proc_2(SA, n, m);
		} else {
			lms_sort_1(T, SA, C, B, n, k, (flags & (4u | 64u)) != 0u);
			name = lms_post_proc_1(T, SA, n, m);
		}
	} else if (m == 1) {
		*b = j + 1;
		name = 1;
	} else {
		name = 0;
	}
	return { m, name };
}
template <typename string_type, typename sarray_type, typename bucketC_type, typename bucketB_type, typename index_type>
uint32_t stage3_sort(string_type T, sarray_type SA, bucketC_type C, bucketB_type B, index_type n, index_type m,
					 index_type k, unsigned flags, bool isbwt) {
	index_type i, j, p, q;
	typename std::iterator_traits<string_type>::value_type c0, c1;
	if ((flags & 8u) != 0) {
		get_counts(T, C, n, k);
	}
	// put all left-most S characters into their buckets
	if (1 < m) {
		get_buckets(C, B, k, 1); // find ends of buckets
		i = m - 1, j = n, p = SA[m - 1], c1 = T[p];
		do {
			q = B[c0 = c1];
			while (q < j) {
				SA[--j] = 0;
			}
			do {
				SA[--j] = p;
				if (--i < 0) {
					break;
				}
				p = SA[i];
			} while ((c1 = T[p]) == c0);
		} while (0 <= i);
		while (0 < j) {
			SA[--j] = 0;
		}
	}
	if (!isbwt) {
		induce_sa(T, SA, C, B, n, k, (flags & (4u | 64u)) != 0);
		return 0;
	}
	return compute_bwt(T, SA, C, B, n, k, (flags & (4u | 64u)) != 0);
}

// find the suffix array SA of T[0..n-1] in {0..k}^n
// use a working space (excluding s and SA) of at most 2n+O(1) for a constant alphabet
template <typename string_type, typename sarray_type, typename index_type>
uint32_t suffix_sort(string_type T, sarray_type SA, index_type fs, index_type n, index_type k, const bool isbwt) {
	sarray_type RA, C, B;
	index_type *Cp, *Bp;
	index_type i, j, m, name, newfs;
	uint32_t flags = 0u;
	typename std::iterator_traits<string_type>::value_type c0, c1;

	// stage 1: reduce the problem by at least 1/2 sort all the S-substrings
	C = B = SA; // for warnings
	unique_ptr<index_type[]> Cp_storage, Bp_storage;
	Cp = nullptr, Bp = nullptr;
	if (k <= 256) {
		Cp = (Cp_storage = make_unique<index_type[]>(uint32_t(k))).get();
		if (k <= fs) {
			B = SA + (n + fs - k);
			flags = 1u;
		} else {
			Bp = (Bp_storage = make_unique<index_type[]>(uint32_t(k))).get();
			flags = 3u;
		}
	} else if (k <= fs) {
		C = SA + (n + fs - k);
		if (k <= (fs - k)) {
			B = C - k;
			flags = 0u;
		} else if (k <= 1024) {
			Bp = (Bp_storage = make_unique<index_type[]>(uint32_t(k))).get();
			flags = 2u;
		} else {
			B = C;
			flags = 64u | 8u;
		}
	} else {
		Cp = (Cp_storage = make_unique<index_type[]>(uint32_t(k))).get();
		Bp = Cp;
		flags = 4u | 8u;
	}
	if ((n <= (std::numeric_limits<index_type>::max() / 2)) && (2 <= (n / k))) {
		if (flags & 1u) {
			flags |= ((k * 2) <= (fs - k)) ? 32u : 16u;
		} else if ((flags == 0u) && ((k * 2) <= (fs - k * 2))) {
			flags |= 32u;
		}
	}
	if (Cp) {
		if (Bp) {
			std::tie(m, name) = stage1_sort(T, SA, Cp, Bp, n, k, flags);
		} else {
			std::tie(m, name) = stage1_sort(T, SA, Cp, B, n, k, flags);
		}
	} else {
		if (Bp) {
			std::tie(m, name) = stage1_sort(T, SA, C, Bp, n, k, flags);
		} else {
			std::tie(m, name) = stage1_sort(T, SA, C, B, n, k, flags);
		}
	}
	if (m < 0) {
		return ~0u;
	}

	// stage 2: solve the reduced problem recurse if names are not yet unique
	if (name < m) {
		newfs = (n + fs) - (m * 2);
		if ((flags & (1u | 4u | 64u)) == 0u) {
			if ((k + name) <= newfs) {
				newfs -= k;
			} else {
				flags |= 8u;
			}
		}
		assert((n >> 1) <= (newfs + m));
		RA = SA + m + newfs;
		for (i = m + (n >> 1) - 1, j = m - 1; m <= i; --i) {
			if (SA[i] != 0) {
				RA[j--] = SA[i] - 1;
			}
		}
		if (suffix_sort(RA, SA, newfs, m, name, false) != 0) {
			return ~0u;
		}
		i = n - 1;
		j = m - 1;
		c0 = T[n - 1];
		do {
			c1 = c0;
		} while ((0 <= --i) && ((c0 = T[i]) >= c1));
		for (; 0 <= i;) {
			do {
				c1 = c0;
			} while ((0 <= --i) && ((c0 = T[i]) <= c1));
			if (0 <= i) {
				RA[j--] = i + 1;
				do {
					c1 = c0;
				} while ((0 <= --i) && ((c0 = T[i]) >= c1));
			}
		}
		for (i = 0; i < m; ++i) {
			SA[i] = RA[SA[i]];
		}
		if (flags & 4u) {
			Cp = (Cp_storage = make_unique<index_type[]>(uint32_t(k))).get();
			Bp = Cp;
		}
		if (flags & 2u) {
			Bp = (Bp_storage = make_unique<index_type[]>(uint32_t(k))).get();
		}
	}

	// stage 3: induce the result for the original problem
	if (Cp) {
		if (Bp) {
			return stage3_sort(T, SA, Cp, Bp, n, m, k, flags, isbwt);
		}
		return stage3_sort(T, SA, Cp, B, n, m, k, flags, isbwt);
	}
	if (Bp) {
		return stage3_sort(T, SA, C, Bp, n, m, k, flags, isbwt);
	}
	return stage3_sort(T, SA, C, B, n, m, k, flags, isbwt);
}

uint32_t saisxx_bwt(const std::span<const uint8_t>& input, std::span<uint8_t> U, std::span<int> A) {
	assert(input.size_bytes() != 0);
	if (input.size_bytes() <= 1) {
		U[0] = input[0];
		return 1;
	}
	auto pidx = saisxx_private::suffix_sort(input.data(), A.data(), 0, int(input.size_bytes()), 256, true);
	U[0] = input.back();
	for (uint32_t i = 0; i < pidx; ++i) {
		U[i + 1] = uint8_t(A[i]);
	}
	for (uint32_t i = pidx + 1, count = uint32_t(input.size_bytes()); i < count; ++i) {
		U[i] = uint8_t(A[i]);
	}
	pidx += 1;
	return pidx;
}
} // namespace saisxx_private
} // namespace bcm

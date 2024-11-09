// BCM (v1.51) - A BWT-based file compressor
//
// Written and placed in the public domain by Ilya Muravyov
// C++ modernization and improvements by Florian Ziesche
//
// Also uses sais-lite (v2.4.1) by Yuta Mori (license in bcm.cpp).

#pragma once

#include <span>
#include <optional>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace bcm {

//! the max amount of storage we need for compressed data for the given "input_size"
inline size_t bcm_estimate_max_compression_size(const size_t input_size) {
	return std::max(input_size + input_size / 10u, 24zu);
}

//! compresses the data in "input" and writes it to "output",
//! see bcm_estimate_max_compression_size() how "output" needs to be sized,
//! returns the actual compressed size on success, 0 on failure
size_t bcm_compress(const std::span<const uint8_t> input, std::span<uint8_t> output);

//! compresses the data in "input" and writes it to a newly allocated vector,
//! returns the compressed data as a vector of the correct size on success, or returns on empty vector on failure
inline std::vector<uint8_t> bcm_compress(const std::span<const uint8_t> input) {
	std::vector<uint8_t> compressed_data(bcm_estimate_max_compression_size(input.size_bytes()));
	if (const auto compressed_size = bcm_compress(input, compressed_data); compressed_size > 0) {
		compressed_data.resize(compressed_size);
		return compressed_data;
	}
	return {};
}

//! decompresses the data in "input" and writes it to "output" (that must be appropriately sized),
//! returns the decompressed size on success, empty on failure
std::optional<size_t> bcm_decompress(const std::span<const uint8_t> input, std::span<uint8_t> output);

//! decompresses the data in "input",
//! returns the decompressed data as a vector of the correct size on success, or returns on empty vector on failure
std::vector<uint8_t> bcm_decompress(const std::span<const uint8_t> input);

} // namespace bcm

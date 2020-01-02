/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2020 Florian Ziesche
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License only.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __FLOOR_COMPUTE_SOFT_PRINTF_HPP__
#define __FLOOR_COMPUTE_SOFT_PRINTF_HPP__

static constexpr const inline uint32_t printf_buffer_size { 1 * 1024 * 1024 };
static constexpr const inline uint32_t printf_buffer_header_size { 2 * sizeof(uint32_t) };
static inline void handle_printf_buffer(const unique_ptr<uint32_t[]>& buf) {
	const uint32_t total_size = buf[1];
	if (total_size != printf_buffer_size) {
		log_error("device printf has overwritten printf buffer size!");
		return;
	}
	const uint32_t bytes_written = min(buf[0], printf_buffer_size);
	if (bytes_written <= printf_buffer_header_size) {
		return; // nothing was written
	}
	
	// handle/decode printf buffer
	const uint32_t* buf_ptr = &buf[2];
	do {
		const auto cur_size = uintptr_t(buf_ptr) - uintptr_t(&buf[0]);
		if (cur_size >= bytes_written) {
			break; // done
		}
		
		const uint32_t entry_size = *buf_ptr;
		if (entry_size == 0) {
			log_error("printf entry with 0 size");
			break;
		}
		if (entry_size % 4u != 0u) {
			log_error("invalid entry size: %u (expected multiple of 4)", entry_size);
			break;
		}
		
		if (cur_size + entry_size > bytes_written) {
			log_error("out-of-bounds entry: total %u, entry: %u", bytes_written, cur_size + entry_size);
			break;
		}
		uint32_t entry_bytes_parsed = 4;
		
		// get format string
		string format_str;
		const char* format_ptr = (const char*)(buf_ptr + 1);
		while (*format_ptr != '\0' && entry_bytes_parsed < entry_size) {
			format_str += *format_ptr++;
			++entry_bytes_parsed;
		}
		// round to next multiple of 4
		switch (entry_bytes_parsed % 4u) {
			case 3: entry_bytes_parsed += 1; break;
			case 2: entry_bytes_parsed += 2; break;
			case 1: entry_bytes_parsed += 3; break;
			default: break;
		}
		
		// get args
		// NOTE: we only support 32-bit values right now
		union printf_arg_t {
			uint32_t u32;
			int32_t i32;
			float f32;
		};
		vector<printf_arg_t> args;
		const uint32_t* arg_ptr = buf_ptr + entry_bytes_parsed / 4;
		while (entry_bytes_parsed < entry_size) {
			args.emplace_back(printf_arg_t { .u32 = *arg_ptr++ });
			entry_bytes_parsed += 4;
		}
		
		// print
		stringstream sstr;
		bool is_invalid = false;
		auto cur_arg = args.cbegin();
		for (auto ch = format_str.cbegin(); ch != format_str.cend(); ) {
			if (*ch != '%') {
				sstr << *ch++;
				continue;
			}
			
			// must have an arg for this format specifier
			if (cur_arg == args.cend()) {
				is_invalid = true;
				log_error("insufficient #args for printf");
				break;
			}
			
			// parse and handle format specifier
			if (++ch == format_str.cend()) {
				log_error("premature end of format string after '%%'");
				is_invalid = true;
				break;
			}
			bool is_done = true;
			do {
				is_done = true;
				const auto cur_ch = *ch;
				switch (*ch) {
					case '%':
						sstr << '%';
						break;
					case 'u': // unsigned integer
						sstr << cur_arg->u32;
						break;
					case 'd':
					case 'i': // integer
						sstr << cur_arg->i32;
						break;
					case 'X':
					case 'x': // hex
						sstr << hex;
						if (cur_ch == 'X') sstr << uppercase;
						sstr << cur_arg->u32;
						if (cur_ch == 'X') sstr << nouppercase;
						sstr << dec;
						break;
					case 'o': // octal
						sstr << oct;
						sstr << cur_arg->u32;
						sstr << dec;
						break;
					case 'F':
					case 'f': // float
						sstr << cur_arg->f32;
						break;
					case 'j': // *intmax
					case 'z': // size_t
					case 't': // ptrdiff
					case 'L': // long double
						if (++ch == format_str.cend()) {
							log_error("premature end of format string after '%c'", cur_ch);
							is_invalid = true;
							break;
						}
						is_done = false;
						break;
					case 'l': // long...
						if (++ch == format_str.cend()) {
							log_error("premature end of format string after 'l'");
							is_invalid = true;
							break;
						}
						if (*ch == 'l') {
							if (++ch == format_str.cend()) {
								log_error("premature end of format string after 'll'");
								is_invalid = true;
								break;
							}
							if (*ch == 'l') {
								log_error("'lll' format specified is invalid");
								is_invalid = true;
								break;
							}
						}
						is_done = false;
						break;
					case 'h': // short...
						if (++ch == format_str.cend()) {
							log_error("premature end of format string after 'h'");
							is_invalid = true;
							break;
						}
						if (*ch == 'h') {
							if (++ch == format_str.cend()) {
								log_error("premature end of format string after 'hh'");
								is_invalid = true;
								break;
							}
							if (*ch == 'h') {
								log_error("'hhh' format specified is invalid");
								is_invalid = true;
								break;
							}
						}
						is_done = false;
						break;
					case 'c':
					case 's':
					case 'E':
					case 'e':
					case 'A':
					case 'a':
					case 'G':
					case 'g':
					case 'n':
					case 'p':
						log_error("unsupported format specifier: %c", *ch);
						is_invalid = true;
						break;
					default:
						log_error("unknown/invalid format specifier: %c", *ch);
						is_invalid = true;
						break;
				}
				if (is_invalid) break;
			} while(!is_done);
			if (is_invalid) break;
			
			++cur_arg;
			++ch;
		}
		if (!is_invalid) {
			printf("%s", sstr.str().c_str());
		}
		
		// done
		buf_ptr += entry_size / 4;
	} while(true);
}

#endif

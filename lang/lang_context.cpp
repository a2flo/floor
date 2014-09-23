/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2014 Florian Ziesche
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

#include <floor/lang/lang_context.hpp>
#include <floor/lang/lexer.hpp>
#include <floor/lang/parser.hpp>
#include <fstream>

uint32_t lang_context::process_files() {
	uint32_t successful_files { 0 };
	for(const auto& file_name : files) {
		// ifstream object if an actual file is being used
		// note that this will automatically close the file in the destructor
		ifstream file;
		const bool file_is_cin = (file_name == "-");
		
		// when tests are run, the file name must be slighty adjusted
		const string adjusted_file_name {
			file_name
		};
		
		// create translation unit for this file
		auto tu = make_unique<translation_unit>(file_is_cin ? "<stdin>" : adjusted_file_name);
		if(translation_units.count(tu->file_name) > 0) {
			// file has already been processed, silently continue
			continue;
		}
		
		// both ifstream and cin inherit from basic_istream whose functionality is sufficient for us
		basic_istream<char>& stream = (file_is_cin ? cin : file);
		
		// in-memory storage of the input (both file and stdin)
		auto& source = tu->source;
		
		// insert new translation unit into our TU container so that it can be looked up
		auto tu_insert = translation_units.emplace(adjusted_file_name, move(tu));
		if(!tu_insert.second) {
			log_error("failed to insert translation unit for file \"%s\"", adjusted_file_name);
			continue;
		}
		auto& tu_ref = *tu_insert.first->second.get();
		
		// read the file (either from stdin or from a file)
		if(file_is_cin) {
			// stdin/cin can't be read all at once (we don't know its size)
			// -> read it in chunks of 4096 bytes
			constexpr uint32_t chunk_size = 4096;
			char stream_data[chunk_size];
			while(!stream.eof() || !stream.fail()) {
				stream.read(stream_data, chunk_size);
				// gcount() tells us how many bytes have actually been read
				if(stream.gcount() > 0) {
					source.append(stream_data, (uint32_t)stream.gcount());
				}
			}
		}
		else {
			file.open(file_name, ios::in | ios::binary);
			if(!file.is_open()) {
				log_error("couldn't open file \"%s\"", file_name);
				continue;
			}
			
			// get the stream size and allocate enough memory
			stream.seekg(0, ios::end);
			const auto stream_size = stream.tellg();
			if(stream_size < 0) {
				log_error("invalid file size in file \"%s\"", tu_ref.file_name);
				continue;
			}
			stream.seekg(0, ios::beg);
			source.resize((size_t)stream_size);
			if(source.size() < (uint64_t)stream_size) {
				log_error("couldn't allocate enough memory to read input from \"%s\"",
						  tu_ref.file_name);
				continue;
			}
			
			// and read everything
			stream.read(&source.front(), (streamsize)stream_size);
		}
		
		// finally: process the input according to the set compiler mode
		lexer::map_characters(*this, tu_ref);
		lexer::lex(*this, tu_ref);
		switch(mode) {
			case COMPILER_MODE::TOKENIZE:
				// if lexing was successful, print all tokens
				if(!has_new_errors()) {
					lexer::print_tokens(tu_ref);
				}
				break;
			case COMPILER_MODE::PARSE:
				// assign token sub-types for easier (enum-based) parsing
				lexer::assign_token_sub_types(tu_ref);
				
				// TODO: parse
				if(!has_new_errors()) {
					parser::parse(*this, tu_ref);
				}
				else break;
				
				if(dump_ast && tu_ref.ast) {
					tu_ref.ast->dump();
				}
				
				// TODO: actual sema checking
				if(mode == COMPILER_MODE::PARSE) break;
		}
		++successful_files;
	}
	return successful_files;
}

void lang_context::clean() {
	translation_units.clear();
}

void lang_context::set_mode(const COMPILER_MODE mode_) {
	mode = mode_;
}

const lang_context::COMPILER_MODE& lang_context::get_mode() const {
	return mode;
}

void lang_context::add_file(string&& file_name) {
	// i like to - move it ;)
	files.emplace_back(file_name);
}

void lang_context::add_file(const string& file_name) {
	files.emplace_back(file_name);
}

void lang_context::set_dump_ast(const bool state) {
	dump_ast = state;
}

const bool& lang_context::get_dump_ast() const {
	return dump_ast;
}

bool lang_context::has_new_errors() {
	const bool cur_new_errors = new_errors;
	new_errors = false;
	return cur_new_errors;
}

const translation_unit* lang_context::get_translation_unit(const string& file_name) const {
	const auto iter = translation_units.find(file_name);
	if(iter == translation_units.end()) {
		return nullptr;
	}
	return iter->second.get();
}

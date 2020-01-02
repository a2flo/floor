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

#ifndef __FLOOR_GRAMMAR_HPP__
#define __FLOOR_GRAMMAR_HPP__

#include <floor/core/essentials.hpp>
#include <floor/core/util.hpp>
#include <floor/lang/source_types.hpp>
#include <floor/lang/lang_context.hpp>
#include <floor/lang/lexer.hpp>

// uncomment this define if you want the parse tree to be printed
//#define FLOOR_DEBUG_PARSER 1
// uncomment this if you only want to print actual matches (implied by the above)
//#define FLOOR_DEBUG_PARSER_MATCHES_ONLY 1
// uncomment this if you want to set the debug names of lhs grammar objects (implied by the above)
#define FLOOR_DEBUG_PARSER_SET_NAMES 1

#if defined(FLOOR_DEBUG_PARSER)
#include <iostream>
#endif

//! ast node base class (inherit from this when using an ast)
struct ast_node_base {
	//! the token range in the source code (if applicable)
	token_range range;
};

//! parser context object that handles all token iteration, backtracking and stores all matches.
struct parser_context {
	//! current parser token iterator
	token_iterator iter;
	//! deepest (greatest) encountered token iterator (used for rudimentary error handling)
	token_iterator deepest_iter;
	//! begin of the tokens
	const token_iterator begin;
	//! end of the tokens
	const token_iterator end;
	//! ref to the corresponding translation unit
	const translation_unit& tu;
	//! iterator stack used for backtracking (simple push/pop stack)
	vector<token_iterator> iter_stack;
	
	parser_context(const translation_unit& tu_) :
	iter(tu_.tokens.cbegin()), deepest_iter(iter),
	begin(tu_.tokens.cbegin()), end(tu_.tokens.cend()),
	tu(tu_) {
		// 512 seems reasonable for now
		iter_stack.reserve(512);
	}
	
	parser_context(parser_context&& ctx) :
	iter(ctx.iter), deepest_iter(iter),
	begin(ctx.begin), end(ctx.end),
	tu(ctx.tu), iter_stack(move(ctx.iter_stack)) {}
	
	parser_context& operator=(parser_context&& ctx) {
		assert(begin == ctx.begin && end == ctx.end && &tu == &ctx.tu);
		iter = ctx.iter;
		return *this;
	}
	parser_context& operator=(const parser_context& ctx) {
		assert(begin == ctx.begin && end == ctx.end && &tu == &ctx.tu);
		iter = ctx.iter;
		return *this;
	}
	
	//! specifies whether a match contains a token (iterator) or an AST node pointer
	enum class MATCH_TYPE {
		INVALID,
		TOKEN,
		AST_NODE
	};
	
	//! the actual match node for a single match:
	//! this can either be a simple token iterator or an AST node that has been constructed
	//! in a lower parser tree level (and is now moving upwards)
	struct match {
		MATCH_TYPE type { MATCH_TYPE::AST_NODE };
		token_iterator token;
		unique_ptr<ast_node_base> ast_node;
		
		match() noexcept {}
		match(const token_iterator& token_) noexcept : type(MATCH_TYPE::TOKEN), token(token_) {}
		match(unique_ptr<ast_node_base> ast_node_) noexcept : type(MATCH_TYPE::AST_NODE), ast_node(move(ast_node_)) {
#if defined(FLOOR_DEBUG)
			if(ast_node == nullptr) {
				assert(ast_node != nullptr);
			}
#endif
		}
		match(match&& m) noexcept : type(m.type), token(m.token), ast_node(m.type == MATCH_TYPE::AST_NODE ? move(m.ast_node) : nullptr) {
#if defined(FLOOR_DEBUG)
			if(type == MATCH_TYPE::AST_NODE && ast_node == nullptr) {
				assert(ast_node != nullptr);
			}
#endif
		}
		match& operator=(match&& m) noexcept {
			type = m.type;
			token = m.token;
			ast_node = (type == MATCH_TYPE::AST_NODE ? move(m.ast_node) : nullptr);
			return *this;
		}
	};
	
	//! container type for all matches inside a grammar_rules object level
	struct match_list {
		vector<match> list;
		match_list() noexcept {}
		match_list(vector<match>&& vec) noexcept : list(move(vec)) {}
		match_list(match_list&& ml) noexcept : list(move(ml.list)) {}
		match_list(match&& m) noexcept {
			list.emplace_back(move(m));
		}
		match_list(unique_ptr<ast_node_base> ast_node) noexcept {
			list.emplace_back(move(ast_node));
		}
		size_t size() const { return list.size(); }
		match& operator[](const size_t& idx) { return list[idx]; }
		const match& operator[](const size_t& idx) const { return list[idx]; }
		bool empty() const { return list.empty(); }
		match& front() { return list.front(); }
		const match& front() const { return list.front(); }
		match& back() { return list.back(); }
		const match& back() const { return list.back(); }
		token_range range() const {
			assert(!list.empty());
			return token_range {
				list.front().type == MATCH_TYPE::TOKEN ? list.front().token : list.front().ast_node->range.first,
				list.back().type == MATCH_TYPE::TOKEN ? list.back().token : list.back().ast_node->range.second,
			};
		}
	};
	match_list root_match;
	
	//! stack of all current (in progress) matches. note the following:
	//! * the memory is handled by each grammar_rule object.
	//! * when going down a level (inside another grammar_rule object), a new match_list will be added onto the stack
	//! * the first match_list in the stack is the root_match, which should never be removed!
	vector<match_list*> match_stack { &root_match };
	
	//! increase token iterator
	bool next() {
		// don't iterate past the end
		if(iter == end) return false;
		
		++iter;
		if(iter > deepest_iter) {
			deepest_iter = iter;
		}
		return true;
	}
	bool at_end() const {
		return (iter == end);
	}
	
	// iterator stack functions (stack based backtracking)
#if !defined(FLOOR_DEBUG_PARSER)
	void push() {
		iter_stack.emplace_back(iter);
	}
	void pop() {
		iter_stack.pop_back();
	}
	void pop_restore() {
		iter = iter_stack.back();
		iter_stack.pop_back();
	}
#else
	token_iterator push() {
		iter_stack.emplace_back(iter);
		return iter;
	}
	token_iterator pop() {
		if(iter_stack.empty()) {
			log_error("can't pop from an empty iterator stack!");
			return token_iterator {};
		}
		auto ret = iter_stack.back();
		iter_stack.pop_back();
		return ret;
	}
	token_iterator pop_restore() {
		if(iter_stack.empty()) {
			log_error("can't pop from an empty iterator stack!");
			return token_iterator {};
		}
		iter = iter_stack.back();
		iter_stack.pop_back();
		return iter;
	}
	
	// debug functions
	uint32_t depth { 0 };
	void inc_depth() { ++depth; }
	void dec_depth() { --depth; }
	void dump_depth() const {
		if(depth == 0) return;
		cout << "|-";
		for(uint32_t i = 1; i < depth; i++) {
			cout << "--";
		}
	}
	void print_at_depth(const string& str) const {
		for(uint32_t i = 0; i < depth; i++) cout << "  ";
		cout << str;
		if(iter < end) {
			const auto line_and_column = lexer::get_line_and_column_from_iter(tu, iter->second.begin);
			cout << " (" << line_and_column.first << ":" << line_and_column.second << ")";
		}
		cout << endl;
	}
#endif

};

//! each rule match function needs to return whether:
//! (a) it was successful
//! (b) the match needs to be pushed onto the match list
//! (c) the matches themselves
struct match_return_type {
	const bool successful;
	const bool push_node;
	parser_context::match_list matches;
	
	//! full constructor with a moved match_list
	match_return_type(const bool successful_, const bool push_node_, parser_context::match_list&& matches_) noexcept :
	successful(successful_), push_node(push_node_), matches(move(matches_)) {}
	
	//! construct from a single match (this implies "successful" and "push_node")
	match_return_type(parser_context::match&& match) noexcept :
	successful(true), push_node(true) {
		matches.list.emplace_back(move(match));
	}
};

//! base class of every parser tree node
template <typename T> struct parser_node_base {
	//! the enclosed type of the parser node object
	typedef T enclosed_type;
	//! storing a const copy is the default, but this can be overwritten in any deriving class.
	//! note that we usually want to store actual copies, since any operator execution leads to temporaries,
	//! which we obviously shouldn't store ;)
	typedef const T storage_type;
	
	//! use this to access the enclosed object
	const T& get_enclosed() const { return *(const enclosed_type*)this; }
	
	//! NOTE: only for demonstration purposes - any derived class should implement this
	match_return_type match(parser_context&) const {
		return { false, false, {} };
	}
};

//! (empty) base class for all literal matchers.
//! NOTE: the storage_type of every specialized literal_matcher can (should!) be a const ref to the specific class.
template <typename T, SOURCE_TOKEN_TYPE token_mask> struct literal_matcher : public parser_node_base<T> {};

//! epsilon: matches anything without touching the iterator and without adding anything to the match list
struct epsilon_matcher : parser_node_base<epsilon_matcher> {
	typedef const epsilon_matcher& storage_type;
	match_return_type match(parser_context& ctx
#if !defined(FLOOR_DEBUG_PARSER) || defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
							floor_unused
#endif
							) const {
#if defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		ctx.print_at_depth("matching EPSILON");
#endif
		return { true, false, {} };
	}
};

//!
template <> struct literal_matcher<FLOOR_KEYWORD, SOURCE_TOKEN_TYPE::KEYWORD> :
public parser_node_base<literal_matcher<FLOOR_KEYWORD, SOURCE_TOKEN_TYPE::KEYWORD>> {
	typedef const literal_matcher<FLOOR_KEYWORD, SOURCE_TOKEN_TYPE::KEYWORD>& storage_type;
	const FLOOR_KEYWORD keyword;
	constexpr literal_matcher(const FLOOR_KEYWORD& keyword_) noexcept : keyword(keyword_) {}
	match_return_type match(parser_context& ctx) const {
#if defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		ctx.print_at_depth("matching IDENTIFIER");
#endif
		if(ctx.at_end() || (ctx.iter->first & SOURCE_TOKEN_TYPE::KEYWORD) == SOURCE_TOKEN_TYPE::INVALID) {
			return { false, false, {} };
		}
		if((FLOOR_KEYWORD)(ctx.iter->first & SOURCE_TOKEN_TYPE::__SUB_TYPE_MASK) == keyword) {
			match_return_type ret { ctx.iter };
			ctx.next();
			return ret;
		}
		return { false, false, {} };
	}
};

//!
template <> struct literal_matcher<FLOOR_PUNCTUATOR, SOURCE_TOKEN_TYPE::PUNCTUATOR> :
public parser_node_base<literal_matcher<FLOOR_PUNCTUATOR, SOURCE_TOKEN_TYPE::PUNCTUATOR>> {
	typedef const literal_matcher<FLOOR_PUNCTUATOR, SOURCE_TOKEN_TYPE::PUNCTUATOR>& storage_type;
	const FLOOR_PUNCTUATOR punctuator;
	constexpr literal_matcher(const FLOOR_PUNCTUATOR& punctuator_) noexcept : punctuator(punctuator_) {}
	match_return_type match(parser_context& ctx) const {
#if defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		ctx.print_at_depth("matching PUNCTUATOR #" + to_string((uint16_t)punctuator) +
						   " to " + to_string(uint32_t(ctx.iter->first & SOURCE_TOKEN_TYPE::__SUB_TYPE_MASK)));
#endif
		if(ctx.at_end() || (ctx.iter->first & SOURCE_TOKEN_TYPE::PUNCTUATOR) == SOURCE_TOKEN_TYPE::INVALID) {
			return { false, false, {} };
		}
		if((FLOOR_PUNCTUATOR)(ctx.iter->first & SOURCE_TOKEN_TYPE::__SUB_TYPE_MASK) == punctuator) {
			match_return_type ret { ctx.iter };
			ctx.next();
			return ret;
		}
		return { false, false, {} };
	}
};

//!
template <SOURCE_TOKEN_TYPE token_mask> struct literal_matcher<char, token_mask> :
public parser_node_base<literal_matcher<char, token_mask>> {
	typedef const literal_matcher<char, token_mask>& storage_type;
	const char c;
	constexpr literal_matcher(const char& c_) noexcept : c(c_) {}
	match_return_type match(parser_context& ctx) const {
#if defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		ctx.print_at_depth(string("matching ") + c);
#endif
		if(ctx.at_end() || (ctx.iter->first & token_mask) == SOURCE_TOKEN_TYPE::INVALID) {
			return { false, false, {} };
		}
		if(ctx.iter->second == c) {
			match_return_type ret { ctx.iter };
			ctx.next();
			return ret;
		}
		return { false, false, {} };
	}
};

//!
template <SOURCE_TOKEN_TYPE token_mask> struct literal_matcher<const char*, token_mask> :
public parser_node_base<literal_matcher<const char*, token_mask>> {
	typedef const literal_matcher<const char*, token_mask>& storage_type;
	const char* str;
	const size_t len;
	template <size_t str_len> constexpr literal_matcher(const char (&str_)[str_len]) noexcept : str(str_), len(str_len - 1) {}
	match_return_type match(parser_context& ctx) const {
#if defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		ctx.print_at_depth("matching " + string(str));
#endif
		if(ctx.at_end() || (ctx.iter->first & token_mask) == SOURCE_TOKEN_TYPE::INVALID) {
			return { false, false, {} };
		}
		if(ctx.iter->second.equal(str, len)) {
			match_return_type ret { ctx.iter };
			ctx.next();
			return ret;
		}
		return { false, false, {} };
	}
};

//! identifier matcher (only matches token type, string/char is unimportant)
template <> struct literal_matcher<const char*, SOURCE_TOKEN_TYPE::IDENTIFIER> :
public parser_node_base<literal_matcher<const char*, SOURCE_TOKEN_TYPE::IDENTIFIER>> {
	typedef const literal_matcher<const char*, SOURCE_TOKEN_TYPE::IDENTIFIER>& storage_type;
	match_return_type match(parser_context& ctx) const {
#if defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		ctx.print_at_depth("matching IDENTIFIER");
#endif
		if(!ctx.at_end() && (ctx.iter->first & SOURCE_TOKEN_TYPE::IDENTIFIER) == SOURCE_TOKEN_TYPE::IDENTIFIER) {
			match_return_type ret { ctx.iter };
			ctx.next();
			return ret;
		}
		return { false, false, {} };
	}
};
//! constant matcher (only matches token type, string/char is unimportant)
template <> struct literal_matcher<const char*, SOURCE_TOKEN_TYPE::CONSTANT> :
public parser_node_base<literal_matcher<const char*, SOURCE_TOKEN_TYPE::CONSTANT>> {
	typedef const literal_matcher<const char*, SOURCE_TOKEN_TYPE::CONSTANT>& storage_type;
	match_return_type match(parser_context& ctx) const {
#if defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		ctx.print_at_depth("matching CONSTANT");
#endif
		if(!ctx.at_end() && (ctx.iter->first & SOURCE_TOKEN_TYPE::CONSTANT) == SOURCE_TOKEN_TYPE::CONSTANT) {
			match_return_type ret { ctx.iter };
			ctx.next();
			return ret;
		}
		return { false, false, {} };
	}
};
//! string literal matcher (only matches token type, string/char is unimportant)
template <> struct literal_matcher<const char*, SOURCE_TOKEN_TYPE::STRING_LITERAL> :
public parser_node_base<literal_matcher<const char*, SOURCE_TOKEN_TYPE::STRING_LITERAL>> {
	typedef const literal_matcher<const char*, SOURCE_TOKEN_TYPE::STRING_LITERAL>& storage_type;
	match_return_type match(parser_context& ctx) const {
#if defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		ctx.print_at_depth("matching STRING-LITERAL");
#endif
		if(!ctx.at_end() && (ctx.iter->first & SOURCE_TOKEN_TYPE::STRING_LITERAL) == SOURCE_TOKEN_TYPE::STRING_LITERAL) {
			match_return_type ret { ctx.iter };
			ctx.next();
			return ret;
		}
		return { false, false, {} };
	}
};
//! constant matcher (only matches token type, string/char is unimportant)
template <> struct literal_matcher<const char*, SOURCE_TOKEN_TYPE::INTEGER_CONSTANT> :
public parser_node_base<literal_matcher<const char*, SOURCE_TOKEN_TYPE::INTEGER_CONSTANT>> {
	typedef const literal_matcher<const char*, SOURCE_TOKEN_TYPE::INTEGER_CONSTANT>& storage_type;
	match_return_type match(parser_context& ctx) const {
#if defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		ctx.print_at_depth("matching INTEGER-CONSTANT");
#endif
		if(!ctx.at_end() && (ctx.iter->first & SOURCE_TOKEN_TYPE::INTEGER_CONSTANT) == SOURCE_TOKEN_TYPE::INTEGER_CONSTANT) {
			match_return_type ret { ctx.iter };
			ctx.next();
			return ret;
		}
		return { false, false, {} };
	}
};
//! constant matcher (only matches token type, string/char is unimportant)
template <> struct literal_matcher<const char*, SOURCE_TOKEN_TYPE::UNSIGNED_INTEGER_CONSTANT> :
public parser_node_base<literal_matcher<const char*, SOURCE_TOKEN_TYPE::UNSIGNED_INTEGER_CONSTANT>> {
	typedef const literal_matcher<const char*, SOURCE_TOKEN_TYPE::UNSIGNED_INTEGER_CONSTANT>& storage_type;
	match_return_type match(parser_context& ctx) const {
#if defined(FLOOR_DEBUG_PARSER) && !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		ctx.print_at_depth("matching UNSIGNED-INTEGER-CONSTANT");
#endif
		if(!ctx.at_end() && (ctx.iter->first & SOURCE_TOKEN_TYPE::UNSIGNED_INTEGER_CONSTANT) == SOURCE_TOKEN_TYPE::UNSIGNED_INTEGER_CONSTANT) {
			match_return_type ret { ctx.iter };
			ctx.next();
			return ret;
		}
		return { false, false, {} };
	}
};

//! binary container for parser tree objects
template <typename lhs_rule, typename rhs_rule, typename parser_type>
struct parser_binary : public parser_type {
	typename lhs_rule::storage_type lhs;
	typename rhs_rule::storage_type rhs;
	constexpr parser_binary(const lhs_rule& lhs_, const rhs_rule& rhs_) noexcept : lhs(lhs_), rhs(rhs_) {}
};

//! unary container for parser tree objects
template <typename node_rule, typename parser_type>
struct parser_unary : public parser_type {
	typename node_rule::storage_type node;
	constexpr parser_unary(const node_rule& node_) noexcept : node(node_) {}
};

//! use this to define a binary grammar operator
#define grammar_rule_binary(name, op) \
template <typename lhs_rule, typename rhs_rule> struct grammar_rule_ ## name; /* forward declaration */ \
template <typename lhs_rule, typename rhs_rule> \
inline grammar_rule_ ## name<lhs_rule, rhs_rule> \
/* provide the scope function/operator to combine parser objects */ \
operator op(const parser_node_base<lhs_rule>& lhs, const parser_node_base<rhs_rule>& rhs) { \
	return grammar_rule_ ## name<lhs_rule, rhs_rule> { lhs.get_enclosed(), rhs.get_enclosed() }; \
} \
template <typename lhs_rule, typename rhs_rule> \
struct grammar_rule_ ## name : public parser_binary<lhs_rule, rhs_rule, parser_node_base<grammar_rule_ ## name<lhs_rule, rhs_rule>>> { \
	typedef parser_binary<lhs_rule, rhs_rule, parser_node_base<grammar_rule_ ## name<lhs_rule, rhs_rule>>> base_type; \
	using grammar_rule_ ## name::base_type::base_type; /* inheriting template constructors is fun */ \
	using base_type::lhs; /* we would have to use this->lhs everywhere otherwise */ \
	using base_type::rhs; \
	match_return_type match(parser_context& ctx) const /* impl by user */

//! use this to define an unary grammar operator
#define grammar_rule_unary(name, op) \
template <typename node_rule> struct grammar_rule_ ## name; /* forward declaration */ \
template <typename node_rule> \
inline grammar_rule_ ## name<node_rule> \
/* provide the scope function/operator to combine parser objects */ \
operator op(const parser_node_base<node_rule>& node) { \
	return grammar_rule_ ## name<node_rule> { node.get_enclosed() }; \
} \
template <typename node_rule> \
struct grammar_rule_ ## name : public parser_unary<node_rule, parser_node_base<grammar_rule_ ## name<node_rule>>> { \
	typedef parser_unary<node_rule, parser_node_base<grammar_rule_ ## name<node_rule>>> base_type; \
	using grammar_rule_ ## name::base_type::base_type; /* inheriting template constructors is fun */ \
	using base_type::node; /* we would have to use this->node everywhere otherwise */ \
	match_return_type match(parser_context& ctx) const /* impl by user */

//! properly end the grammar operator defintion (close the struct)
#define grammar_rule_end };

void move_matches(parser_context::match_list& dst_matches, parser_context::match_list& src_matches);

grammar_rule_binary(concat, &) {
	auto& match_list = *ctx.match_stack.back();
	ctx.push();
	
	auto lhs_ret = lhs.match(ctx);
	if(!lhs_ret.successful) {
		ctx.pop_restore();
		return { false, false, {} };
	}
	
	// already move matches onto the match list (but save positions in case they need to be dropped again)
	const auto drop_iter_begin = match_list.size();
	if(lhs_ret.push_node) move_matches(match_list, lhs_ret.matches);
	const auto drop_iter_end = match_list.size();
	
	auto rhs_ret = rhs.match(ctx);
	if(!rhs_ret.successful) {
		ctx.pop_restore();
		if(drop_iter_begin != drop_iter_end) {
			typedef typename vector<parser_context::match>::difference_type diff_type;
			match_list.list.erase(next(begin(match_list.list), (diff_type)drop_iter_begin),
								  next(begin(match_list.list), (diff_type)drop_iter_end));
		}
		return { false, false, {} };
	}
	if(rhs_ret.push_node) move_matches(match_list, rhs_ret.matches);
	
	// success
	ctx.pop();
	
	// matches have already been pushed -> no need to return and push them again
	return { true, false, {} };
} grammar_rule_end;

grammar_rule_binary(or, |) {
	auto& match_list = *ctx.match_stack.back();
	ctx.push();
	
	// match lhs first
	auto lhs_ret = lhs.match(ctx);
	if(!lhs_ret.successful) {
		ctx.pop_restore();
		// nothing to be done
	}
	else {
		// lhs matched -> return
		if(lhs_ret.push_node) move_matches(match_list, lhs_ret.matches);
		ctx.pop();
		return { true, false, {} };
	}
	ctx.push();
	
	// match rhs second (only if lhs didn't match)
	auto rhs_ret = rhs.match(ctx);
	if(!rhs_ret.successful) {
		ctx.pop_restore();
		// neither side matched -> no match
		return { false, false, {} };
	}
	// rhs matched -> return
	if(rhs_ret.push_node) move_matches(match_list, rhs_ret.matches);
	ctx.pop();
	return { true, false, {} };
} grammar_rule_end;

grammar_rule_unary(match_none_or_more, *) {
	ctx.push();
	parser_context::match_list matches;
	for(;;) {
		auto ret = node.match(ctx);
		if(!ret.successful) break;
		if(ret.push_node) move_matches(matches, ret.matches);
	}
	ctx.pop();
	return { true, true, move(matches) };
} grammar_rule_end;

grammar_rule_unary(match_once_or_more, +) {
	ctx.push();
	auto ret_first = node.match(ctx);
	if(!ret_first.successful) {
		ctx.pop_restore();
		return { false, false, {} };
	}
	parser_context::match_list matches;
	if(ret_first.push_node) move_matches(matches, ret_first.matches);
	for(;;) {
		auto ret = node.match(ctx);
		if(!ret.successful) break;
		if(ret.push_node) move_matches(matches, ret.matches);
	}
	ctx.pop();
	return { true, true, move(matches) };
} grammar_rule_end;

grammar_rule_unary(match_optional, ~) {
	ctx.push();
	auto ret_first = node.match(ctx);
	if(!ret_first.successful) {
		// no match -> nothing to be added to the match list!
		ctx.pop();
		return { true, false, {} };
	}
	// matched once
	ctx.pop();
	// push matched node depending on if it is flagged as such
	// NOTE: this can't be pushed onto the match list here, because it would screw up the ordering
	if(ret_first.push_node) return { true, true, move(ret_first.matches) };
	return { true, false, {} };
} grammar_rule_end;

grammar_rule_unary(match_except, !) {
	ctx.push();
	auto ret = node.match(ctx);
	if(ret.successful) {
		// unwanted match -> fail
		ctx.pop_restore();
		return { false, false, {} };
	}
	ctx.pop();
	// push matched node depending on if it is flagged as such
	// NOTE: this can't be pushed onto the match list here, because it would screw up the ordering
	if(ret.push_node) return { true, true, move(ret.matches) };
	return { true, false, {} };
} grammar_rule_end;

//! for the need to store and call an arbitrary parser tree/node object.
//! use a ptr to parser_wrapper_base for storage and use a parser_node_wrapper<type> for assignments.
struct parser_node_wrapper_base {
	constexpr parser_node_wrapper_base() noexcept {}
	virtual ~parser_node_wrapper_base() noexcept;
	virtual match_return_type match(parser_context& ctx) const = 0;
};

//! see parser_node_wrapper_base.
//! this class exists, so that arbitrary parser node objects can be accessed through the same wrapper type.
template <typename parser_type> struct parser_node_wrapper : parser_node_wrapper_base {
	typename parser_type::storage_type p;
	constexpr parser_node_wrapper(const parser_type& p_) noexcept : p(p_) {}
	//! simple pass-through
	virtual match_return_type match(parser_context& ctx) const override {
		return p.match(ctx);
	}
};

//!
struct grammar_rule : public parser_node_base<grammar_rule> {
	//! we never want (container + match exec logic) or can (infinite construction recursion is fun) make/store a grammar_rule copy,
	//! so use a simple ref instead. also note that grammar_rule objects are always named, actual objects, so this won't lead
	//! to any "ref to temporary object" problems.
	typedef const grammar_rule& storage_type;
	
	//! since we can't store arbitrarily templated parser node objects, we need to do this through some indirection.
	//! we also want to be able to assign a parser_node/grammar_rule object after construction, so a pointer that points to
	//! a parser_node/grammar_rule object that is allocated and wrapped according to its templates is the only option.
	unique_ptr<parser_node_wrapper_base> parser_obj;
	
	//! match function type
	typedef function<parser_context::match_list(parser_context::match_list&)> match_function_type;
	//! match function object of this grammar rule object
	match_function_type match_function { [](parser_context::match_list&) -> parser_context::match_list {
		return {};
	} };
	
#if defined(FLOOR_DEBUG_PARSER) || defined(FLOOR_DEBUG_PARSER_SET_NAMES)
	//! debug name of this grammar rule object (used in tree dump)
	string debug_name { "<none>" };
#endif
	
	//! nothing to see here, move along.
	grammar_rule() noexcept {}
	
	//! creates a reference to another grammar_rule object
	grammar_rule(const grammar_rule& g) noexcept :
	parser_obj(make_unique<parser_node_wrapper<grammar_rule>>(g))
#if defined(FLOOR_DEBUG_PARSER) || defined(FLOOR_DEBUG_PARSER_SET_NAMES)
	, debug_name("&"+g.debug_name)
#endif
	{}
	
	//! creates a reference to another grammar_rule object
	grammar_rule& operator=(const grammar_rule& g) {
		parser_obj = make_unique<parser_node_wrapper<grammar_rule>>(g);
#if defined(FLOOR_DEBUG_PARSER) || defined(FLOOR_DEBUG_PARSER_SET_NAMES)
		debug_name = "&"+g.debug_name;
#endif
		return *this;
	}
	
	//! constructs a wrapped parser object according to its type
	template <typename parser_type> grammar_rule(const parser_type& p) noexcept :
	parser_obj(make_unique<parser_node_wrapper<parser_type>>(p)) {}
	
	//! constructs a wrapped parser object according to its type
	template <typename parser_type> grammar_rule& operator=(const parser_type& p) {
		parser_obj = make_unique<parser_node_wrapper<parser_type>>(p);
		return *this;
	}
	
	//! pass-through to the wrapped grammar_rule/parser node object
	match_return_type match(parser_context& ctx) const {
		assert(parser_obj != nullptr);
		
		// if we're at the end, return immediately
		if(ctx.at_end()) {
			return { false, false, {} };
		}
		
		// create the match list for this level/grammar_rule object
		parser_context::match_list matches;
		ctx.match_stack.emplace_back(&matches);
		
#if defined(FLOOR_DEBUG_PARSER)
		ctx.inc_depth();
#endif
		
		// push the current token iterator onto the stack, so it can be restored on a mismatch
#if !defined(FLOOR_DEBUG_PARSER)
		ctx.push();
#else
		const auto first_iter = ctx.push();
#if !defined(FLOOR_DEBUG_PARSER_MATCHES_ONLY)
		for(uint32_t i = 1; i < ctx.depth; i++) cout << "  ";
		cout << "matching " << debug_name;
		if(!ctx.at_end()) {
			const auto line_and_column = lexer::get_line_and_column_from_iter(ctx.tu, ctx.iter->second.begin);
			cout << " (" << line_and_column.first << ":" << line_and_column.second << ")";
		}
		cout << endl;
#endif
#endif
		// try to match
		auto ret = parser_obj->match(ctx);
		
		// pop the match list for this level from the stack again
		// this isn't needed nor wanted when calling the match_function
		ctx.match_stack.pop_back();
		
#if defined(FLOOR_DEBUG_PARSER)
		ctx.dec_depth();
#endif
		// check if matching was successful
		if(!ret.successful) {
			// restore iter if the sub-rule/grammar-rule didn't match ("backtracking")
			ctx.pop_restore();
		}
		else {
			// match was successful, just pop the iterator again
			ctx.pop();
#if defined(FLOOR_DEBUG_PARSER)
			const auto last_iter = ctx.iter;
			ctx.dump_depth();
			cout << debug_name << " (" << distance(first_iter, last_iter) << "): ";
			for(auto iter = first_iter; iter < last_iter; ++iter) {
				cout << iter->second.to_string() << " ";
			}
			cout << endl;
#endif
			
			// if the last match hasn't been pushed to the match list yet, do so here
			if(ret.push_node) {
				move_matches(matches, ret.matches);
			}
	  
			// call the specified match function and then return the constructed match node
			// to the match stack/function of the prior/upper grammar_rule object
			// note that this will only be called if matches is non-empty (so to ignore epsilon matches)
			assert(!ctx.match_stack.empty());
			if(!matches.empty()) {
				return { ret.successful, true, match_function(matches) };
			}
		}
		
		// tell the upper/parent node that there is now new node in this match
		return { ret.successful, false, {} };
	}
	
	//! adds a on-match function that is called if match was successful for the sub-parse-tree
	void on_match(match_function_type match_func) {
		match_function = match_func;
	}
	
};

#endif

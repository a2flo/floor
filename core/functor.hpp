/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2013 Florian Ziesche
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

#ifndef __FLOOR_FUNCTOR_HPP__
#define __FLOOR_FUNCTOR_HPP__

#include <memory>

// functor implementation
template <typename... types> class functor_impl;
template <typename return_type, typename... Args> class functor_impl<return_type, Args...> {
public:
	virtual return_type operator()(Args... args) = 0;
	virtual functor_impl* clone() const = 0;
	virtual ~functor_impl() {}
};

// functor
template <typename return_type, typename... Args> class functor {
protected:
	typedef functor_impl<return_type, Args...> impl;
	std::unique_ptr<impl> sp_impl;
	
public:
	functor();
	functor(const functor&);
	functor& operator=(const functor&);
	explicit functor(std::unique_ptr<impl> sp_impl_);
	
	typedef return_type result_type;
	
	return_type operator()(Args... args) {
		return (*sp_impl)(args...);
	}
	
	// will be defined later on
	template <class fun> functor(const fun& func);
	template <class obj_pointer, typename memfn_pointer> functor(const obj_pointer& obj_p, memfn_pointer memfn_p);
};

// functor handler
template <class parent_functor, typename fun, typename... Args> class functor_handler
: public functor_impl<typename parent_functor::result_type, Args...> {
public:
	typedef typename parent_functor::result_type result_type;
	
	functor_handler(const fun& func_) : func(func_) {}
	functor_handler* clone() const {
		return new functor_handler(*this);
	}
	
	result_type operator()(Args... args) {
		return func(args...);
	}
	
protected:
	fun func;
};

// member function handler
template <class parent_functor, class obj_pointer, typename memfn_pointer, typename... Args> class mem_fun_handler
: public functor_impl<typename parent_functor::result_type, Args...> {
public:
	typedef typename parent_functor::result_type result_type;
	
	mem_fun_handler(const obj_pointer& obj_p, memfn_pointer memfn_p) : obj(obj_p), memfn(memfn_p) {}
	mem_fun_handler* clone() const {
		return new mem_fun_handler(*this);
	}
	
	result_type operator()(Args... args) {
		return ((*obj).*memfn)(args...);
	}
	
protected:
	obj_pointer obj;
	memfn_pointer memfn;
};

// functor constructor
template <typename return_type, typename... Args>
template <class fun>
functor<return_type, Args...>::functor(const fun& func) : sp_impl(new functor_handler<functor, fun, Args...>(func)) {}

template <typename return_type, typename... Args>
template <class obj_pointer, typename memfn_pointer>
functor<return_type, Args...>::functor(const obj_pointer& obj_p, memfn_pointer memfn_p) : sp_impl(new mem_fun_handler<functor, obj_pointer, memfn_pointer, Args...>(obj_p, memfn_p)) {}

#endif

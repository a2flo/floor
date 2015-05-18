/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2015 Florian Ziesche
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

#ifndef __FLOOR_COMPUTE_DEVICE_OPENCL_ATOMIC_HPP__
#define __FLOOR_COMPUTE_DEVICE_OPENCL_ATOMIC_HPP__

#if defined(FLOOR_COMPUTE_OPENCL)

// NOTE/TODO: opencl/spir 1.x doesn't support float atomics, but opencl 2.0+ does and _might_ be supported via spir 1.2
// alternatively: fallback to CAS with uint32_t reinterpretation

// add
int32_t atomic_add(volatile global int32_t* p, int32_t val);
uint32_t atomic_add(volatile global uint32_t* p, uint32_t val);
int32_t atomic_add(volatile local int32_t* p, int32_t val);
uint32_t atomic_add(volatile local uint32_t* p, uint32_t val);

// sub
int32_t atomic_sub(volatile global int32_t* p, int32_t val);
uint32_t atomic_sub(volatile global uint32_t* p, uint32_t val);
int32_t atomic_sub(volatile local int32_t* p, int32_t val);
uint32_t atomic_sub(volatile local uint32_t* p, uint32_t val);

// inc
int32_t atomic_inc(volatile global int32_t* p);
uint32_t atomic_inc(volatile global uint32_t* p);
int32_t atomic_inc(volatile local int32_t* p);
uint32_t atomic_inc(volatile local uint32_t* p);

// dec
int32_t atomic_dec(volatile global int32_t* p);
uint32_t atomic_dec(volatile global uint32_t* p);
int32_t atomic_dec(volatile local int32_t* p);
uint32_t atomic_dec(volatile local uint32_t* p);

// xchg
int32_t atomic_xchg(volatile global int32_t* p, int32_t val);
uint32_t atomic_xchg(volatile global uint32_t* p, uint32_t val);
float atomic_xchg(volatile global float* p, float val);
int32_t atomic_xchg(volatile local int32_t* p, int32_t val);
uint32_t atomic_xchg(volatile local uint32_t* p, uint32_t val);
float atomic_xchg(volatile local float* p, float val);

// cmpxchg
int32_t atomic_cmpxchg(volatile global int32_t* p, int32_t cmp, int32_t val);
uint32_t atomic_cmpxchg(volatile global uint32_t* p, uint32_t cmp, uint32_t val);
int32_t atomic_cmpxchg(volatile local int32_t* p, int32_t cmp, int32_t val);
uint32_t atomic_cmpxchg(volatile local uint32_t* p, uint32_t cmp, uint32_t val);

// min
int32_t atomic_min(volatile global int32_t* p, int32_t val);
uint32_t atomic_min(volatile global uint32_t* p, uint32_t val);
int32_t atomic_min(volatile local int32_t* p, int32_t val);
uint32_t atomic_min(volatile local uint32_t* p, uint32_t val);

// max
int32_t atomic_max(volatile global int32_t* p, int32_t val);
uint32_t atomic_max(volatile global uint32_t* p, uint32_t val);
int32_t atomic_max(volatile local int32_t* p, int32_t val);
uint32_t atomic_max(volatile local uint32_t* p, uint32_t val);

// and
int32_t atomic_and(volatile global int32_t* p, int32_t val);
uint32_t atomic_and(volatile global uint32_t* p, uint32_t val);
int32_t atomic_and(volatile local int32_t* p, int32_t val);
uint32_t atomic_and(volatile local uint32_t* p, uint32_t val);

// or
int32_t atomic_or(volatile global int32_t* p, int32_t val);
uint32_t atomic_or(volatile global uint32_t* p, uint32_t val);
int32_t atomic_or(volatile local int32_t* p, int32_t val);
uint32_t atomic_or(volatile local uint32_t* p, uint32_t val);

// xor
int32_t atomic_xor(volatile global int32_t* p, int32_t val);
uint32_t atomic_xor(volatile global uint32_t* p, uint32_t val);
int32_t atomic_xor(volatile local int32_t* p, int32_t val);
uint32_t atomic_xor(volatile local uint32_t* p, uint32_t val);

#endif

#endif

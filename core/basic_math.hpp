/*
 *  Flexible OpenCL Rasterizer (oclraster)
 *  Copyright (C) 2012 - 2013 Florian Ziesche
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

#ifndef __FLOOR_BASIC_MATH_HPP__
#define __FLOOR_BASIC_MATH_HPP__

#define PI 3.1415926535897932384626433832795
#define _180DIVPI 57.295779513082322
#define _PIDIV180 0.01745329251994
#define _PIDIV360 0.00872664625997
#define RAD2DEG(rad) (rad * _180DIVPI)
#define DEG2RAD(deg) (deg * _PIDIV180)

#define EPSILON 0.00001f
#define FLOAT_EQ(x, v) ((((v) - EPSILON) < (x)) && ((x) < ((v) + EPSILON)))

#endif

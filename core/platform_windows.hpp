/*
 *  Flo's Open libRary (floor)
 *  Copyright (C) 2004 - 2025 Florian Ziesche
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

// NOTE: this should not be included in a header file
// NOTE: it's recommended to include <floor/core/essentials.hpp> after this for macro cleanup

#pragma once

#if defined(__WINDOWS__)

#if defined(_MSC_VER)
#include <Windows.h>
#include <windef.h>
#include <WinBase.h>
#else
#define byte __windows_byte_workaround
#include <windows.h>
#include <windef.h>
#include <winbase.h>
#undef byte
#endif

#endif // __WINDOWS__

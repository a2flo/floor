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

#include "core.h"

#if !(defined(__clang__) && defined(WIN_UNIXENV))
random_device core::rd;
mt19937 core::gen(core::rd());
#else
mt19937 core::gen;
#endif

/*! converts (projects) a 3d vertex to a 2d screen position
 *  @param v the 3d vertex
 *  @param p the 2d screen position
 */
ipnt core::get_2d_from_3d(const float3& vec, const matrix4f& mview, const matrix4f& mproj, const int4& viewport) {
	// transform vector (*TMVP)
	const float3 mview_vec = vec * mview; // save tmp result, needed later
	float3 proj_vec = mview_vec * mproj;
	
	// check if point is behind cam / "invisible"
	if(mview_vec.z >= 0.0f) {
		return ipnt(numeric_limits<int>::min());
	}
	
	proj_vec *= -1.0f / mview_vec.z;
	
	// and finally: compute window position
	return ipnt((int)(viewport[2] * (proj_vec.x * 0.5f + 0.5f) + viewport[0]),
				(int)(viewport[3] - viewport[3] * (proj_vec.y * 0.5f + 0.5f) + viewport[1])); // flip y
}

float3 core::get_3d_from_2d(const pnt& p, const matrix4f& mview, const matrix4f& mproj, const int4& viewport) {
	const matrix4f ipm((mview * mproj).invert());
	const float3 wnd_vec((((p.x - float(viewport[0])) * 2.0f) / float(viewport[2])) - 1.0f,
						 (((p.y - float(viewport[1])) * 2.0f) / float(viewport[3])) - 1.0f,
						 1.0f);
	return (wnd_vec * ipm);
}

void core::reset(stringstream& sstr) {
	sstr.seekp(0);
	sstr.seekg(0);
	sstr.clear();
	sstr.str("");
}

/*! returns the nearest power of two value of num (only numerical upwards)
 *  @param num the number which next pot value we want to have 
 */
unsigned int core::next_pot(const unsigned int& num) {
	unsigned int tmp = 2;
	for(unsigned int i = 0; i < (sizeof(unsigned int)*8)-1; i++) {
		if(tmp >= num) return tmp;
		tmp <<= 1;
	}
	return 0;
}
unsigned long long int core::next_pot(const unsigned long long int& num) {
	unsigned long long int tmp = 2;
	for(unsigned long long int i = 0; i < (sizeof(unsigned long long int)*8)-1; i++) {
		if(tmp >= num) return tmp;
		tmp <<= 1;
	}
	return 0;
}

string core::find_and_replace(const string& str, const string& find, const string& repl) {
	string ret = str;
	find_and_replace(ret, find, repl);
	return ret;
}

void core::find_and_replace(string& str, const string& find, const string& repl) {
	// consecutive search and replace routine
	size_t pos, old_pos;
	size_t find_len = find.size();
	size_t replace_len = repl.size();
	if(find_len == 0) return; // replace_len might be 0 (if it's an empty string -> "")
	old_pos = 0;
	while((pos = str.find(find, old_pos)) != string::npos) {
		str.replace(pos, find_len, repl.c_str(), replace_len);
		old_pos = pos + replace_len;
	}
}

vector<string> core::tokenize(const string& src, const char& delim) {
	vector<string> dst;
	size_t pos = 0;
	size_t old_pos = 0;
	if(src.find(delim, pos) != string::npos) {
		while((pos = src.find(delim, old_pos)) != string::npos) {
			dst.push_back(src.substr(old_pos, pos-old_pos).c_str());
			old_pos = pos+1;
		}
		dst.push_back(src.substr(old_pos, pos-old_pos).c_str());
	}
	else dst.push_back(src.c_str());
	return dst;
}

void core::str_to_lower_inplace(string& str) {
	transform(str.begin(), str.end(), str.begin(), ptr_fun(::tolower));
}

void core::str_to_upper_inplace(string& str) {
	transform(str.begin(), str.end(), str.begin(), ptr_fun(::toupper));
}

string core::str_to_lower(const string& str) {
	string ret;
	ret.resize(str.length());
	transform(str.begin(), str.end(), ret.begin(), ptr_fun(::tolower));
	return ret;
}

string core::str_to_upper(const string& str) {
	string ret;
	ret.resize(str.length());
	transform(str.begin(), str.end(), ret.begin(), ptr_fun(::toupper));
	return ret;
}

string core::strip_path(const string& in_path) {
	string path = in_path;
	size_t pos = 0, erase_pos;
	while((pos = path.find("../", 0)) != string::npos) {
		erase_pos = path.rfind("/", pos-2);
#if defined(__WINDOWS__)
		if(erase_pos == string::npos) erase_pos = path.rfind("\\", pos-2);
#endif
		if(erase_pos != string::npos) {
			path.erase(erase_pos+1, pos+2-erase_pos);
		}
	}
	
#if defined(__WINDOWS__) // additional windows path handling
	pos = 0;
	while((pos = path.find("..\\", 0)) != string::npos) {
		erase_pos = path.rfind("/", pos-2);
		if(erase_pos == string::npos) erase_pos = path.rfind("\\", pos-2);
		if(erase_pos != string::npos) {
			path.erase(erase_pos+1, pos+2-erase_pos);
		}
	}
#endif
	
	if(path[path.length()-1] != '/' && path[path.length()-1] != '\\') {
		pos = path.rfind("/");
		if(pos == string::npos) pos = path.rfind("\\");
		if(pos == string::npos) {
			path = "/"; // sth is wrong?
		}
		else {
			path = path.substr(0, pos+1);
		}
	}
	
	return path;
}

size_t core::lcm(size_t v1, size_t v2) {
	size_t lcm_ = 1, div = 2;
	while(v1 != 1 || v2 != 1) {
		if((v1 % div) == 0 || (v2 % div) == 0) {
			if((v1 % div) == 0) v1 /= div;
			if((v2 % div) == 0) v2 /= div;
			
			lcm_ *= div;
		}
		else div++;
	}
	return lcm_;
}

size_t core::gcd(size_t v1, size_t v2) {
	return ((v1 * v2) / lcm(v1, v2));
}

string core::trim(const string& str) {
	if(str.length() == 0) return "";
	
	string ret = str;
	size_t pos = 0;
	while(pos < ret.length() && (ret[pos] == ' ' || ret[pos] == '\t' || ret[pos] == '\r' || ret[pos] == '\n')) {
		ret.erase(pos, 1);
	}
	
	if(ret.length() > 0) {
		pos = ret.length()-1;
		while(pos < ret.length() && (ret[pos] == ' ' || ret[pos] == '\t' || ret[pos] == '\r' || ret[pos] == '\n')) {
			ret.erase(pos, 1);
			if(pos > 0) pos--;
		}
	}
	return ret;
}

string core::escape_string(const string& str) {
	if(str.size() == 0) return str;
	
	string ret = str;
	size_t pos = 0;
	for(;;) {
		if((pos = ret.find_first_of("\'\"\\", pos)) == string::npos) break;
		ret.insert(pos, "\\");
		pos += 2;
	}
	return ret;
}

map<string, file_io::FILE_TYPE> core::get_file_list(const string& directory, const string file_extension) {
	map<string, file_io::FILE_TYPE> file_list;
	string dir_str = directory;
	size_t pos;

	// get file list (os specific)
#if defined(__WINDOWS__)
	struct _finddata_t c_file;
	intptr_t h_file;
	
	dir_str += "*";
	if((h_file = _findfirst(dir_str.c_str(), &c_file)) != -1L) {
		do {
			const string name = c_file.name;
			if(file_extension != "") {
				if((pos = name.rfind(".")) != string::npos) {
					if(name.substr(pos+1, name.size()-pos-1) != file_extension) continue;
				}
				else continue; // no extension
			}
			
			if(c_file.attrib & _A_SUBDIR) file_list[name] = file_io::FILE_TYPE::DIR;
			else file_list[name] = file_io::FILE_TYPE::NONE;
		}
		while(_findnext(h_file, &c_file) == 0);
		
		_findclose(h_file);
	}
#else // works under Linux and OS X
	struct dirent** namelist = nullptr;
	
	dir_str += ".";
	int n = scandir(dir_str.c_str(), &namelist, 0, alphasort);
	
	for(int j = 1; j < n; j++) {
		const string name = namelist[j]->d_name;
		if(file_extension != "") {
			if((pos = name.rfind(".")) != string::npos) {
				if(name.substr(pos+1, name.size()-pos-1) != file_extension) continue;
			}
			else continue; // no extension
		}

		// TODO: use sys/stat.h instead (glibc has some issues where DT_DIR is not defined or recursively-self-defined ...)
		// note: 4 == DT_DIR
		if(namelist[j]->d_type == 4) file_list[name] = file_io::FILE_TYPE::DIR;
		else file_list[name] = file_io::FILE_TYPE::NONE;
	}
	
	if(namelist != nullptr) {
		for(int i = 0; i < n; i++) {
			free(namelist[i]);
		}
		free(namelist);
	}
#endif
	
	return file_list;
}

/*! computes the normal of a triangle specified by v1, v2 and v3
 *  @param v1 the first vertex
 *  @param v2 the second vertex
 *  @param v3 the third vertex
 *  @param normal the "output" normal
 */
void core::compute_normal(const float3& v1, const float3& v2, const float3& v3, float3& normal) {
	normal = (v2 - v1) ^ (v3 - v1);
	normal.normalize();
}

/*! computes the normal, binormal and tangent of a triangle
 *! specified by v1, v2 and v3 and their texture coordinates
 *  @param v1 the first vertex
 *  @param v2 the second vertex
 *  @param v3 the third vertex
 *  @param normal the "output" normal
 *  @param binormal the "output" binormal
 *  @param tangent the "output" tangent
 *  @param t1 the first texture coordinate
 *  @param t2 the second texture coordinate
 *  @param t3 the third texture coordinate
 */
void core::compute_normal_tangent_binormal(const float3& v1, const float3& v2, const float3& v3,
										   float3& normal, float3& binormal, float3& tangent,
										   const coord& t1, const coord& t2, const coord& t3) {
	// compute deltas
	float delta_x1 = t2.u - t1.u;
	float delta_y1 = t2.v - t1.v;
	float delta_x2 = t3.u - t1.u;
	float delta_y2 = t3.v - t1.v;
	
	// normal
	float3 edge1(v2 - v1);
	float3 edge2(v3 - v1);
	normal = (edge1 ^ edge2);
	normal.normalize();
	
	// binormal
	binormal = (edge1 * delta_x2) - (edge2 * delta_x1);
	binormal.normalize();
	
	// tangent
	tangent = (edge1 * delta_y2) - (edge2 * delta_y1);
	tangent.normalize();
	
	// adjust
	float3 txb = tangent ^ binormal;
	(normal * txb > 0.0f) ? tangent *= -1.0f : binormal *= -1.0f;
}

void core::system(const string& cmd) {
	string str_dump;
	core::system(cmd, str_dump);
}

void core::system(const string& cmd, string& output) {
	static constexpr size_t buffer_size = 8192;
	char buffer[buffer_size+1];
	memset(&buffer, 0, buffer_size+1);
	
	FILE* sys_pipe = popen(cmd.c_str(), "r");
	while(fgets(buffer, buffer_size, sys_pipe) != nullptr) {
		output += buffer;
		memset(buffer, 0, buffer_size); // size+1 is always 0
	}
	pclose(sys_pipe);
}

int core::rand(const int& max) {
	uniform_int_distribution<> dist(0, max-1);
	return dist(gen);
}

int core::rand(const int& min, const int& max) {
	uniform_int_distribution<> dist(min, max-1);
	return dist(gen);
}

float core::rand(const float& max) {
	uniform_real_distribution<> dist(0.0f, max);
	return dist(gen);
}

float core::rand(const float& min, const float& max) {
	uniform_real_distribution<> dist(min, max);
	return dist(gen);
}

void core::set_random_seed(const unsigned int& seed) {
	gen.seed(seed);
}

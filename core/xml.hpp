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

#ifndef __FLOOR_XML_HPP__
#define __FLOOR_XML_HPP__

#include <floor/core/platform.hpp>
#include <floor/core/core.hpp>

#include <libxml/xmlreader.h>
#include <libxml/xmlwriter.h>

//! xml functions
class xml {
public:
	xml();
	~xml();

	// new, easy xml document handling
	// TODO: continuous alloc?
	struct xml_node {
		xml_node(const xmlNode* node, const string name);
		xml_node(const xmlNode* node);
		~xml_node();
		const string node_name;
		const string node_content;
		vector<pair<string, xml_node*>> children;
		unordered_map<string, string> attributes;
		
		const string& name() const { return node_name; }
		const string& content() const { return node_content; }
		const string& operator[](const string& attr_name) const;
		bool set(const string& attr_name, const string& value) {
			const auto attr = attributes.find(attr_name);
			if(attr == attributes.end()) return false;
			attr->second = value;
			return true;
		}
	};
	struct xml_doc {
	private:
		template<typename T> class default_value {
		public: static T def() { return T(); }
		};
		const string& extract_attr(const string& path) const;
		bool set_attr(const string& path, const string& value);
		
	public:
		xml_doc() = default;
		xml_doc(xml_doc&) = delete;
		xml_doc(xml_doc&&) = default;
		xml_doc& operator=(xml_doc&) = delete;
		xml_doc& operator=(xml_doc&&) = default;
		vector<pair<string, xml_node*>> nodes;
		bool valid { true };
		
		xml_node* get_node(const string& path) const;
		
		//! "root.subnode.subnode.attr"
		template<typename T> T get(const string& path, const T default_val = default_value<T>::def()) const;
		template<typename T> bool set(const string& path, const T& value);
	};
	xml_doc process_file(const string& filename,
#if !defined(FLOOR_IOS) && !defined(__WINDOWS__)
						 const bool validate = true
#else
						 const bool validate = false
#endif
						 ) const;
	xml_doc process_data(const string& data,
#if !defined(FLOOR_IOS) && !defined(__WINDOWS__)
						 const bool validate = true
#else
						 const bool validate = false
#endif
						 ) const;
	
	bool save_file(const xml_doc& doc, const string& filename, const string doc_type) const;

	// for manual reading
	template <typename T> T get_attribute(const xmlAttribute* attr, const char* attr_name) {
		for(const xmlAttr* cur_attr = (const xmlAttr*)attr; cur_attr; cur_attr = (const xmlAttr*)cur_attr->next) {
			if(strcmp(attr_name, (const char*)cur_attr->name) == 0) {
				return converter<string, T>::convert(string((const char*)cur_attr->children->content));
			}
		}
		log_error("element has no attribute named %s!", attr_name);
		return (T)nullptr;
	}
	bool is_attribute(const xmlAttribute* attr, const char* attr_name) {
		for(const xmlAttr* cur_attr = (const xmlAttr*)attr; cur_attr; cur_attr = (const xmlAttr*)cur_attr->next) {
			if(strcmp(attr_name, (const char*)cur_attr->name) == 0) {
				return true;
			}
		}
		return false;
	}

protected:
	void create_node_structure(xml_doc& doc, xmlDocPtr xmldoc) const;
	
	bool write_node(const xml_node& node, xmlTextWriterPtr* writer, const string& filename, size_t& tabs, bool& first_node) const;

};

//
template<> class xml::xml_doc::default_value<string> {
public: static string def() { return ""; }
};
template<> class xml::xml_doc::default_value<float> {
public: static float def() { return 0.0f; }
};
template<> class xml::xml_doc::default_value<size_t> {
public: static size_t def() { return 0; }
};
template<> class xml::xml_doc::default_value<ssize_t> {
public: static ssize_t def() { return 0; }
};
template<> class xml::xml_doc::default_value<bool> {
public: static bool def() { return false; }
};
template<> class xml::xml_doc::default_value<float2> {
public: static float2 def() { return float2(0.0f); }
};
template<> class xml::xml_doc::default_value<float3> {
public: static float3 def() { return float3(0.0f); }
};
template<> class xml::xml_doc::default_value<float4> {
public: static float4 def() { return float4(0.0f); }
};
template<> class xml::xml_doc::default_value<size2> {
public: static size2 def() { return size2(0, 0); }
};
template<> class xml::xml_doc::default_value<size3> {
public: static size3 def() { return size3(0, 0, 0); }
};
template<> class xml::xml_doc::default_value<size4> {
public: static size4 def() { return size4(0, 0, 0, 0); }
};
template<> class xml::xml_doc::default_value<ssize2> {
public: static ssize2 def() { return ssize2(0, 0); }
};
template<> class xml::xml_doc::default_value<ssize3> {
public: static ssize3 def() { return ssize3(0, 0, 0); }
};
template<> class xml::xml_doc::default_value<ssize4> {
public: static ssize4 def() { return ssize4(0, 0, 0, 0); }
};

template<> string xml::xml_doc::get<string>(const string& path, const string default_value) const;
template<> float xml::xml_doc::get<float>(const string& path, const float default_value) const;
template<> size_t xml::xml_doc::get<size_t>(const string& path, const size_t default_value) const;
template<> ssize_t xml::xml_doc::get<ssize_t>(const string& path, const ssize_t default_value) const;
template<> bool xml::xml_doc::get<bool>(const string& path, const bool default_value) const;
template<> float2 xml::xml_doc::get<float2>(const string& path, const float2 default_value) const;
template<> float3 xml::xml_doc::get<float3>(const string& path, const float3 default_value) const;
template<> float4 xml::xml_doc::get<float4>(const string& path, const float4 default_value) const;
template<> size2 xml::xml_doc::get<size2>(const string& path, const size2 default_value) const;
template<> size3 xml::xml_doc::get<size3>(const string& path, const size3 default_value) const;
template<> size4 xml::xml_doc::get<size4>(const string& path, const size4 default_value) const;
template<> ssize2 xml::xml_doc::get<ssize2>(const string& path, const ssize2 default_value) const;
template<> ssize3 xml::xml_doc::get<ssize3>(const string& path, const ssize3 default_value) const;
template<> ssize4 xml::xml_doc::get<ssize4>(const string& path, const ssize4 default_value) const;

template<> bool xml::xml_doc::set<string>(const string& path, const string& value);
template<> bool xml::xml_doc::set<float>(const string& path, const float& value);
template<> bool xml::xml_doc::set<size_t>(const string& path, const size_t& value);
template<> bool xml::xml_doc::set<ssize_t>(const string& path, const ssize_t& value);
template<> bool xml::xml_doc::set<bool>(const string& path, const bool& value);
template<> bool xml::xml_doc::set<float2>(const string& path, const float2& value);
template<> bool xml::xml_doc::set<float3>(const string& path, const float3& value);
template<> bool xml::xml_doc::set<float4>(const string& path, const float4& value);
template<> bool xml::xml_doc::set<size2>(const string& path, const size2& value);
template<> bool xml::xml_doc::set<size3>(const string& path, const size3& value);
template<> bool xml::xml_doc::set<size4>(const string& path, const size4& value);
template<> bool xml::xml_doc::set<ssize2>(const string& path, const ssize2& value);
template<> bool xml::xml_doc::set<ssize3>(const string& path, const ssize3& value);
template<> bool xml::xml_doc::set<ssize4>(const string& path, const ssize4& value);

#endif

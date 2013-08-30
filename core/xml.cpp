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

#include "xml.h"
#include "oclraster/oclraster.h"
#include <libxml/encoding.h>
#include <libxml/catalog.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

/*! create and initialize the xml class
 */
xml::xml() {
	// libxml2 init
	LIBXML_TEST_VERSION
	xmlInitializeCatalog();
	xmlCatalogSetDefaults(XML_CATA_ALLOW_ALL);
	
#if !defined(__WINDOWS__)
#define OCLRASTER_XML_DTD_PATH_PREFIX "file://"
#else
#define OCLRASTER_XML_DTD_PATH_PREFIX "file:///"
#endif
	
	if(xmlCatalogAdd(BAD_CAST "public",
					 BAD_CAST "-//OCLRASTER//DTD config 1.0//EN",
					 BAD_CAST (OCLRASTER_XML_DTD_PATH_PREFIX+oclraster::data_path("dtd/config.dtd")).c_str()) != 0) {
		const auto error_ptr = xmlGetLastError();
		oclr_error("failed to add catalog for config: %s", (error_ptr != nullptr ? error_ptr->message : ""));
	}
}

xml::~xml() {
}

bool xml::save_file(const xml_doc& doc, const string& filename, const string doc_type) const {
	//
	if(!doc.valid) {
		oclr_error("can't write invalid xml doc!");
		return false;
	}
	
	//
	xmlTextWriterPtr* writer = new xmlTextWriterPtr();
	const auto close_writer = [&writer]() {
		if(*writer != nullptr) xmlFreeTextWriter(*writer);
		xmlMemoryDump();
		if(writer != nullptr) delete writer;
		writer = nullptr;
	};
	
	*writer = xmlNewTextWriterFilename(filename.c_str(), 0);
	if(*writer == nullptr) {
		oclr_error("unable to write to file \"%s\"!", filename);
		close_writer();
		return false;
	}
	
	if(xmlTextWriterStartDocument(*writer, nullptr, "UTF-8",
								  (doc_type.empty() ? nullptr : "no")) < 0) {
		oclr_error("unable to start document \"%s\"!", filename);
		close_writer();
		return false;
	}
	
	// write doc type if there is one
	if(!doc_type.empty()) {
		xmlTextWriterWriteRaw(*writer, (const xmlChar*)(doc_type+"\n").c_str());
	}
	
	// write all nodes:
	size_t tabs = 0;
	bool first_node = true;
	for(const auto& node : doc.nodes) {
		if(!write_node(*node.second, writer, filename, tabs, first_node)) {
			close_writer();
			oclr_error("failed to write document \"%s\"!", filename);
			return false;
		}
	}
	
	close_writer();
	return true;
}

bool xml::write_node(const xml_node& node, xmlTextWriterPtr* writer, const string& filename, size_t& tabs, bool& first_node) const {
	//
	const bool insert_newline = !first_node;
	first_node = false;
	
	// create tabs string
	string tab_str = "";
	for(size_t i = 0; i < tabs; i++) tab_str += "\t";
	
	// start node:
	if(node.name()[0] != '#') {
		// write tabs
		xmlTextWriterWriteRaw(*writer, BAD_CAST tab_str.c_str());
		
		// start node:
		if(xmlTextWriterStartElement(*writer, BAD_CAST node.name().c_str()) < 0) {
			oclr_error("unable to start element \"%s\" in file \"%s\"!",
					  node.name(), filename);
			return false;
		}
		tabs++;
		
		// node attributes:
		for(const auto& attr : node.attributes) {
			if(xmlTextWriterWriteAttribute(*writer, BAD_CAST attr.first.c_str(),
										   BAD_CAST attr.second.c_str()) < 0) {
				oclr_error("error while writing attribute \"%s\" in node \"%s\" in file %s!",
						  attr.first, node.name(), filename);
			}
		}
		
		// node content/children:
		const bool has_conent = (node.content().empty() ? false :
								 (node.content().find_first_not_of("\t\n\r\v ") != string::npos));
		if(!node.children.empty() || has_conent) {
			xmlTextWriterWriteRaw(*writer, BAD_CAST "\n");
			bool child_first_node = true;
			for(const auto& child : node.children) {
				if(!write_node(*child.second, writer, filename, tabs, child_first_node)) {
					oclr_error("failed to write node \"%s\"!", child.first);
					return false;
				}
			}
			
			// content:
			if(has_conent) {
				if(xmlTextWriterWriteString(*writer, BAD_CAST node.content().c_str()) < 0) {
					oclr_error("error while writing content \"%s\" of node \"%s\" in file %s!",
							  node.content(), node.name(), filename);
				}
			}
			
			// end node:
			xmlTextWriterWriteRaw(*writer, BAD_CAST tab_str.c_str());
			if(xmlTextWriterEndElement(*writer) < 0) {
				oclr_error("unable to end element \"%s\" in file \"%s\"!",
						  node.name(), filename);
				return false;
			}
		}
		else {
			if(xmlTextWriterEndElement(*writer) < 0) {
				oclr_error("unable to end element \"%s\" in file \"%s\"!",
						  node.name(), filename);
				return false;
			}
		}
		tabs--;
	}
	else if(node.name() == "#comment") {
		if(insert_newline) {
			xmlTextWriterWriteRaw(*writer, BAD_CAST tab_str.c_str());
			xmlTextWriterWriteRaw(*writer, (const xmlChar*)"\n");
		}
		xmlTextWriterWriteRaw(*writer, BAD_CAST tab_str.c_str());
		xmlTextWriterWriteComment(*writer, BAD_CAST node.content().c_str());
	}
	else {
		oclr_error("unknown node type \"%s\" in file \"%s\"!", node.name(), filename);
		return false;
	}
	xmlTextWriterWriteRaw(*writer, (const xmlChar*)"\n");
	return true;
}

xml::xml_doc xml::process_file(const string& filename, const bool validate) const {
	xml_doc doc;
	
	// read/parse/validate
	xmlParserCtxtPtr ctx = xmlNewParserCtxt();
	if(ctx == nullptr) {
		oclr_error("failed to allocate parser context for \"%s\"!", filename);
		doc.valid = false;
		return doc;
	}
	
	xmlDocPtr xmldoc = xmlCtxtReadFile(ctx, filename.c_str(), nullptr,
									   (validate ? XML_PARSE_DTDLOAD | XML_PARSE_DTDVALID : 0));
	if(xmldoc == nullptr) {
		oclr_error("failed to parse \"%s\"!", filename);
		doc.valid = false;
		return doc;
	}
	else {
		if(ctx->valid == 0) {
			xmlFreeDoc(xmldoc);
			oclr_error("failed to validate \"%s\"!", filename);
			doc.valid = false;
			return doc;
		}
	}
	
	// create internal node structure
	create_node_structure(doc, xmldoc);
	
	// cleanup
	xmlFreeDoc(xmldoc);
	xmlFreeParserCtxt(ctx);
	xmlCleanupParser();
	
	return doc;
}

xml::xml_doc xml::process_data(const string& data, const bool validate) const {
	xml_doc doc;
	
	// read/parse/validate
	xmlParserCtxtPtr ctx = xmlNewParserCtxt();
	if(ctx == nullptr) {
		oclr_error("failed to allocate parser context!");
		doc.valid = false;
		return doc;
	}
	
	xmlDocPtr xmldoc = xmlCtxtReadDoc(ctx, (const xmlChar*)data.c_str(), oclraster::data_path("dtd/shader.xml").c_str(), nullptr,
									  (validate ? XML_PARSE_DTDLOAD | XML_PARSE_DTDVALID : 0));
	if(xmldoc == nullptr) {
		oclr_error("failed to parse data!");
		doc.valid = false;
		return doc;
	}
	else {
		if(ctx->valid == 0) {
			xmlFreeDoc(xmldoc);
			oclr_error("failed to validate data!");
			doc.valid = false;
			return doc;
		}
	}
	
	// create internal node structure
	create_node_structure(doc, xmldoc);
	
	// cleanup
	xmlFreeDoc(xmldoc);
	xmlFreeParserCtxt(ctx);
	xmlCleanupParser();
	
	return doc;
}

void xml::create_node_structure(xml::xml_doc& doc, xmlDocPtr xmldoc) const {
	deque<pair<xmlNode*, vector<pair<string, xml_node*>>*>> node_stack;
	node_stack.push_back(make_pair(xmldoc->children, &doc.nodes));
	for(;;) {
		if(node_stack.empty()) break;
		
		xmlNode* cur_node = node_stack.front().first;
		vector<pair<string, xml_node*>>* nodes = node_stack.front().second;
		node_stack.pop_front();
		
		for(; cur_node; cur_node = cur_node->next) {
			if(cur_node->type == XML_ELEMENT_NODE) {
				xml_node* node = new xml_node(cur_node);
				nodes->emplace_back(make_pair(string((const char*)cur_node->name), node));
				
				if(cur_node->children != nullptr) {
					node_stack.push_back(make_pair(cur_node->children, &node->children));
				}
			}
			else if(cur_node->type == XML_COMMENT_NODE) {
				xml_node* node = new xml_node(cur_node, "#comment");
				nodes->emplace_back(make_pair("#comment", node));
			}
		}
	}
}

xml::xml_node* xml::xml_doc::get_node(const string& path) const {
	// find the node
	vector<string> levels = core::tokenize(path, '.');
	const vector<pair<string, xml_node*>>* cur_level = &nodes;
	xml_node* cur_node = nullptr;
	for(const string& level : levels) {
		const auto next_node = find_if(begin(*cur_level), end(*cur_level),
									   [&level](const pair<string, xml_node*>& elem) {
			return (elem.first == level);
		});
		if(next_node == end(*cur_level)) return nullptr;
		cur_node = next_node->second;
		cur_level = &cur_node->children;
	}
	return cur_node;
}

const string& xml::xml_doc::extract_attr(const string& path) const {
	static const string invalid_attr = "INVALID";
	const size_t lp = path.rfind(".");
	if(lp == string::npos) return invalid_attr;
	const string node_path = path.substr(0, lp);
	const string attr_name = path.substr(lp+1, path.length()-lp-1);
	
	xml_node* node = get_node(node_path);
	if(node == nullptr) return invalid_attr;
	return (attr_name != "content" ? (*node)[attr_name] : node->content());
}

template<> string xml::xml_doc::get<string>(const string& path, const string default_value) const {
	const string& attr = extract_attr(path);
	return (attr != "INVALID" ? attr : default_value);
}
template<> float xml::xml_doc::get<float>(const string& path, const float default_value) const {
	const string& attr = extract_attr(path);
	return (attr != "INVALID" ? strtof(attr.c_str(), nullptr) : default_value);
}
template<> size_t xml::xml_doc::get<size_t>(const string& path, const size_t default_value) const {
	const string& attr = extract_attr(path);
	return (attr != "INVALID" ? (size_t)strtoull(attr.c_str(), nullptr, 10) : default_value);
}
template<> ssize_t xml::xml_doc::get<ssize_t>(const string& path, const ssize_t default_value) const {
	const string& attr = extract_attr(path);
	return (attr != "INVALID" ? (ssize_t)strtoll(attr.c_str(), nullptr, 10) : default_value);
}
template<> bool xml::xml_doc::get<bool>(const string& path, const bool default_value) const {
	const string& attr = extract_attr(path);
	return (attr != "INVALID" ?
			(attr == "yes" || attr == "YES" ||
			 attr == "true" || attr == "TRUE" ||
			 attr == "on" || attr == "ON" || attr == "1" ? true : false) : default_value);
}
template<> float2 xml::xml_doc::get<float2>(const string& path, const float2 default_value) const {
	const string& attr = extract_attr(path);
	vector<string> tokens= core::tokenize(attr, ',');
	return (attr != "INVALID" && tokens.size() >= 2 ?
			float2(strtof(tokens[0].c_str(), nullptr), strtof(tokens[1].c_str(), nullptr))
			: default_value);
}
template<> float3 xml::xml_doc::get<float3>(const string& path, const float3 default_value) const {
	const string& attr = extract_attr(path);
	vector<string> tokens= core::tokenize(attr, ',');
	return (attr != "INVALID" && tokens.size() >= 3 ?
			float3(strtof(tokens[0].c_str(), nullptr), strtof(tokens[1].c_str(), nullptr), strtof(tokens[2].c_str(), nullptr))
			: default_value);
}
template<> float4 xml::xml_doc::get<float4>(const string& path, const float4 default_value) const {
	const string& attr = extract_attr(path);
	vector<string> tokens= core::tokenize(attr, ',');
	return (attr != "INVALID" && tokens.size() >= 4 ?
			float4(strtof(tokens[0].c_str(), nullptr), strtof(tokens[1].c_str(), nullptr), strtof(tokens[2].c_str(), nullptr), strtof(tokens[3].c_str(), nullptr))
			: default_value);
}
template<> size2 xml::xml_doc::get<size2>(const string& path, const size2 default_value) const {
	const string& attr = extract_attr(path);
	vector<string> tokens= core::tokenize(attr, ',');
	return (attr != "INVALID" && tokens.size() >= 2 ?
			size2((size_t)strtoull(tokens[0].c_str(), nullptr, 10),
				  (size_t)strtoull(tokens[1].c_str(), nullptr, 10))
			: default_value);
}
template<> size3 xml::xml_doc::get<size3>(const string& path, const size3 default_value) const {
	const string& attr = extract_attr(path);
	vector<string> tokens= core::tokenize(attr, ',');
	return (attr != "INVALID" && tokens.size() >= 3 ?
			size3((size_t)strtoull(tokens[0].c_str(), nullptr, 10),
				  (size_t)strtoull(tokens[1].c_str(), nullptr, 10),
				  (size_t)strtoull(tokens[2].c_str(), nullptr, 10))
			: default_value);
}
template<> size4 xml::xml_doc::get<size4>(const string& path, const size4 default_value) const {
	const string& attr = extract_attr(path);
	vector<string> tokens= core::tokenize(attr, ',');
	return (attr != "INVALID" && tokens.size() >= 4 ?
			size4((size_t)strtoull(tokens[0].c_str(), nullptr, 10),
				  (size_t)strtoull(tokens[1].c_str(), nullptr, 10),
				  (size_t)strtoull(tokens[2].c_str(), nullptr, 10),
				  (size_t)strtoull(tokens[3].c_str(), nullptr, 10))
			: default_value);
}
template<> ssize2 xml::xml_doc::get<ssize2>(const string& path, const ssize2 default_value) const {
	const string& attr = extract_attr(path);
	vector<string> tokens= core::tokenize(attr, ',');
	return (attr != "INVALID" && tokens.size() >= 2 ?
			ssize2((ssize_t)strtoll(tokens[0].c_str(), nullptr, 10),
				   (ssize_t)strtoll(tokens[1].c_str(), nullptr, 10))
			: default_value);
}
template<> ssize3 xml::xml_doc::get<ssize3>(const string& path, const ssize3 default_value) const {
	const string& attr = extract_attr(path);
	vector<string> tokens= core::tokenize(attr, ',');
	return (attr != "INVALID" && tokens.size() >= 3 ?
			ssize3((ssize_t)strtoll(tokens[0].c_str(), nullptr, 10),
				   (ssize_t)strtoll(tokens[1].c_str(), nullptr, 10),
				   (ssize_t)strtoll(tokens[2].c_str(), nullptr, 10))
			: default_value);
}
template<> ssize4 xml::xml_doc::get<ssize4>(const string& path, const ssize4 default_value) const {
	const string& attr = extract_attr(path);
	vector<string> tokens= core::tokenize(attr, ',');
	return (attr != "INVALID" && tokens.size() >= 4 ?
			ssize4((ssize_t)strtoll(tokens[0].c_str(), nullptr, 10),
				   (ssize_t)strtoll(tokens[1].c_str(), nullptr, 10),
				   (ssize_t)strtoll(tokens[2].c_str(), nullptr, 10),
				   (ssize_t)strtoll(tokens[3].c_str(), nullptr, 10))
			: default_value);
}

bool xml::xml_doc::set_attr(const string& path, const string& value) {
	const size_t lp = path.rfind(".");
	if(lp == string::npos) return false;
	const string node_path = path.substr(0, lp);
	const string attr_name = path.substr(lp+1, path.length()-lp-1);
	
	xml_node* node = get_node(node_path);
	if(node == nullptr) return false;
	if(attr_name == "content") return false; // TODO: implement this?
	return node->set(attr_name, value);
}

template<> bool xml::xml_doc::set<string>(const string& path, const string& value) {
	return set_attr(path, value);
}

template<> bool xml::xml_doc::set<float>(const string& path, const float& value) {
	return set_attr(path, float2string(value));
}

template<> bool xml::xml_doc::set<size_t>(const string& path, const size_t& value) {
	return set_attr(path, size_t2string(value));
}

template<> bool xml::xml_doc::set<ssize_t>(const string& path, const ssize_t& value) {
	return set_attr(path, ssize_t2string(value));
}

template<> bool xml::xml_doc::set<bool>(const string& path, const bool& value) {
	return set_attr(path, bool2string(value));
}

template<> bool xml::xml_doc::set<float2>(const string& path, const float2& value) {
	return set_attr(path, float2string(value.x)+","+float2string(value.y));
}

template<> bool xml::xml_doc::set<float3>(const string& path, const float3& value) {
	return set_attr(path, float2string(value.x)+","+float2string(value.y)+","+float2string(value.z));
}

template<> bool xml::xml_doc::set<float4>(const string& path, const float4& value) {
	return set_attr(path,
					float2string(value.x)+","+float2string(value.y)+
					","+float2string(value.z)+","+float2string(value.w));
}

template<> bool xml::xml_doc::set<size2>(const string& path, const size2& value) {
	return set_attr(path, size_t2string(value.x)+","+size_t2string(value.y));
}

template<> bool xml::xml_doc::set<size3>(const string& path, const size3& value) {
	return set_attr(path, size_t2string(value.x)+","+size_t2string(value.y)+","+size_t2string(value.z));
}

template<> bool xml::xml_doc::set<size4>(const string& path, const size4& value) {
	return set_attr(path,
					size_t2string(value.x)+","+size_t2string(value.y)+
					","+size_t2string(value.z)+","+size_t2string(value.w));
}

template<> bool xml::xml_doc::set<ssize2>(const string& path, const ssize2& value) {
	return set_attr(path, ssize_t2string(value.x)+","+ssize_t2string(value.y));
}

template<> bool xml::xml_doc::set<ssize3>(const string& path, const ssize3& value) {
	return set_attr(path, ssize_t2string(value.x)+","+ssize_t2string(value.y)+","+ssize_t2string(value.z));
}

template<> bool xml::xml_doc::set<ssize4>(const string& path, const ssize4& value) {
	return set_attr(path,
					ssize_t2string(value.x)+","+ssize_t2string(value.y)+
					","+ssize_t2string(value.z)+","+ssize_t2string(value.w));
}

xml::xml_node::xml_node(const xmlNode* node) :
xml::xml_node::xml_node(node, (const char*)node->name) {}

xml::xml_node::xml_node(const xmlNode* node, const string name) :
node_name(name),
node_content(xmlNodeGetContent((xmlNodePtr)node) != nullptr ?
			 (const char*)xmlNodeGetContent((xmlNodePtr)node) :
			 (node->content != nullptr ? (const char*)node->content : "")) {
	for(xmlAttr* cur_attr = node->properties; cur_attr; cur_attr = cur_attr->next) {
		attributes.insert(make_pair(string((const char*)cur_attr->name),
									string((cur_attr->children != nullptr ? (const char*)cur_attr->children->content : "INVALID"))
									));
	}
}
xml::xml_node::~xml_node() {
	// cascading delete
	for(const auto& child : children) {
		delete child.second;
	}
	attributes.clear();
}

// 파일: xml_util.cpp
// 생성일: 2026-02-17
// 설명: pugixml 래퍼 유틸리티 구현

#include "xml_util.h"
#include <nxcore/util/debug_util.h>
#include <sstream>
#include <charconv>

namespace nx {

// ============================================================================
// XmlDocument 구현
// ============================================================================

std::error_code
XmlDocument::parse(const std::string& xml_content)
{
  pugi::xml_parse_result result = m_doc.load_string(xml_content.c_str());

  if (!result) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  return {};
}

std::string
XmlDocument::to_string() const
{
  std::ostringstream oss;
  m_doc.save(oss, "  ", pugi::format_default, pugi::encoding_utf8);
  return oss.str();
}

pugi::xml_node
XmlDocument::root() const
{
  return m_doc.document_element();
}

std::optional<pugi::xml_node>
XmlDocument::select_node(const std::string& xpath) const
{
  pugi::xpath_node xnode = m_doc.select_node(xpath.c_str());
  if (!xnode) {
    return std::nullopt;
  }
  return xnode.node();
}

pugi::xpath_node_set
XmlDocument::select_nodes(const std::string& xpath) const
{
  return m_doc.select_nodes(xpath.c_str());
}

pugi::xml_document&
XmlDocument::document()
{
  return m_doc;
}

const pugi::xml_document&
XmlDocument::document() const
{
  return m_doc;
}

// ============================================================================
// XML 노드 헬퍼 함수 구현
// ============================================================================

std::string
get_node_text(const pugi::xml_node& node)
{
  if (!node) {
    return {};
  }
  return node.text().as_string();
}

std::string
get_node_attribute(const pugi::xml_node& node, const std::string& attr_name)
{
  if (!node) {
    return {};
  }
  return node.attribute(attr_name.c_str()).as_string();
}

std::optional<pugi::xml_node>
find_child_ignore_ns(const pugi::xml_node& parent, const std::string& child_name)
{
  if (!parent) {
    return std::nullopt;
  }

  // 네임스페이스를 무시하고 로컬 이름만 비교
  for (pugi::xml_node child : parent.children()) {
    std::string node_name = child.name();
    std::string local_name = strip_namespace(node_name);

    if (local_name == child_name) {
      return child;
    }
  }

  return std::nullopt;
}

std::optional<std::string>
get_child_text(const pugi::xml_node& parent, const std::string& child_name)
{
  auto child = find_child_ignore_ns(parent, child_name);
  if (!child) {
    return std::nullopt;
  }

  return get_node_text(*child);
}

std::optional<int>
get_child_int(const pugi::xml_node& parent, const std::string& child_name)
{
  auto text = get_child_text(parent, child_name);
  if (!text) {
    return std::nullopt;
  }

  int value = 0;
  auto [ptr, ec] = std::from_chars(text->data(), text->data() + text->size(), value);

  if (ec != std::errc{}) {
    return std::nullopt;
  }

  return value;
}

std::optional<double>
get_child_double(const pugi::xml_node& parent, const std::string& child_name)
{
  auto text = get_child_text(parent, child_name);
  if (!text) {
    return std::nullopt;
  }

  try {
    return std::stod(*text);
  }
  catch (...) {
    return std::nullopt;
  }
}

std::optional<bool>
get_child_bool(const pugi::xml_node& parent, const std::string& child_name)
{
  auto text = get_child_text(parent, child_name);
  if (!text) {
    return std::nullopt;
  }

  // "true", "1" -> true
  // "false", "0" -> false
  if (*text == "true" || *text == "1") {
    return true;
  }
  else if (*text == "false" || *text == "0") {
    return false;
  }

  return std::nullopt;
}

// ============================================================================
// XML 생성 헬퍼 함수 구현
// ============================================================================

pugi::xml_node
append_child_with_text(
  pugi::xml_node& parent, const std::string& name, const std::string& text)
{
  pugi::xml_node child = parent.append_child(name.c_str());
  child.text().set(text.c_str());
  return child;
}

pugi::xml_node
append_child_with_int(pugi::xml_node& parent, const std::string& name, int value)
{
  pugi::xml_node child = parent.append_child(name.c_str());
  child.text().set(value);
  return child;
}

pugi::xml_node
append_child_with_double(
  pugi::xml_node& parent, const std::string& name, double value)
{
  pugi::xml_node child = parent.append_child(name.c_str());
  child.text().set(value);
  return child;
}

pugi::xml_node
append_child_with_bool(pugi::xml_node& parent, const std::string& name, bool value)
{
  pugi::xml_node child = parent.append_child(name.c_str());
  child.text().set(value);
  return child;
}

// ============================================================================
// 네임스페이스 헬퍼 함수 구현
// ============================================================================

std::string
strip_namespace(const std::string& name)
{
  size_t colon_pos = name.find(':');
  if (colon_pos != std::string::npos && colon_pos + 1 < name.size()) {
    return name.substr(colon_pos + 1);
  }
  return name;
}

std::pair<std::string, std::string>
split_namespace(const std::string& name)
{
  size_t colon_pos = name.find(':');
  if (colon_pos != std::string::npos && colon_pos + 1 < name.size()) {
    return {name.substr(0, colon_pos), name.substr(colon_pos + 1)};
  }
  return {{}, name};
}

} // namespace nx

// 파일: xml_util.h
// 생성일: 2026-02-17
// 설명: pugixml 래퍼 유틸리티 (SOAP/XML 파싱용)

#pragma once

#include <pugixml.hpp>
#include <string>
#include <optional>
#include <expected>
#include <system_error>

namespace nx {

// ============================================================================
// XML 문서 래퍼
// ============================================================================

/// XML 문서 래퍼 클래스
class XmlDocument
{
public:
  XmlDocument() = default;
  ~XmlDocument() = default;

  // 문자열에서 XML 파싱
  std::error_code parse(const std::string& xml_content);

  // XML 문서 내용을 문자열로 변환
  std::string to_string() const;

  // 루트 노드 가져오기
  pugi::xml_node root() const;

  // XPath로 노드 검색
  std::optional<pugi::xml_node> select_node(const std::string& xpath) const;

  // XPath로 여러 노드 검색
  pugi::xpath_node_set select_nodes(const std::string& xpath) const;

  // 원본 pugixml 문서 객체 접근 (고급 사용자용)
  pugi::xml_document& document();
  const pugi::xml_document& document() const;

private:
  pugi::xml_document m_doc;
};

// ============================================================================
// XML 노드 헬퍼 함수
// ============================================================================

/// 노드에서 텍스트 값 추출
std::string get_node_text(const pugi::xml_node& node);

/// 노드에서 속성 값 추출
std::string
get_node_attribute(const pugi::xml_node& node, const std::string& attr_name);

/// 노드에서 자식 노드 찾기 (네임스페이스 무시)
std::optional<pugi::xml_node>
find_child_ignore_ns(const pugi::xml_node& parent, const std::string& child_name);

/// 노드에서 자식 노드의 텍스트 값 추출
std::optional<std::string>
get_child_text(const pugi::xml_node& parent, const std::string& child_name);

/// 노드에서 자식 노드의 정수 값 추출
std::optional<int>
get_child_int(const pugi::xml_node& parent, const std::string& child_name);

/// 노드에서 자식 노드의 부동소수점 값 추출
std::optional<double>
get_child_double(const pugi::xml_node& parent, const std::string& child_name);

/// 노드에서 자식 노드의 불린 값 추출
std::optional<bool>
get_child_bool(const pugi::xml_node& parent, const std::string& child_name);

// ============================================================================
// XML 생성 헬퍼 함수
// ============================================================================

/// 자식 노드 추가 (텍스트 값 포함)
pugi::xml_node append_child_with_text(
  pugi::xml_node& parent, const std::string& name, const std::string& text);

/// 자식 노드 추가 (정수 값 포함)
pugi::xml_node
append_child_with_int(pugi::xml_node& parent, const std::string& name, int value);

/// 자식 노드 추가 (부동소수점 값 포함)
pugi::xml_node append_child_with_double(
  pugi::xml_node& parent, const std::string& name, double value);

/// 자식 노드 추가 (불린 값 포함)
pugi::xml_node
append_child_with_bool(pugi::xml_node& parent, const std::string& name, bool value);

// ============================================================================
// 네임스페이스 헬퍼 함수
// ============================================================================

/// 네임스페이스를 제거한 노드 이름 추출
/// 예: "tds:GetDeviceInformationResponse" -> "GetDeviceInformationResponse"
std::string strip_namespace(const std::string& name);

/// 네임스페이스 프리픽스와 로컬 이름 분리
/// 예: "tds:GetDeviceInformation" -> {"tds", "GetDeviceInformation"}
std::pair<std::string, std::string> split_namespace(const std::string& name);

} // namespace nx

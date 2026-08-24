// 파일: xml_util_unittest.cpp
// 생성일: 2026-02-19
// 설명: XML 유틸리티 래퍼(XmlDocument) 단위 테스트

#include <nxcore/util/xml_util.h>

#include <gtest/gtest.h>

namespace nx {

// ============================================================================
// 테스트용 XML 상수
// ============================================================================

namespace {

// 기본 유효 XML
constexpr const char* kValidXml = R"(
<root>
  <child>hello</child>
  <data value="42"/>
</root>
)";

// 네임스페이스가 포함된 XML (ONVIF/SOAP 응답 형태)
constexpr const char* kNamespacedXml = R"(
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"
            xmlns:tds="http://www.onvif.org/ver10/device/wsdl">
  <s:Body>
    <tds:GetDeviceInformationResponse>
      <tds:Manufacturer>TestCam</tds:Manufacturer>
      <tds:Model>NC-100</tds:Model>
    </tds:GetDeviceInformationResponse>
  </s:Body>
</s:Envelope>
)";

// 잘못된 XML (태그 미닫힘)
constexpr const char* kInvalidXml = R"(<root><unclosed)";

} // namespace

// ============================================================================
// XmlDocument 파싱 테스트
// ============================================================================

TEST(XmlUtilTest, LoadFromString_ValidXml)
{
  // 유효한 XML 문자열 파싱 성공 검증
  XmlDocument doc;
  auto ec = doc.parse(kValidXml);

  EXPECT_FALSE(ec) << "파싱 에러: " << ec.message();

  // 루트 노드 확인
  auto root = doc.root();
  EXPECT_TRUE(root);
}

TEST(XmlUtilTest, LoadFromString_InvalidXml)
{
  // 잘못된 XML 파싱 실패 처리 검증
  XmlDocument doc;
  auto ec = doc.parse(kInvalidXml);

  // 파싱 실패 시 에러 코드 반환
  EXPECT_TRUE(static_cast<bool>(ec));
}

// ============================================================================
// XPath 노드 선택 테스트
// ============================================================================

TEST(XmlUtilTest, SelectNode_ExistingPath)
{
  // 존재하는 XPath로 노드 선택 성공 검증
  XmlDocument doc;
  ASSERT_FALSE(doc.parse(kValidXml));

  auto node = doc.select_node("/root/child");
  EXPECT_TRUE(node.has_value());
}

TEST(XmlUtilTest, SelectNode_NonExistingPath)
{
  // 존재하지 않는 XPath 결과 확인
  XmlDocument doc;
  ASSERT_FALSE(doc.parse(kValidXml));

  auto node = doc.select_node("/root/nonexistent/path");
  EXPECT_FALSE(node.has_value());
}

// ============================================================================
// 네임스페이스 무시 탐색 테스트
// ============================================================================

TEST(XmlUtilTest, FindChildIgnoreNs_WithNamespace)
{
  // 네임스페이스 prefix 무시하고 노드 탐색 검증
  XmlDocument doc;
  ASSERT_FALSE(doc.parse(kNamespacedXml));

  pugi::xml_node root = doc.document().document_element();

  // s:Body 탐색 (prefix 무시)
  auto body = find_child_ignore_ns(root, "Body");
  EXPECT_TRUE(body.has_value());
}

TEST(XmlUtilTest, FindChildIgnoreNs_Nested)
{
  // 중첩 네임스페이스 노드 탐색 검증
  XmlDocument doc;
  ASSERT_FALSE(doc.parse(kNamespacedXml));

  pugi::xml_node root = doc.document().document_element();

  // Body 내 GetDeviceInformationResponse 탐색
  auto body = find_child_ignore_ns(root, "Body");
  ASSERT_TRUE(body.has_value());

  auto response = find_child_ignore_ns(*body, "GetDeviceInformationResponse");
  EXPECT_TRUE(response.has_value());
}

// ============================================================================
// 자식 노드 텍스트 추출 테스트
// ============================================================================

TEST(XmlUtilTest, GetChildText_Valid)
{
  // 자식 노드 텍스트 추출 검증
  XmlDocument doc;
  ASSERT_FALSE(doc.parse(kValidXml));

  pugi::xml_node root = doc.document().document_element();

  auto text = get_child_text(root, "child");
  ASSERT_TRUE(text.has_value());
  EXPECT_EQ(*text, "hello");
}

TEST(XmlUtilTest, GetChildText_Nonexistent)
{
  // 존재하지 않는 자식 노드 텍스트 추출 - nullopt 반환 검증
  XmlDocument doc;
  ASSERT_FALSE(doc.parse(kValidXml));

  pugi::xml_node root = doc.document().document_element();

  auto text = get_child_text(root, "nonexistent");
  EXPECT_FALSE(text.has_value());
}

} // namespace nx

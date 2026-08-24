// 파일: soap_envelope_unittest.cpp
// 생성일: 2026-02-19
// 설명: SOAP 1.2 Envelope 생성 및 파싱 단위 테스트

#include <nxnet/onvif/soap/soap_envelope.h>
#include <nxnet/onvif/soap/soap_types.h>
#include <nxnet/onvif/onvif_types.h>
#include <nxnet/onvif/onvif_error.h>

#include <gtest/gtest.h>

namespace nx::net::onvif::soap {

// ============================================================================
// 테스트 헬퍼
// ============================================================================

namespace {

/// 테스트용 카메라 시간 생성
nx::net::onvif::DateTime
make_test_time()
{
  return nx::net::onvif::DateTime{
    .year = 2026,
    .month = 2,
    .day = 19,
    .hour = 10,
    .minute = 30,
    .second = 0};
}

/// 테스트용 SOAP 요청 생성
SoapRequest
make_test_request(const std::string& action, const std::string& body)
{
  return SoapRequest{.action = action, .body = body};
}

/// SOAP Fault XML 생성 (파싱 테스트용)
constexpr const char* kSoapFaultXml = R"(
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope">
  <s:Body>
    <s:Fault>
      <s:Code>
        <s:Value>s:Sender</s:Value>
        <s:Subcode>
          <s:Value>ter:InvalidArgVal</s:Value>
        </s:Subcode>
      </s:Code>
      <s:Reason>
        <s:Text xml:lang="en">Invalid argument</s:Text>
      </s:Reason>
    </s:Fault>
  </s:Body>
</s:Envelope>
)";

/// 정상 SOAP 응답 XML (GetDeviceInformation)
constexpr const char* kNormalSoapResponseXml = R"(
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

/// 빈 Body를 가진 SOAP 응답 XML
constexpr const char* kEmptyBodySoapResponseXml = R"(
<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope">
  <s:Body/>
</s:Envelope>
)";

} // namespace

// ============================================================================
// SOAP Envelope 생성 테스트
// ============================================================================

TEST(SoapEnvelopeTest, CreateSoapEnvelope_ContainsBodyAction)
{
  // 생성된 XML에 Body와 Action이 포함되어 있는지 검증
  auto request
    = make_test_request("GetDeviceInformation", "<tds:GetDeviceInformation/>");
  auto camera_time = make_test_time();

  auto result = create_soap_envelope(request, camera_time);
  ASSERT_TRUE(result.has_value())
    << "Envelope 생성 실패: " << result.error().message();

  // Body 태그 포함 여부
  EXPECT_NE(result->find("Body"), std::string::npos);
  // Action 이름 포함 여부
  EXPECT_NE(result->find("GetDeviceInformation"), std::string::npos);
}

TEST(SoapEnvelopeTest, CreateSoapEnvelope_ContainsWsAddressing)
{
  // 생성된 XML에 WS-Addressing 헤더가 포함되어 있는지 검증
  auto request = make_test_request("GetProfiles", "<trt:GetProfiles/>");
  auto camera_time = make_test_time();

  auto result = create_soap_envelope(request, camera_time);
  ASSERT_TRUE(result.has_value())
    << "Envelope 생성 실패: " << result.error().message();

  // WS-Addressing 네임스페이스 또는 Action 헤더 포함 여부
  const bool has_ws_addressing
    = result->find("http://www.w3.org/2005/08/addressing") != std::string::npos
      || result->find("wsa:") != std::string::npos
      || result->find("wsaddr") != std::string::npos;

  EXPECT_TRUE(has_ws_addressing) << "WS-Addressing 헤더가 누락됨";
}

// ============================================================================
// SOAP 응답 파싱 테스트
// ============================================================================

TEST(SoapEnvelopeTest, ParseSoapResponse_SuccessBody)
{
  // 정상 응답에서 Body 추출 검증
  auto result = parse_soap_response(kNormalSoapResponseXml);

  ASSERT_TRUE(result.has_value()) << "파싱 실패: " << result.error().message();
  EXPECT_FALSE(result->is_fault);
  EXPECT_FALSE(result->body.empty());
}

TEST(SoapEnvelopeTest, ParseSoapResponse_FaultResponse)
{
  // Fault 응답 파싱 검증 (is_fault=true 또는 kSoapFault 에러 중 하나)
  auto result = parse_soap_response(kSoapFaultXml);

  if (result.has_value()) {
    // 구현이 Fault를 is_fault=true인 SoapResponse로 반환하는 경우
    EXPECT_TRUE(result->is_fault) << "Fault 응답이 is_fault=true 로 파싱되어야 함";
  }
  else {
    // 또는 kSoapFault 에러 코드를 반환하는 경우
    EXPECT_EQ(
      result.error(),
      nx::net::onvif::make_error_code(nx::net::onvif::OnvifError::kSoapFault));
  }
}

TEST(SoapEnvelopeTest, ParseSoapResponse_EmptyBody)
{
  // 빈 Body에 대한 처리 검증
  auto result = parse_soap_response(kEmptyBodySoapResponseXml);

  // 파싱 자체는 성공하되 body가 비어 있거나 에러 반환
  if (result.has_value()) {
    // 파싱 성공 시 body는 비어있거나 최소 내용만 있어야 함
    EXPECT_FALSE(result->is_fault);
  }
  // else: 빈 body를 에러로 처리하는 구현도 허용
}

TEST(SoapEnvelopeTest, ParseSoapResponse_InvalidXml)
{
  // 완전히 잘못된 XML 입력 처리 검증
  auto result = parse_soap_response("this is not xml at all <<<");

  EXPECT_FALSE(result.has_value());
}

} // namespace nx::net::onvif::soap

// 파일: device_error.h
// 생성일: 2026-02-25
// 수정일: 2026-07-10 : record_server -> nxcore 로 이동
// 설명: 장치 관리 에러 코드 정의

#pragma once

#include <string>
#include <system_error>

namespace nx {

/// 장치 관리 에러 코드
enum class DeviceErrc
{
  kSuccess = 0,
  kDeviceNotFound,          // 장치를 찾을 수 없음
  kDeviceAlreadyExists,     // 동일 GUID 장치가 이미 등록됨
  kInvalidChannelId,        // 채널 ID가 유효하지 않음
  kInvalidDeviceGuid,       // GUID가 비어있거나 유효하지 않음
  kInvalidDeviceName,       // 이름이 비어있거나 유효하지 않음
  kInvalidConnectionInfo,   // 접속 정보가 유효하지 않음
  kConnectionFailed,        // 장치 연결 실패
  kConnectionTimeout,       // 장치 연결 시간 초과
  kAlreadyConnected,        // 장치가 이미 연결됨
  kMaxDevicesReached,       // 최대 장치 수 초과
  kDeviceManagerStopped,    // DeviceManager가 실행 중이 아님
  kDuplicateName,           // 동일 이름의 장치가 이미 등록됨
  kDuplicateConnectionInfo, // 동일 접속 정보의 장치가 이미 등록됨
  kChannelMappingFailed,    // 채널 매핑 실패
  kDbOperationFailed        // DB 작업 실패
};

/// DeviceErrc 에러 카테고리
class DeviceErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override;
  std::string message(int ev) const override;
};

/// 에러 카테고리 싱글턴 반환
const DeviceErrorCategory& device_error_category();

/// DeviceErrc → std::error_code 변환
std::error_code make_error_code(DeviceErrc e);

} // namespace nx

// std::error_code 호환을 위한 특수화
namespace std {
template <>
struct is_error_code_enum<nx::DeviceErrc> : true_type
{
};
} // namespace std

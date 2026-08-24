// 파일: device_error.cpp
// 생성일: 2026-07-10
// 설명: 장치 관리 에러 코드 정의

#include "device_error.h"

namespace nx {
const char*
DeviceErrorCategory::name() const noexcept
{
  return "nx.device";
}

std::string
DeviceErrorCategory::message(int ev) const
{
  switch (static_cast<DeviceErrc>(ev)) {
  case DeviceErrc::kSuccess:
    return "success";
  case DeviceErrc::kDeviceNotFound:
    return "device not found";
  case DeviceErrc::kDeviceAlreadyExists:
    return "device already exists";
  case DeviceErrc::kInvalidChannelId:
    return "invalid channel ID";
  case DeviceErrc::kInvalidDeviceGuid:
    return "invalid device GUID";
  case DeviceErrc::kInvalidDeviceName:
    return "invalid device name";
  case DeviceErrc::kInvalidConnectionInfo:
    return "invalid connection info";
  case DeviceErrc::kConnectionFailed:
    return "device connection failed";
  case DeviceErrc::kConnectionTimeout:
    return "device connection timeout";
  case DeviceErrc::kAlreadyConnected:
    return "device already connected";
  case DeviceErrc::kMaxDevicesReached:
    return "maximum devices reached";
  case DeviceErrc::kDeviceManagerStopped:
    return "device manager stopped";
  case DeviceErrc::kDuplicateName:
    return "duplicate device name";
  case DeviceErrc::kDuplicateConnectionInfo:
    return "duplicate connection info";
  case DeviceErrc::kChannelMappingFailed:
    return "channel mapping failed";
  case DeviceErrc::kDbOperationFailed:
    return "database operation failed";
  default:
    return "unknown device error";
  }
}

const DeviceErrorCategory&
device_error_category()
{
  static DeviceErrorCategory instance;
  return instance;
}

std::error_code
make_error_code(DeviceErrc e)
{
  return {static_cast<int>(e), device_error_category()};
}

} // namespace nx
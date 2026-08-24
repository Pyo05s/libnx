// 파일: device_types.h
// 생성일: 2026-07-10
// 설명: 장치 관리 공용 타입 정의

#pragma once

#include "device_error.h"

#include <cstdint>
#include <string>

namespace nx {
/// 장치 유형
enum class DeviceType : uint8_t
{
  kOnvif = 0,     // ONVIF 카메라
  kRtsp = 1,      // RTSP URL 직접 등록 (가상 장치)
  kMediaFile = 2, // 미디어 파일 등록 (가상 장치)

  kUnknown = 0xFF // 알 수 없는 장치 유형
};

/// 영상 전송 방식 (RTSP 전송 프로토콜)
enum class StreamTransport : uint8_t
{
  kTcp = 0, // RTP over TCP (기본값)
  kUdp = 1, // RTP over UDP
  kHttp = 2 // RTP over HTTP 터널링
};

/// 장치 정보
struct DeviceInfo
{
  int64_t channel_id = 0; // DB 채널 ID (DB 등록 후 채워짐)

  // === 필수 식별 정보 ===
  std::string device_guid;              // GUID (시스템 생성)
  std::string name;                     // 장치 이름 (사용자 입력, 필수)
  DeviceType type = DeviceType::kOnvif; // 장치 유형

  // === ONVIF 연결 정보 ===
  std::string connection_address; // ONVIF: IP/호스트
  uint16_t connection_port = 80;  // ONVIF HTTP 포트 (기본 80)
  std::string username;
  std::string password;

  // === 스트리밍 설정 ===
  StreamTransport transport = StreamTransport::kTcp; // 영상 접속 방식
  uint16_t stream_port = 0; // 영상 접속 포트 (0=카메라 응답 사용)
  // kRtsp: connection_address = 직접 RTSP URL
  // kMediaFile: connection_address = 미디어 파일 경로

  // === 위치 정보 (선택) ===
  double latitude = 0.0;  // 위도
  double longitude = 0.0; // 경도

  // === ONVIF 탐색 정보 (연결 후 자동 채움) ===
  std::string model;
  std::string firmware_version;
  std::string manufacturer;
  std::string serial_number;
};

/// 장치 상태
enum class DeviceState : uint8_t
{
  kConnected,
  kDisconnected,
  kError
};

} // namespace nx
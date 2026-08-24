// 파일: drive_util.h
// 생성일: 2026-03-25
// 설명: 드라이브 정보 조회 유틸리티 (Windows: fixed/remote 유형)

#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace nx {

/// 드라이브 정보
struct DriveInfo
{
  std::string path;  // 드라이브 루트 경로 (예: "C:\\")
  std::string label; // 볼륨 레이블
  std::string type;  // "fixed" | "remote"
  uint64_t total_bytes = 0;
  uint64_t free_bytes = 0;
  uint64_t used_bytes = 0;
  double usage_percent = 0.0;
};

/// 서버 드라이브 목록 조회 (fixed + remote 유형만)
std::vector<DriveInfo> enumerate_drives();

/// DriveInfo → JSON 변환
inline void
to_json(nlohmann::json& j, const DriveInfo& d)
{
  j = nlohmann::json{
    {         "path",          d.path},
    {        "label",         d.label},
    {         "type",          d.type},
    {  "total_bytes",   d.total_bytes},
    {   "free_bytes",    d.free_bytes},
    {   "used_bytes",    d.used_bytes},
    {"usage_percent", d.usage_percent}
  };
}

} // namespace nx

// 파일: drive_util.cpp
// 생성일: 2026-03-25
// 설명: 드라이브 정보 조회 구현 (Windows)

#include "drive_util.h"

#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace nx {

std::vector<DriveInfo>
enumerate_drives()
{
  std::vector<DriveInfo> drives;

#ifdef _WIN32
  // 논리 드라이브 문자열 획득
  wchar_t drive_strings[512]{};
  DWORD len = GetLogicalDriveStringsW(
    static_cast<DWORD>(std::size(drive_strings) - 1),
    drive_strings);

  if (len == 0) {
    spdlog::warn("[DriveUtil] GetLogicalDriveStringsW 실패: err={}", GetLastError());
    return drives;
  }

  // 널 구분 문자열 순회
  for (const wchar_t* p = drive_strings; *p != L'\0'; p += wcslen(p) + 1) {
    UINT drive_type = GetDriveTypeW(p);

    // DRIVE_FIXED(로컬) 및 DRIVE_REMOTE(네트워크/NAS/SAN)만 포함
    if (drive_type != DRIVE_FIXED && drive_type != DRIVE_REMOTE) {
      continue;
    }

    DriveInfo info;

    // 경로 (wchar → char 변환)
    int path_len
      = WideCharToMultiByte(CP_UTF8, 0, p, -1, nullptr, 0, nullptr, nullptr);
    if (path_len > 0) {
      std::string path_str(path_len - 1, '\0');
      WideCharToMultiByte(
        CP_UTF8,
        0,
        p,
        -1,
        path_str.data(),
        path_len,
        nullptr,
        nullptr);
      info.path = std::move(path_str);
    }

    info.type = (drive_type == DRIVE_FIXED) ? "fixed" : "remote";

    // 볼륨 레이블
    wchar_t label_buf[MAX_PATH + 1]{};
    if (
      GetVolumeInformationW(
        p,
        label_buf,
        MAX_PATH + 1,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0)) {
      int label_len = WideCharToMultiByte(
        CP_UTF8,
        0,
        label_buf,
        -1,
        nullptr,
        0,
        nullptr,
        nullptr);
      if (label_len > 0) {
        std::string label_str(label_len - 1, '\0');
        WideCharToMultiByte(
          CP_UTF8,
          0,
          label_buf,
          -1,
          label_str.data(),
          label_len,
          nullptr,
          nullptr);
        info.label = std::move(label_str);
      }
    }

    // 용량 정보
    ULARGE_INTEGER free_avail{}, total{}, total_free{};
    if (GetDiskFreeSpaceExW(p, &free_avail, &total, &total_free)) {
      info.total_bytes = total.QuadPart;
      info.free_bytes = free_avail.QuadPart;
      info.used_bytes = total.QuadPart - free_avail.QuadPart;
      if (total.QuadPart > 0) {
        info.usage_percent = static_cast<double>(info.used_bytes)
                             / static_cast<double>(info.total_bytes) * 100.0;
      }
    }

    drives.push_back(std::move(info));
  }
#else
  spdlog::warn("[DriveUtil] 드라이브 열거는 Windows 전용 기능입니다");
#endif

  return drives;
}

} // namespace nx

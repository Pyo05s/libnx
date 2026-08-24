// 파일: device_util.cpp
// 생성일: 2026-07-10
// 설명: 장치 관련 유틸 함수 정의

#include "device_util.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace nx {

std::string
create_device_guid()
{
  boost::uuids::random_generator generator;
  boost::uuids::uuid uuid = generator();
  return boost::uuids::to_string(uuid);
}

} // namespace nx
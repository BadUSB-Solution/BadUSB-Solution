#pragma once

#include <string>
#include <vector>

namespace badusb_guardian {

// 위험도 판단 임계값과 키워드 목록을 담는 런타임 설정.
struct Config {
  double warn_cps = 15.0;
  double block_cps = 25.0;
  int warn_score = 20;
  int block_score = 50;
  std::vector<std::string> keywords;
};

// config.json 기반 설정 입출력 API.
Config LoadConfig(const std::string& path);
void SaveConfig(const std::string& path, const Config& cfg);

}  // namespace badusb_guardian

#include "core/Config.h"

#include <fstream>
#include <sstream>

namespace badusb_guardian {

namespace {
std::string ReadFile(const std::string& path) {
  // 설정 파일 전체를 문자열로 읽기
  std::ifstream file(path);
  if (!file) {
    return {};
  }
  std::ostringstream ss;
  ss << file.rdbuf();
  return ss.str();
}

std::string Trim(const std::string& s) {
  // 숫자 파싱 전 공백 제거
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

std::vector<std::string> ExtractArray(const std::string& json, const std::string& key) {
  // 키워드 배열을 단순 파서로 추출
  std::vector<std::string> out;
  auto pos = json.find('"' + key + '"');
  if (pos == std::string::npos) return out;
  pos = json.find('[', pos);
  if (pos == std::string::npos) return out;
  auto end = json.find(']', pos);
  if (end == std::string::npos) return out;
  auto body = json.substr(pos + 1, end - pos - 1);
  size_t cur = 0;
  while (true) {
    auto q1 = body.find('"', cur);
    if (q1 == std::string::npos) break;
    auto q2 = body.find('"', q1 + 1);
    if (q2 == std::string::npos) break;
    out.push_back(body.substr(q1 + 1, q2 - q1 - 1));
    cur = q2 + 1;
  }
  return out;
}

bool ExtractNumber(const std::string& json, const std::string& key, double& value) {
  // JSON에서 숫자 값을 찾아 double로 파싱
  auto pos = json.find('"' + key + '"');
  if (pos == std::string::npos) return false;
  pos = json.find(':', pos);
  if (pos == std::string::npos) return false;
  auto end = json.find_first_of(",}\n", pos + 1);
  auto num = Trim(json.substr(pos + 1, end - pos - 1));
  try {
    value = std::stod(num);
    return true;
  } catch (...) {
    return false;
  }
}

bool ExtractInt(const std::string& json, const std::string& key, int& value) {
  // 숫자를 int로 변환하여 저장
  double temp = 0.0;
  if (!ExtractNumber(json, key, temp)) return false;
  value = static_cast<int>(temp);
  return true;
}
}  // namespace

Config LoadConfig(const std::string& path) {
  // 파일이 없거나 파싱 실패 시 기본 설정으로 동작
  Config cfg;
  auto json = ReadFile(path);
  if (json.empty()) {
    return cfg;
  }

  ExtractNumber(json, "warn_cps", cfg.warn_cps);
  ExtractNumber(json, "block_cps", cfg.block_cps);
  ExtractInt(json, "warn_score", cfg.warn_score);
  ExtractInt(json, "block_score", cfg.block_score);
  cfg.keywords = ExtractArray(json, "keywords");

  return cfg;
}

void SaveConfig(const std::string& path, const Config& cfg) {
  // 현재 설정을 JSON 형식으로 저장
  std::ofstream file(path, std::ios::out | std::ios::trunc);
  if (!file) {
    return;
  }
  file << "{\n";
  file << "  \"warn_cps\": " << cfg.warn_cps << ",\n";
  file << "  \"block_cps\": " << cfg.block_cps << ",\n";
  file << "  \"warn_score\": " << cfg.warn_score << ",\n";
  file << "  \"block_score\": " << cfg.block_score << ",\n";
  file << "  \"keywords\": [\n";
  for (size_t i = 0; i < cfg.keywords.size(); ++i) {
    file << "    \"" << cfg.keywords[i] << "\"";
    if (i + 1 < cfg.keywords.size()) file << ",";
    file << "\n";
  }
  file << "  ]\n";
  file << "}\n";
}

}  // namespace badusb_guardian

#include "core/Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace badusb_guardian {

Logger& Logger::Instance() {
  // 전역에서 공유하는 싱글턴 로거
  static Logger instance;
  return instance;
}

void Logger::Open(const std::string& path) {
  // 로그 파일을 append 모드로 열기
  std::lock_guard<std::mutex> lock(mutex_);
  file_.open(path, std::ios::out | std::ios::app);
}

void Logger::Log(LogLevel level, const std::string& message) {
  // 스레드 안전하게 로그 한 줄 기록
  std::lock_guard<std::mutex> lock(mutex_);
  if (!file_.is_open()) {
    return;
  }

  // 현재 시간을 로컬 타임스탬프로 변환
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
  localtime_s(&tm, &t);

  std::ostringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");

  const char* level_str = "INFO";
  if (level == LogLevel::Warning) level_str = "WARNING";
  if (level == LogLevel::Critical) level_str = "CRITICAL";

  // "YYYY-MM-DD HH:MM:SS [LEVEL] message" 형식으로 저장
  file_ << ss.str() << " [" << level_str << "] " << message << "\n";
  file_.flush();
}

}  // namespace badusb_guardian

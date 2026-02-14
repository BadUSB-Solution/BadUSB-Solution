#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace badusb_guardian {

// 로그 메시지 심각도.
enum class LogLevel {
  Info,
  Warning,
  Critical
};

// 위험 이벤트를 레벨별로 파일에 남기는 스레드 안전 로거.
class Logger {
 public:
  // 전역 공유 로거 인스턴스를 반환한다.
  static Logger& Instance();

  // 로그 출력 파일을 연다.
  void Open(const std::string& path);
  // 타임스탬프/레벨과 함께 메시지를 파일에 기록한다.
  void Log(LogLevel level, const std::string& message);

 private:
  Logger() = default;

  std::mutex mutex_;
  std::ofstream file_;
};

}  // namespace badusb_guardian

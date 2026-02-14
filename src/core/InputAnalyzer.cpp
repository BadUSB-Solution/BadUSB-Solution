#include "core/InputAnalyzer.h"

#include <algorithm>
#include <cctype>

namespace badusb_guardian {

namespace {
std::u32string ToLower(const std::u32string& input) {
  // ASCII 범위만 소문자로 변환해 키워드 탐지에 사용
  std::u32string out = input;
  for (auto& ch : out) {
    if (ch <= 0x7F) {
      ch = static_cast<char32_t>(std::tolower(static_cast<unsigned char>(ch)));
    }
  }
  return out;
}

std::string ToLowerAscii(const std::string& input) {
  // 키워드 리스트를 비교하기 위한 소문자 변환
  std::string out = input;
  for (auto& ch : out) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return out;
}
}  // namespace

InputAnalyzer::InputAnalyzer(size_t buffer_limit)
    : buffer_limit_(buffer_limit) {
  // 기본 위험 키워드 세트
  keywords_ = {"powershell", "cmd", "netsh", "rundll32"};
}

void InputAnalyzer::SetCpsThresholds(double warn_cps, double block_cps) {
  // CPS 경고/차단 기준 업데이트
  warn_cps_ = warn_cps;
  block_cps_ = block_cps;
}

void InputAnalyzer::SetKeywordList(std::vector<std::string> keywords) {
  // UI/설정에서 넘어온 키워드로 교체
  keywords_ = std::move(keywords);
}

void InputAnalyzer::PushEvent(const InputEvent& ev) {
  // 최근 입력 버퍼를 유지하면서 CPS 갱신
  buffer_.push_back(ev);
  if (buffer_.size() > buffer_limit_) {
    buffer_.pop_front();
  }
  UpdateCps(ev);
}

void InputAnalyzer::UpdateCps(const InputEvent& ev) {
  if (!ev.is_down) {
    return;
  }

  // 최근 1초 구간의 keydown 개수를 CPS로 계산
  const uint64_t window_ms = 1000;
  uint64_t now = ev.timestamp_ms;
  size_t count = 0;
  for (auto it = buffer_.rbegin(); it != buffer_.rend(); ++it) {
    if (now - it->timestamp_ms > window_ms) {
      break;
    }
    if (it->is_down) {
      ++count;
    }
  }
  cps_ = static_cast<double>(count);
}

double InputAnalyzer::GetCps() const {
  return cps_;
}

std::u32string InputAnalyzer::GetRecentText() const {
  // 입력 버퍼에서 실제 문자만 모아 최근 텍스트를 구성
  std::u32string text;
  for (const auto& ev : buffer_) {
    if (ev.is_down && ev.text != U'\0') {
      text.push_back(ev.text);
    }
  }
  return text;
}

bool InputAnalyzer::MatchesSequence() const {
  // Simple defensive heuristic: Win+R followed by fast text entry within 2 seconds.
  bool saw_win_r = false;
  uint64_t win_r_time = 0;
  for (const auto& ev : buffer_) {
    // Win 키 이후 2초 내 텍스트 입력이 있으면 의심 패턴으로 처리
    if (ev.key_code == 0x5B && ev.is_down) {  // VK_LWIN
      saw_win_r = true;
      win_r_time = ev.timestamp_ms;
    }
    if (saw_win_r && ev.timestamp_ms - win_r_time <= 2000 && ev.text != U'\0') {
      return true;
    }
  }
  return false;
}

bool InputAnalyzer::HasInjectedInput() const {
  // OS가 주입된 입력으로 표시한 이벤트가 있는지 확인
  for (const auto& ev : buffer_) {
    if (ev.injected) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> InputAnalyzer::FindKeywords(const std::u32string& text) const {
  // 최근 텍스트에서 위험 키워드가 포함되었는지 탐지
  std::vector<std::string> hits;
  auto lower_text = ToLower(text);
  std::string ascii_text;
  ascii_text.reserve(lower_text.size());
  for (auto ch : lower_text) {
    if (ch <= 0x7F) {
      ascii_text.push_back(static_cast<char>(ch));
    } else {
      ascii_text.push_back(' ');
    }
  }

  for (const auto& kw : keywords_) {
    auto kw_lower = ToLowerAscii(kw);
    if (ascii_text.find(kw_lower) != std::string::npos) {
      hits.push_back(kw);
    }
  }
  return hits;
}

RiskResult InputAnalyzer::ComputeRiskScore() const {
  // CPS/시퀀스/주입/키워드 기반으로 위험 점수를 합산
  RiskResult result;
  if (cps_ >= block_cps_) {
    result.score += 50;
    result.high_cps = true;
  } else if (cps_ >= warn_cps_) {
    result.score += 30;
    result.high_cps = true;
  }

  if (MatchesSequence()) {
    result.score += 40;
    result.sequence_match = true;
  }

  if (HasInjectedInput()) {
    result.score += 20;
    result.injected_input = true;
  }

  auto keywords = FindKeywords(GetRecentText());
  if (!keywords.empty()) {
    result.score += 10 * static_cast<int>(keywords.size());
    result.keywords = std::move(keywords);
  }

  return result;
}

}  // namespace badusb_guardian

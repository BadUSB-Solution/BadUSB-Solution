#pragma once

#include <string>
#include <vector>
#include <deque>
#include <cstdint>

namespace badusb_guardian {

// 키보드 훅에서 넘겨받는 단일 입력 이벤트.
struct InputEvent {
  uint32_t key_code;
  bool is_down;
  uint64_t timestamp_ms;
  char32_t text;
  bool injected = false;
};

// 입력 이벤트 집합을 분석해 계산한 위험 점수 결과.
struct RiskResult {
  int score = 0;
  bool high_cps = false;
  bool sequence_match = false;
  bool injected_input = false;
  std::vector<std::string> keywords;
};

// 최근 입력에서 CPS, 시퀀스, 키워드를 기반으로 위험 점수를 계산한다.
class InputAnalyzer {
 public:
  explicit InputAnalyzer(size_t buffer_limit = 256);

  // 입력 이벤트를 버퍼에 추가하고 내부 상태(CPS)를 갱신한다.
  void PushEvent(const InputEvent& ev);
  // 최근 1초 기준 입력 속도(CPS)를 반환한다.
  double GetCps() const;
  // 현재 버퍼를 기준으로 위험 점수를 계산한다.
  RiskResult ComputeRiskScore() const;
  // 최근 입력에서 문자로 해석 가능한 텍스트만 모아 반환한다.
  std::u32string GetRecentText() const;

  // CPS 경고/차단 임계값을 런타임에 변경한다.
  void SetCpsThresholds(double warn_cps, double block_cps);
  // 위험 키워드 목록을 런타임에 변경한다.
  void SetKeywordList(std::vector<std::string> keywords);

 private:
  void UpdateCps(const InputEvent& ev);
  bool MatchesSequence() const;
  bool HasInjectedInput() const;
  std::vector<std::string> FindKeywords(const std::u32string& text) const;

  size_t buffer_limit_;
  std::deque<InputEvent> buffer_;
  double cps_ = 0.0;
  double warn_cps_ = 15.0;
  double block_cps_ = 25.0;
  std::vector<std::string> keywords_;
};

}  // namespace badusb_guardian

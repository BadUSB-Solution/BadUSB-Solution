#include "core/DefenseEngine.h"

#include <sstream>

namespace badusb_guardian {

DefenseEngine::DefenseEngine() = default;

void DefenseEngine::PushEvent(const InputEvent& ev) {
  // 다중 스레드 환경에서 입력 버퍼 업데이트 보호
  std::lock_guard<std::mutex> lock(mutex_);
  analyzer_.PushEvent(ev);
}

void DefenseEngine::Configure(const Config& config) {
  // 설정 값을 분석기에 적용하고 임계값 갱신
  std::lock_guard<std::mutex> lock(mutex_);
  analyzer_.SetCpsThresholds(config.warn_cps, config.block_cps);
  if (!config.keywords.empty()) {
    analyzer_.SetKeywordList(config.keywords);
  }
  warn_score_ = config.warn_score;
  block_score_ = config.block_score;
}

DefenseDecision DefenseEngine::Evaluate() {
  // 최신 입력 버퍼 기준으로 위험도를 계산
  std::lock_guard<std::mutex> lock(mutex_);
  auto risk = analyzer_.ComputeRiskScore();
  DefenseDecision decision;
  decision.score = risk.score;

  // 위험 사유를 사람이 읽기 쉬운 메시지로 구성
  std::ostringstream reason;
  if (risk.high_cps) {
    reason << "High CPS; ";
  }
  if (risk.sequence_match) {
    reason << "Suspicious sequence; ";
  }
  if (risk.injected_input) {
    reason << "Injected input; ";
  }
  if (!risk.keywords.empty()) {
    reason << "Keywords: ";
    for (size_t i = 0; i < risk.keywords.size(); ++i) {
      reason << risk.keywords[i];
      if (i + 1 < risk.keywords.size()) {
        reason << ",";
      }
    }
    reason << "; ";
  }

  if (risk.score >= block_score_) {
    // 차단 기준을 넘으면 입력 차단 플래그 설정
    decision.block = true;
  }

  if (risk.score >= warn_score_ && !reason.str().empty()) {
    // 경고 기준 이상일 때만 이유 문자열을 노출
    decision.reason = reason.str();
  }

  return decision;
}

const InputAnalyzer& DefenseEngine::Analyzer() const {
  return analyzer_;
}

}  // namespace badusb_guardian

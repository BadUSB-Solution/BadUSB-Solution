#pragma once

#include "core/Config.h"
#include "core/InputAnalyzer.h"
#include <string>
#include <mutex>

namespace badusb_guardian {

// 훅 콜백에 전달되는 최종 방어 판단 결과.
struct DefenseDecision {
  bool block = false;
  int score = 0;
  std::string reason;
};

// 분석 결과를 바탕으로 경고/차단 여부를 최종 판단하는 엔진.
class DefenseEngine {
 public:
  DefenseEngine();

  // 입력 이벤트를 분석기에 누적한다.
  void PushEvent(const InputEvent& ev);
  // 누적 입력을 기준으로 현재 차단/경고 여부를 계산한다.
  DefenseDecision Evaluate();
  // 설정 파일/GUI에서 받은 임계값을 엔진에 반영한다.
  void Configure(const Config& config);

  // UI 표시용으로 분석기 읽기 전용 참조를 제공한다.
  const InputAnalyzer& Analyzer() const;

 private:
  mutable std::mutex mutex_;
  InputAnalyzer analyzer_;
  int block_score_ = 50;
  int warn_score_ = 20;
};

}  // namespace badusb_guardian

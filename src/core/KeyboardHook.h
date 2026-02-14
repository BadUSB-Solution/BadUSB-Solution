#pragma once

#ifdef _WIN32

#include "core/DefenseEngine.h"
#include <functional>
#include <windows.h>

namespace badusb_guardian {

// 시스템 전역 키 이벤트를 받아 방어 엔진 결정에 따라 통과/차단한다.
class KeyboardHook {
 public:
  // 훅 콜백에서 입력 이벤트를 받아 최종 차단 여부를 반환하는 함수 타입.
  using DecisionFn = std::function<DefenseDecision(const InputEvent&)>;

  KeyboardHook();
  ~KeyboardHook();

  // 시스템 전역 키보드 훅을 등록하고 결정 함수를 연결한다.
  bool Start(DecisionFn decision_fn);
  // 등록된 전역 훅을 해제한다.
  void Stop();

 private:
  static LRESULT CALLBACK HookProc(int nCode, WPARAM wParam, LPARAM lParam);
  static KeyboardHook* instance_;

  HHOOK hook_ = nullptr;
  DecisionFn decision_fn_;
};

}  // namespace badusb_guardian

#endif  // _WIN32

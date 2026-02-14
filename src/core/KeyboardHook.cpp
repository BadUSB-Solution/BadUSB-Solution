#ifdef _WIN32

#include "core/KeyboardHook.h"

#include <chrono>

namespace badusb_guardian {

KeyboardHook* KeyboardHook::instance_ = nullptr;

namespace {
uint64_t NowMs() {
  // 시스템 부팅 후 경과 시간을 ms로 가져옴
  return static_cast<uint64_t>(GetTickCount64());
}

char32_t ToChar(const KBDLLHOOKSTRUCT* info) {
  // 현재 키보드 상태를 이용해 VK 코드를 문자로 변환
  BYTE keyboard_state[256] = {};
  if (!GetKeyboardState(keyboard_state)) {
    return U'\0';
  }

  WCHAR buffer[4] = {0};
  UINT scan = info->scanCode;
  int result = ToUnicode(info->vkCode, scan, keyboard_state, buffer, 4, 0);
  if (result == 1) {
    return static_cast<char32_t>(buffer[0]);
  }
  return U'\0';
}
}  // namespace

KeyboardHook::KeyboardHook() = default;

KeyboardHook::~KeyboardHook() {
  // 인스턴스 종료 시 훅을 자동 해제
  Stop();
}

bool KeyboardHook::Start(DecisionFn decision_fn) {
  if (hook_) {
    // 이미 훅이 걸려 있으면 재설치하지 않음
    return true;
  }
  decision_fn_ = std::move(decision_fn);
  instance_ = this;
  // 전역 키보드 훅 등록
  hook_ = SetWindowsHookEx(WH_KEYBOARD_LL, HookProc, nullptr, 0);
  return hook_ != nullptr;
}

void KeyboardHook::Stop() {
  if (hook_) {
    // 시스템 훅 해제
    UnhookWindowsHookEx(hook_);
    hook_ = nullptr;
  }
  instance_ = nullptr;
}

LRESULT CALLBACK KeyboardHook::HookProc(int nCode, WPARAM wParam, LPARAM lParam) {
  if (nCode < 0 || !instance_) {
    // 훅 체인을 유지하면서 다음 훅으로 전달
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
  }

  if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
    // 눌림 이벤트만 분석 대상으로 수집
    const auto* info = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
    InputEvent ev{};
    ev.key_code = static_cast<uint32_t>(info->vkCode);
    ev.is_down = true;
    ev.timestamp_ms = NowMs();
    ev.text = ToChar(info);
    ev.injected = (info->flags & LLKHF_INJECTED) != 0;

    if (instance_->decision_fn_) {
      // 결정 함수에서 차단 판단이 나오면 이벤트를 막음
      auto decision = instance_->decision_fn_(ev);
      if (decision.block) {
        return 1;  // block this key event
      }
    }
  }

  // 차단하지 않으면 다음 훅/시스템으로 전달
  return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

}  // namespace badusb_guardian

#endif  // _WIN32

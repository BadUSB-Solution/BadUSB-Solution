#include "core/DefenseEngine.h"

#include <iostream>

using namespace badusb_guardian;

namespace {
InputEvent MakeChar(char32_t ch, uint64_t t) {
  // 문자 입력 이벤트 생성
  return InputEvent{0, true, t, ch, false};
}

InputEvent MakeKey(uint32_t key, uint64_t t, bool injected = false) {
  // 키 코드 입력 이벤트 생성
  return InputEvent{key, true, t, U'\0', injected};
}

void FeedText(DefenseEngine& engine, const std::u32string& text, uint64_t& t, uint64_t step_ms) {
  // 일정 간격으로 텍스트를 입력한 것처럼 이벤트를 주입
  for (auto ch : text) {
    engine.PushEvent(MakeChar(ch, t));
    t += step_ms;
  }
}

bool TestFastTyping() {
  // 초고속 타이핑이 CPS 경고 이상인지 확인
  DefenseEngine engine;
  uint64_t t = 0;
  FeedText(engine, U"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", t, 10);
  auto decision = engine.Evaluate();
  return decision.score >= 30;
}

bool TestKeywordDetection() {
  // 위험 키워드가 점수에 반영되는지 확인
  DefenseEngine engine;
  uint64_t t = 0;
  FeedText(engine, U"powershell", t, 30);
  auto decision = engine.Evaluate();
  return decision.score >= 10;
}

bool TestWinRSequence() {
  // Win+R 이후 텍스트 입력 시 시퀀스 탐지가 동작하는지 확인
  DefenseEngine engine;
  uint64_t t = 0;
  engine.PushEvent(MakeKey(0x5B, t));  // VK_LWIN
  t += 50;
  FeedText(engine, U"cmd", t, 50);
  auto decision = engine.Evaluate();
  return decision.score >= 40;
}

bool TestSlowTyping() {
  // 느린 입력은 위험 점수가 0인지 확인
  DefenseEngine engine;
  uint64_t t = 0;
  FeedText(engine, U"hello world", t, 250);
  auto decision = engine.Evaluate();
  return decision.score == 0;
}

bool TestInjectedInput() {
  // 주입된 입력 플래그가 위험 점수에 반영되는지 확인
  DefenseEngine engine;
  uint64_t t = 0;
  engine.PushEvent(MakeKey('A', t, true));
  auto decision = engine.Evaluate();
  return decision.score >= 20;
}
// Digispark WiFi Stealer 공격 패턴 시뮬레이션
// 실제 공격 코드가 아닌, 그 공격이 생성하는 키보드 입력 패턴을 재현하여
// 방어 엔진이 올바르게 차단하는지 검증
bool TestDigisparkWifiStealer() {
  DefenseEngine engine;
  uint64_t t = 5000;  // Digispark은 5초 delay 후 시작

  // --- Phase 1: Win+R (실행 창 열기) ---
  engine.PushEvent(MakeKey(0x5B, t));  // VK_LWIN down
  t += 10;
  engine.PushEvent(MakeKey('R', t));   // R key
  t += 10;

  auto decision = engine.Evaluate();
  std::cout << "  [Phase 1] Win+R -> score=" << decision.score
            << " block=" << decision.block << "\n";

  // --- Phase 2: 300ms 대기 후 powershell 명령 초고속 입력 ---
  // Digispark print()는 ~3ms/char 속도 (약 300+ CPS)
  t += 300;
  std::u32string payload = U"powershell -WindowStyle Hidden -Command \"";

  int blocked_at = -1;
  for (size_t i = 0; i < payload.size(); ++i) {
    engine.PushEvent(MakeChar(payload[i], t));
    t += 3;  // Digispark의 실제 입력 속도: ~3ms per char
    decision = engine.Evaluate();
    if (decision.block && blocked_at < 0) {
      blocked_at = static_cast<int>(i);
      std::cout << "  [Phase 2] BLOCKED at char " << blocked_at
                << " ('" << static_cast<char>(payload[i]) << "')"
                << " score=" << decision.score
                << " reason=" << decision.reason << "\n";
    }
  }

  if (blocked_at < 0) {
    std::cout << "  [Phase 2] WARNING: powershell command was NOT blocked!\n";
    std::cout << "  Final score=" << decision.score << "\n";
  }

  // --- Phase 3: WiFi 수집 명령 입력 시도 ---
  std::u32string wifi_cmd = U"netsh wlan show profiles";
  bool wifi_blocked = false;
  for (size_t i = 0; i < wifi_cmd.size(); ++i) {
    engine.PushEvent(MakeChar(wifi_cmd[i], t));
    t += 3;
    decision = engine.Evaluate();
    if (decision.block) {
      wifi_blocked = true;
    }
  }
  std::cout << "  [Phase 3] netsh wlan command blocked=" << wifi_blocked
            << " score=" << decision.score << "\n";

  // 최종 판정: Phase 2에서 차단되어야 통과
  bool passed = (blocked_at >= 0 && blocked_at < 20);
  std::cout << "  [Result] " << (passed ? "PASS" : "FAIL")
            << " - attack blocked at char " << blocked_at << "\n";
  return passed;
}

// 정상 사용과 공격을 구분하는지 확인
bool TestNormalWinRUsage() {
  // 정상적인 Win+R 사용은 차단되지 않아야 함
  DefenseEngine engine;
  uint64_t t = 0;

  // 정상적으로 Win+R 누르고 "calc" 입력 (사람 속도: ~100ms/char)
  engine.PushEvent(MakeKey(0x5B, t));  // VK_LWIN
  t += 100;
  engine.PushEvent(MakeKey('R', t));
  t += 500;  // 사람은 실행 창 뜰 때까지 좀 기다림

  std::u32string normal_input = U"calc";
  for (auto ch : normal_input) {
    engine.PushEvent(MakeChar(ch, t));
    t += 120;  // 사람 타이핑 속도
  }

  auto decision = engine.Evaluate();
  std::cout << "  Normal Win+R 'calc' -> score=" << decision.score
            << " block=" << decision.block << "\n";

  // 정상 사용은 차단되면 안 됨
  bool passed = !decision.block;
  std::cout << "  [Result] " << (passed ? "PASS" : "FAIL")
            << " - normal usage " << (decision.block ? "incorrectly blocked" : "allowed") << "\n";
  return passed;
}
}

int main() {
  int failed = 0;
  int total = 0;

  // 간단한 테스트 러너로 각 케이스 실행
  auto RunTest = [&](const char* name, bool (*fn)()) {
    total++;
    std::cout << "[TEST] " << name << "\n";
    if (!fn()) {
      std::cout << "  FAILED\n\n";
      failed++;
    } else {
      std::cout << "  PASSED\n\n";
    }
  };

  RunTest("FastTyping", TestFastTyping);
  RunTest("KeywordDetection", TestKeywordDetection);
  RunTest("WinRSequence", TestWinRSequence);
  RunTest("SlowTyping", TestSlowTyping);
  RunTest("InjectedInput", TestInjectedInput);
  RunTest("DigisparkWifiStealer", TestDigisparkWifiStealer);
  RunTest("NormalWinRUsage", TestNormalWinRUsage);

  std::cout << "============================\n";
  std::cout << total - failed << "/" << total << " tests passed\n";

  if (failed > 0) {
    std::cout << failed << " test(s) FAILED\n";
  }

  return failed == 0 ? 0 : 1;
}

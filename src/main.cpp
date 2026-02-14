#include "core/DefenseEngine.h"
#include "core/KeyboardHook.h"
#include "core/Config.h"
#include "core/Logger.h"
#include "gui/MainWindow.h"

#include <atomic>
#include <iostream>
#include <windows.h>

using namespace badusb_guardian;

namespace {
std::atomic<bool> g_running{true};

BOOL WINAPI ConsoleHandler(DWORD type) {
  if (type == CTRL_C_EVENT || type == CTRL_CLOSE_EVENT) {
    // Ctrl+C나 콘솔 종료 신호를 받으면 메시지 루프를 끝냄
    g_running = false;
    PostQuitMessage(0);
    return TRUE;
  }
  return FALSE;
}
}  // namespace

int main() {
#ifdef _WIN32
  // 콘솔 종료 이벤트를 잡아 깔끔하게 종료
  SetConsoleCtrlHandler(ConsoleHandler, TRUE);

  // 설정 파일 로드 후 기본값 누락분은 Config 기본값 사용
  auto cfg = LoadConfig("config.json");

  // 로그 파일을 열고 시작 이벤트 기록
  Logger::Instance().Open("badusb_guardian.log");
  Logger::Instance().Log(LogLevel::Info, "BadUSB Guardian started");

  // 방어 엔진에 초기 설정 적용
  DefenseEngine engine;
  engine.Configure(cfg);

  KeyboardHook hook;
  MainWindow ui;
  if (!ui.Initialize()) {
    std::cerr << "Failed to initialize UI.\n";
  }
  // UI에 현재 설정값을 전달해 슬라이더/텍스트에 반영
  ui.SetConfig(cfg);

  // 키보드 이벤트마다 점수 계산 후 UI/로그를 업데이트
  bool started = hook.Start([&engine, &ui](const InputEvent& ev) {
    // 입력 이벤트를 엔진에 전달하고 위험도 평가
    engine.PushEvent(ev);
    auto decision = engine.Evaluate();

    // UI 표시용 최신 상태 갱신
    UiState state;
    state.cps = engine.Analyzer().GetCps();
    state.score = decision.score;
    state.block = decision.block;
    state.reason = decision.reason;
    ui.Update(state);

    // 경고 이상 점수는 콘솔과 UI 로그로 기록
    if (decision.score >= 20) {
      std::cout << "Score=" << decision.score;
      if (!decision.reason.empty()) {
        std::cout << " Reason=" << decision.reason;
      }
      std::cout << "\n";

      ui.AddLog("Score=" + std::to_string(decision.score) + " Reason=" + decision.reason,
                decision.score >= 50 ? LogLevel::Critical : LogLevel::Warning);
    }

    // 차단 판단 시 사용자에게 알림
    if (decision.block) {
      std::cout << "Blocked suspicious input.\n";
    }

    // 경고/차단 수준에 따라 파일 로그 레벨을 다르게 기록
    if (decision.score >= 50) {
      Logger::Instance().Log(LogLevel::Critical, decision.reason);
    } else if (decision.score >= 20) {
      Logger::Instance().Log(LogLevel::Warning, decision.reason);
    }

    return decision;
  });

  if (!started) {
    std::cerr << "Failed to start keyboard hook.\n";
    Logger::Instance().Log(LogLevel::Critical, "Failed to start keyboard hook");
    return 1;
  }

  std::cout << "BadUSB Guardian test harness running. Press Ctrl+C to exit.\n";

  MSG msg;
  while (g_running && GetMessage(&msg, nullptr, 0, 0)) {
    // 윈도우 메시지를 처리하고 UI 프레임을 렌더링
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    ui.Render();

    // UI에서 설정 변경 요청이 들어오면 즉시 엔진에 반영
    Config updated;
    if (ui.ConsumeConfigUpdate(updated)) {
      engine.Configure(updated);
      SaveConfig("config.json", updated);
      ui.AddLog("Config applied from UI", LogLevel::Info);
    }

    // 창이 닫히면 종료 루프 탈출
    if (!ui.IsOpen()) {
      g_running = false;
      PostQuitMessage(0);
    }
  }

  // 시스템 훅과 UI 자원을 정리
  hook.Stop();
  ui.Shutdown();
  Logger::Instance().Log(LogLevel::Info, "BadUSB Guardian stopped");
  return 0;
#else
  std::cerr << "Windows only.\n";
  return 1;
#endif
}

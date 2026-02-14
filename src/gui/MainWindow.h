#pragma once

#include "core/Config.h"
#include "core/Logger.h"

#include <string>
#include <vector>

#ifdef BADUSB_WITH_IMGUI
#include <d3d11.h>
#include <windows.h>
#endif

namespace badusb_guardian {

// UI가 표시할 최신 엔진 상태 스냅샷.
struct UiState {
  double cps = 0.0;
  int score = 0;
  bool block = false;
  std::string reason;
};

// UI 내부 알림 로그 한 줄.
struct LogEntry {
  std::string text;
  LogLevel level = LogLevel::Info;
};

// ImGui 기반 대시보드 창 래퍼.
class MainWindow {
 public:
  // UI 리소스를 초기화한다.
  bool Initialize();
  // UI 리소스를 해제한다.
  void Shutdown();
  // 최신 엔진 상태를 UI에 반영한다.
  void Update(const UiState& state);
  // 우측 경고 로그 패널에 항목을 추가한다.
  void AddLog(const std::string& line, LogLevel level = LogLevel::Info);
  // 한 프레임을 렌더링한다.
  void Render();
  // 창이 열려 있는지 확인한다.
  bool IsOpen() const;

  // 외부 설정값을 UI 컨트롤에 반영한다.
  void SetConfig(const Config& cfg);
  // Apply 요청이 있을 때 변경된 설정을 반환한다.
  bool ConsumeConfigUpdate(Config& out);

 private:
  std::vector<float> cps_history_;
  size_t cps_cursor_ = 0;
  bool history_ready_ = false;
  UiState latest_;
  std::vector<LogEntry> logs_;

  Config config_;
  std::string keywords_text_;
  bool request_apply_ = false;

#ifdef BADUSB_WITH_IMGUI
  static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
  bool CreateDeviceD3D(HWND hWnd);
  void CleanupDeviceD3D();
  void CreateRenderTarget();
  void CleanupRenderTarget();

  HWND hwnd_ = nullptr;
  ID3D11Device* device_ = nullptr;
  ID3D11DeviceContext* context_ = nullptr;
  IDXGISwapChain* swap_chain_ = nullptr;
  ID3D11RenderTargetView* render_target_ = nullptr;
  bool is_open_ = true;
#else
  bool is_open_ = true;
#endif
};

}  // namespace badusb_guardian

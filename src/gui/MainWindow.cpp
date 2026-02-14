#include "gui/MainWindow.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <sstream>

#ifdef BADUSB_WITH_IMGUI
#include <tchar.h>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

// Ensure the Win32 handler is visible in this translation unit.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace badusb_guardian {

#ifdef BADUSB_WITH_IMGUI
static MainWindow* g_window = nullptr;
#endif

namespace {
std::string JoinKeywords(const std::vector<std::string>& keywords) {
  // 키워드 리스트를 UI 입력창용 문자열로 합침
  std::ostringstream ss;
  for (size_t i = 0; i < keywords.size(); ++i) {
    ss << keywords[i];
    if (i + 1 < keywords.size()) ss << ",";
  }
  return ss.str();
}

std::vector<std::string> SplitKeywords(const std::string& text) {
  // 콤마 기준으로 키워드 입력을 분리
  std::vector<std::string> out;
  std::string token;
  for (char ch : text) {
    if (ch == ',') {
      if (!token.empty()) {
        out.push_back(token);
        token.clear();
      }
    } else if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
      token.push_back(ch);
    }
  }
  if (!token.empty()) out.push_back(token);
  return out;
}
}  // namespace

bool MainWindow::Initialize() {
  // CPS 그래프 히스토리 버퍼 초기화
  cps_history_.assign(120, 0.0f);
#ifdef BADUSB_WITH_IMGUI
  // Win32 창과 DX11 디바이스 생성
  WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, MainWindow::WndProc, 0L, 0L,
                   GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                   _T("BadUSBGuardian"), nullptr};
  RegisterClassEx(&wc);
  hwnd_ = CreateWindow(wc.lpszClassName, _T("BadUSB Guardian"), WS_OVERLAPPEDWINDOW,
                       100, 100, 1000, 650, nullptr, nullptr, wc.hInstance, nullptr);

  if (!CreateDeviceD3D(hwnd_)) {
    CleanupDeviceD3D();
    UnregisterClass(wc.lpszClassName, wc.hInstance);
    return false;
  }

  // ImGui 컨텍스트 구성 및 백엔드 초기화
  ShowWindow(hwnd_, SW_SHOWDEFAULT);
  UpdateWindow(hwnd_);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();

  ImGui_ImplWin32_Init(hwnd_);
  ImGui_ImplDX11_Init(device_, context_);

  g_window = this;
  return true;
#else
  return true;
#endif
}

void MainWindow::Shutdown() {
#ifdef BADUSB_WITH_IMGUI
  // ImGui/DirectX 리소스 정리
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();

  CleanupRenderTarget();
  CleanupDeviceD3D();
  if (hwnd_) {
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
  }
#endif
}

void MainWindow::SetConfig(const Config& cfg) {
  // UI에 표시할 설정 값을 저장
  config_ = cfg;
  keywords_text_ = JoinKeywords(cfg.keywords);
}

bool MainWindow::ConsumeConfigUpdate(Config& out) {
  // Apply 버튼이 눌렸을 때만 설정을 외부로 내보냄
  if (!request_apply_) return false;
  out = config_;
  out.keywords = SplitKeywords(keywords_text_);
  request_apply_ = false;
  return true;
}

void MainWindow::Update(const UiState& state) {
  // 최신 상태를 저장하고 CPS 그래프 히스토리에 반영
  latest_ = state;
  if (!cps_history_.empty()) {
    cps_history_[cps_cursor_] = static_cast<float>(state.cps);
    cps_cursor_ = (cps_cursor_ + 1) % cps_history_.size();
    if (cps_cursor_ == 0) {
      history_ready_ = true;
    }
  }
}

void MainWindow::AddLog(const std::string& line, LogLevel level) {
  // 최근 로그 80개만 유지
  logs_.push_back({line, level});
  if (logs_.size() > 80) {
    logs_.erase(logs_.begin());
  }
}

void MainWindow::Render() {
#ifdef BADUSB_WITH_IMGUI
  if (!is_open_) return;

  // ImGui 프레임 시작
  ImGui_ImplDX11_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowSize(ImVec2(1000, 650), ImGuiCond_Once);
  ImGui::Begin("BadUSB Guardian", &is_open_);

  // 좌측 상태/설정 패널
  ImGui::BeginChild("Status", ImVec2(300, 0), true);
  ImGui::Text("Status");
  ImGui::Separator();
  ImGui::Text("CPS: %.2f", latest_.cps);
  ImGui::Text("Score: %d", latest_.score);
  ImGui::Text("Block: %s", latest_.block ? "YES" : "NO");
  if (!latest_.reason.empty()) {
    ImGui::TextWrapped("Reason: %s", latest_.reason.c_str());
  }
  ImGui::Separator();
  ImGui::Text("Settings");

  float warn_cps = static_cast<float>(config_.warn_cps);
  float block_cps = static_cast<float>(config_.block_cps);
  int warn_score = config_.warn_score;
  int block_score = config_.block_score;

  // 경고/차단 임계값을 슬라이더로 조정
  if (ImGui::SliderFloat("Warn CPS", &warn_cps, 5.0f, 40.0f)) {
    config_.warn_cps = warn_cps;
  }
  if (ImGui::SliderFloat("Block CPS", &block_cps, 10.0f, 60.0f)) {
    config_.block_cps = block_cps;
  }
  if (ImGui::SliderInt("Warn Score", &warn_score, 0, 100)) {
    config_.warn_score = warn_score;
  }
  if (ImGui::SliderInt("Block Score", &block_score, 10, 150)) {
    config_.block_score = block_score;
  }

  // 키워드 입력은 콤마로 구분
  std::array<char, 512> buf{};
  if (keywords_text_.size() >= buf.size()) {
    keywords_text_.resize(buf.size() - 1);
  }
  std::copy(keywords_text_.begin(), keywords_text_.end(), buf.begin());
  if (ImGui::InputText("Keywords", buf.data(), buf.size())) {
    keywords_text_ = buf.data();
  }
  if (ImGui::Button("Apply")) {
    request_apply_ = true;
  }
  ImGui::EndChild();

  ImGui::SameLine();

  // 중앙 CPS 그래프 패널
  ImGui::BeginChild("Graph", ImVec2(380, 0), true);
  ImGui::Text("CPS Trend");
  ImGui::Separator();
  if (history_ready_) {
    std::vector<float> ordered;
    ordered.reserve(cps_history_.size());
    ordered.insert(ordered.end(), cps_history_.begin() + cps_cursor_, cps_history_.end());
    ordered.insert(ordered.end(), cps_history_.begin(), cps_history_.begin() + cps_cursor_);
    ImGui::PlotLines("##cps", ordered.data(), static_cast<int>(ordered.size()), 0, nullptr, 0.0f, 40.0f, ImVec2(360, 200));
  } else {
    ImGui::PlotLines("##cps", cps_history_.data(), static_cast<int>(cps_history_.size()), 0, nullptr, 0.0f, 40.0f, ImVec2(360, 200));
  }
  ImGui::EndChild();

  ImGui::SameLine();

  // 우측 로그 패널
  ImGui::BeginChild("Logs", ImVec2(0, 0), true);
  ImGui::Text("Recent Alerts");
  ImGui::Separator();
  for (const auto& entry : logs_) {
    ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
    if (entry.level == LogLevel::Warning) color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
    if (entry.level == LogLevel::Critical) color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::TextWrapped("%s", entry.text.c_str());
    ImGui::PopStyleColor();
  }
  ImGui::EndChild();

  ImGui::End();

  // 렌더링 및 화면 스왑
  ImGui::Render();
  const float clear_color_with_alpha[4] = {0.1f, 0.1f, 0.1f, 1.0f};
  context_->OMSetRenderTargets(1, &render_target_, nullptr);
  context_->ClearRenderTargetView(render_target_, clear_color_with_alpha);
  ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
  swap_chain_->Present(1, 0);
#else
  // ImGui 미사용 빌드에서는 콘솔로 상태 출력
  std::cout << "[UI] CPS=" << latest_.cps
            << " Score=" << latest_.score
            << " Block=" << (latest_.block ? "YES" : "NO") << "\n";
#endif
}

bool MainWindow::IsOpen() const {
  return is_open_;
}

#ifdef BADUSB_WITH_IMGUI
LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  // ImGui가 처리할 메시지는 먼저 넘김
  if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
    return true;
  }

  switch (msg) {
    case WM_SIZE:
      // 창 리사이즈 시 렌더 타겟을 재생성
      if (g_window && g_window->device_ != nullptr && wParam != SIZE_MINIMIZED) {
        g_window->CleanupRenderTarget();
        g_window->swap_chain_->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
        g_window->CreateRenderTarget();
      }
      return 0;
    case WM_SYSCOMMAND:
      if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
      break;
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProc(hWnd, msg, wParam, lParam);
}

bool MainWindow::CreateDeviceD3D(HWND hWnd) {
  // DX11 디바이스와 스왑체인 초기화
  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 2;
  sd.BufferDesc.Width = 0;
  sd.BufferDesc.Height = 0;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hWnd;
  sd.SampleDesc.Count = 1;
  sd.SampleDesc.Quality = 0;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  UINT create_device_flags = 0;
#ifdef _DEBUG
  create_device_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
  D3D_FEATURE_LEVEL feature_level;
  const D3D_FEATURE_LEVEL feature_level_array[1] = {D3D_FEATURE_LEVEL_11_0};

  if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, create_device_flags,
                                   feature_level_array, 1, D3D11_SDK_VERSION, &sd, &swap_chain_,
                                   &device_, &feature_level, &context_) != S_OK) {
    return false;
  }

  CreateRenderTarget();
  return true;
}

void MainWindow::CleanupDeviceD3D() {
  // DX 리소스 해제
  CleanupRenderTarget();
  if (swap_chain_) { swap_chain_->Release(); swap_chain_ = nullptr; }
  if (context_) { context_->Release(); context_ = nullptr; }
  if (device_) { device_->Release(); device_ = nullptr; }
}

void MainWindow::CreateRenderTarget() {
  // 스왑체인 백버퍼로 렌더 타겟 생성
  ID3D11Texture2D* back_buffer = nullptr;
  swap_chain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
  if (back_buffer) {
    device_->CreateRenderTargetView(back_buffer, nullptr, &render_target_);
    back_buffer->Release();
  }
}

void MainWindow::CleanupRenderTarget() {
  // 렌더 타겟 뷰 해제
  if (render_target_) { render_target_->Release(); render_target_ = nullptr; }
}
#endif

}  // namespace badusb_guardian

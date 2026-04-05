#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace BufferViewUI
{
    // 모달 뷰어: 내부에서 GetBufferedKeys() 폴링. true=허용, false=거부
    bool Show(HWND hParent, uint16_t vid, uint16_t pid,
              const std::wstring& deviceName = L"");

    // 모달리스 모니터링 창 (Copy / Normalize 버튼 포함, 비차단)
    void Open(HWND hParent, uint16_t vid, uint16_t pid,
              const std::wstring& deviceName = L"");
    void Close();

    // 정규화 디버그 창: Raw Buffer 스냅샷 + 정규화 결과 예정 영역
    // KeyDecoder::Decode() 구현 전까지 정규화 섹션은 플레이스홀더 표시
    void OpenNormalizeDebug();
}

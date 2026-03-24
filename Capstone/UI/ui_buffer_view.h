#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace BufferViewUI
{
    // 모달 뷰어: 내부에서 GetBufferedKeys() 폴링. true=허용, false=거부
    bool Show(HWND hParent, uint16_t vid, uint16_t pid,
              const std::wstring& deviceName = L"");

    // 모달리스 모니터링 창 (버튼 없음, 비차단)
    void Open(HWND hParent, uint16_t vid, uint16_t pid,
              const std::wstring& deviceName = L"");
    void Close();
}

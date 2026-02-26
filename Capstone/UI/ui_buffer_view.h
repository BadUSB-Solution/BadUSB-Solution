#pragma once
#include <windows.h>
#include <vector>
#include <cstdint>
#include "keyboard_hook.h"

namespace BufferViewUI
{
    // 모달 뷰어: true=허용, false=거부
    bool Show(HWND hParent, uint16_t vid, uint16_t pid,
              const std::vector<KeyboardHook::KeyRecord>& keys);
}

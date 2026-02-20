#include "keyboard_hook.h"

namespace KeyboardHook
{
    bool Install()
    {
        return true;
    }

    void Uninstall()
    {
    }

    bool IsActive()
    {
        return false;
    }

    std::vector<KeyRecord> GetBufferedKeys()
    {
        return {};
    }

    void ClearBuffer()
    {
    }
}

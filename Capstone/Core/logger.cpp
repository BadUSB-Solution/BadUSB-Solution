// ¾Û »ó¿¡ ·Î±× ¶ç¿ì´Â ºÎºÐ

#include "logger.h"
#include <vector>
#include <mutex>

namespace Logger
{
    static std::mutex g_mu;
    static std::vector<std::wstring> g_logs;

    static HWND g_notifyWnd = nullptr;
    static UINT g_notifyMsg = 0;

    void SetNotifyWindow(HWND hWnd, UINT msgId)
    {
        g_notifyWnd = hWnd;
        g_notifyMsg = msgId;
    }

    void Add(const std::wstring& s)
    {
        {
            std::lock_guard<std::mutex> lk(g_mu);
            g_logs.push_back(s);
            if (g_logs.size() > 500) g_logs.erase(g_logs.begin());
        }
        if (g_notifyWnd && g_notifyMsg)
            PostMessage(g_notifyWnd, g_notifyMsg, 0, 0);
    }

    std::wstring GetAll()
    {
        std::wstring all;
        std::lock_guard<std::mutex> lk(g_mu);
        for (auto& s : g_logs) {
            all += s + L"\r\n";
        }
        return all;
    }
}

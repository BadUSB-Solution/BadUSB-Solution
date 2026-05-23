#include "app_paths.h"

#include <windows.h>
#include <shlobj.h>

#pragma comment(lib, "Shell32.lib")

namespace Storage
{
    namespace
    {
        std::wstring ResolveAbangAllDir()
        {
            wchar_t appData[MAX_PATH]{};
            if (SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData) != S_OK)
            {
                DWORD len = GetEnvironmentVariableW(L"APPDATA", appData, MAX_PATH);
                if (len == 0 || len >= MAX_PATH)
                    wcscpy_s(appData, L"C:\\Temp");
            }

            std::wstring dir(appData);
            dir += L"\\AbangAll";
            CreateDirectoryW(dir.c_str(), nullptr);
            return dir;
        }
    }

    const std::wstring& GetAbangAllDir()
    {
        // 첫 호출에서만 폴더 생성 + 캐싱
        static const std::wstring dir = ResolveAbangAllDir();
        return dir;
    }

    std::wstring GetAllowListPath()
    {
        return GetAbangAllDir() + L"\\allow_list.json";
    }

    std::wstring GetSettingsPath()
    {
        return GetAbangAllDir() + L"\\settings.json";
    }

    void UnhideForWrite(const std::wstring& path)
    {
        const DWORD attr = GetFileAttributesW(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) return;
        if (attr & FILE_ATTRIBUTE_HIDDEN)
            SetFileAttributesW(path.c_str(), attr & ~FILE_ATTRIBUTE_HIDDEN);
    }

    void MarkHidden(const std::wstring& path)
    {
        const DWORD attr = GetFileAttributesW(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES) return;
        SetFileAttributesW(path.c_str(), attr | FILE_ATTRIBUTE_HIDDEN);
    }
}

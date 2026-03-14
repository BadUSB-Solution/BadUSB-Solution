#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "usb_item.h"

namespace Policy
{
    // JSON DB 파일 경로 설정 (기본: 실행파일 옆 policy_db.json)
    void SetDbPath(const std::wstring& path);
    std::wstring GetDbPath();

    // 파일에서 불러오기 / 파일에 저장
    bool Load();
    bool Save();

    // 조회
    bool IsWhitelisted(uint64_t fp);
    bool IsBlacklisted(uint64_t fp);

    // 추가 (메모리 + 즉시 파일 저장)
    void AddWhitelist(const UsbItem& item);
    void AddBlacklist(const UsbItem& item);

    // 제거
    void Remove(uint64_t fp);

    // 전체 목록 반환
    std::vector<UsbItem> GetWhitelist();
    std::vector<UsbItem> GetBlacklist();
}

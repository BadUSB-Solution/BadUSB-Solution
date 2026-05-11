#include "url_parser.h"
#include <cwctype>

namespace UrlClassifier
{
    std::wstring ToLower(const std::wstring& s)
    {
        std::wstring r;
        r.reserve(s.size());
        for (auto c : s) {
            if (c >= L'A' && c <= L'Z') c = c - L'A' + L'a';
            r.push_back(c);
        }
        return r;
    }

    bool EndsWith(const std::wstring& s, const std::wstring& suffix)
    {
        if (s.size() < suffix.size()) return false;
        return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    bool MatchesSuffix(const std::wstring& host, const std::wstring& domain)
    {
        if (host == domain) return true;
        if (host.size() <= domain.size() + 1) return false;
        // host 끝이 "." + domain 인지
        std::wstring dotDomain = L"." + domain;
        return EndsWith(host, dotDomain);
    }

    std::string WideToNarrow(const std::wstring& s)
    {
        std::string r;
        r.reserve(s.size());
        for (auto c : s) {
            if (c <= 0x7F) r.push_back(static_cast<char>(c));
            else r.push_back('?');  // 비ASCII는 ? (호스트 비교용이라 충분)
        }
        return r;
    }

    // URL 문자열을 scheme/host/port/path 로 분리.
    // 잘못된 형식은 가능한 한 부분 채움 (예: 스킴 없으면 host에 다 들어감).
    ParsedUrl ParseUrl(const std::wstring& url)
    {
        ParsedUrl p;
        p.port = -1;

        // 1) scheme 추출: "://" 위치
        size_t schemeEnd = url.find(L"://");
        size_t hostStart = 0;
        if (schemeEnd != std::wstring::npos) {
            p.scheme = ToLower(url.substr(0, schemeEnd));
            hostStart = schemeEnd + 3;
        }

        // 2) host + 포트 + path 분리
        size_t pathStart = url.find(L'/', hostStart);
        std::wstring hostPort;
        if (pathStart == std::wstring::npos) {
            hostPort = url.substr(hostStart);
            p.path = L"";
        } else {
            hostPort = url.substr(hostStart, pathStart - hostStart);
            p.path = url.substr(pathStart);
        }

        // 3) 쿼리스트링 제거된 host:port 영역에서 port 분리
        size_t portColon = hostPort.find(L':');
        if (portColon != std::wstring::npos) {
            p.host = ToLower(hostPort.substr(0, portColon));
            std::wstring portStr = hostPort.substr(portColon + 1);
            int port = 0;
            for (auto c : portStr) {
                if (c >= L'0' && c <= L'9') {
                    port = port * 10 + (c - L'0');
                    if (port > 65535) { port = -1; break; }
                } else {
                    port = -1;
                    break;
                }
            }
            if (port > 0) p.port = port;
        } else {
            p.host = ToLower(hostPort);
        }

        return p;
    }
}

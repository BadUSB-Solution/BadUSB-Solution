#include "hosting_filter.h"

namespace UrlClassifier::HostingFilter
{
    // ----- 규칙 7: 무료 TLD (Freenom 계열) -----
    static const wchar_t* FREE_TLDS[] = {
        L".tk", L".ml", L".ga", L".cf", L".gq",
    };

    // ----- 규칙 8: 무료 정적 호스팅 -----
    static const wchar_t* FREE_HOSTING_DOMAINS[] = {
        L"github.io",       L"gitlab.io",
        L"netlify.app",     L"vercel.app",
        L"pages.dev",       L"web.app",
        L"firebaseapp.com", L"surge.sh",
        L"glitch.me",       L"repl.co",        L"replit.app",
    };

    // ----- 규칙 9: 동적 DNS -----
    static const wchar_t* DDNS_DOMAINS[] = {
        L"duckdns.org", L"ddns.net",   L"no-ip.com",
        L"hopto.org",   L"dyndns.org", L"sytes.net",
        L"zapto.org",
    };

    // ----- 규칙 10: 파일 공유 / 직접 다운로드 -----
    static const wchar_t* FILE_SHARING_DOMAINS[] = {
        L"mega.nz",      L"anonfiles.com", L"mediafire.com",
        L"transfer.sh",  L"file.io",       L"gofile.io",
    };

    // ----- 규칙 11: 터널링 서비스 -----
    static const wchar_t* TUNNELING_DOMAINS[] = {
        L"ngrok.io",          L"ngrok.app",
        L"serveo.net",        L"localtunnel.me",
        L"loca.lt",           L"trycloudflare.com",
    };

    uint32_t Check(const ParsedUrl& p)
    {
        if (p.host.empty()) return 0;

        uint32_t flags = 0;

        // 규칙 7: 무료 TLD (EndsWith — TLD는 suffix 매칭 의미가 다름)
        for (auto* tld : FREE_TLDS) {
            if (EndsWith(p.host, tld)) {
                flags |= URL_FLAG_FREE_TLD;
                break;
            }
        }

        // 규칙 8~11: 도메인 suffix 매칭
        for (auto* d : FREE_HOSTING_DOMAINS) {
            if (MatchesSuffix(p.host, d)) { flags |= URL_FLAG_FREE_HOSTING; break; }
        }
        for (auto* d : DDNS_DOMAINS) {
            if (MatchesSuffix(p.host, d)) { flags |= URL_FLAG_DYNAMIC_DNS; break; }
        }
        for (auto* d : FILE_SHARING_DOMAINS) {
            if (MatchesSuffix(p.host, d)) { flags |= URL_FLAG_FILE_SHARING; break; }
        }
        for (auto* d : TUNNELING_DOMAINS) {
            if (MatchesSuffix(p.host, d)) { flags |= URL_FLAG_TUNNELING; break; }
        }

        return flags;
    }
}

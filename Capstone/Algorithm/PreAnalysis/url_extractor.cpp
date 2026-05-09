#include "url_extractor.h"
#include <map>
#include <cwctype>

namespace UrlExtractor
{
    // ── 헬퍼 ──────────────────────────────────────────────────────────────────

    // Chr(N) 패턴을 문자로 치환 (case-insensitive, ASCII 1-127만)
    static std::wstring ExpandChrTokens(const std::wstring& s)
    {
        std::wstring out;
        out.reserve(s.size());
        size_t i = 0;
        while (i < s.size()) {
            if (i + 4 <= s.size() &&
                towlower(s[i])   == L'c' &&
                towlower(s[i+1]) == L'h' &&
                towlower(s[i+2]) == L'r' &&
                s[i+3] == L'(')
            {
                size_t j = i + 4;
                while (j < s.size() && iswdigit(s[j])) ++j;
                if (j < s.size() && s[j] == L')' && j > i + 4) {
                    int code = _wtoi(s.c_str() + i + 4);
                    if (code >= 1 && code <= 127) out += (wchar_t)code;
                    i = j + 1;
                    continue;
                }
            }
            out += s[i++];
        }
        return out;
    }

    // 표현식을 & 또는 + 로 토큰화 (따옴표 내부 무시)
    static std::vector<std::wstring> TokenizeExpr(const std::wstring& expr)
    {
        std::vector<std::wstring> tokens;
        std::wstring cur;
        bool inQuote = false;

        for (size_t i = 0; i < expr.size(); ++i) {
            wchar_t c = expr[i];
            if (c == L'"') {
                inQuote = !inQuote;
                cur += c;
            } else if (!inQuote && (c == L'&' || c == L'+')) {
                size_t s = 0, e = cur.size();
                while (s < e && iswspace(cur[s])) ++s;
                while (e > s && iswspace(cur[e-1])) --e;
                if (s < e) tokens.push_back(cur.substr(s, e - s));
                cur.clear();
            } else {
                cur += c;
            }
        }
        {
            size_t s = 0, e = cur.size();
            while (s < e && iswspace(cur[s])) ++s;
            while (e > s && iswspace(cur[e-1])) --e;
            if (s < e) tokens.push_back(cur.substr(s, e - s));
        }
        return tokens;
    }

    // 단일 토큰 평가: 문자열 리터럴 / Chr() / 변수 참조
    static std::wstring EvalToken(const std::wstring& tok,
                                   const std::map<std::wstring, std::wstring>& vars)
    {
        if (tok.empty()) return L"";

        // 문자열 리터럴 "..."
        if (tok.size() >= 2 && tok.front() == L'"' && tok.back() == L'"')
            return tok.substr(1, tok.size() - 2);

        // Chr() 치환
        std::wstring resolved = ExpandChrTokens(tok);
        if (resolved != tok) return resolved;

        // 변수 참조 (소문자 정규화)
        std::wstring key = tok;
        for (auto& c : key) c = (wchar_t)towlower(c);
        auto it = vars.find(key);
        if (it != vars.end()) return it->second;

        return L"";
    }

    // 표현식 전체 평가 (토큰 나열 → 연결)
    static std::wstring EvalExpr(const std::wstring& expr,
                                  const std::map<std::wstring, std::wstring>& vars)
    {
        std::wstring result;
        for (auto& tok : TokenizeExpr(expr))
            result += EvalToken(tok, vars);
        return result;
    }

    // %XX → 문자 퍼센트 디코딩
    // 0x00-0x7F: 직접 캐스트 / 0x80+: UTF-8 멀티바이트 → MultiByteToWideChar
    static std::wstring PercentDecode(const std::wstring& s)
    {
        std::wstring out;
        std::string  utf8;
        out.reserve(s.size());

        auto flushUtf8 = [&]() {
            if (utf8.empty()) return;
            int n = MultiByteToWideChar(CP_UTF8, 0,
                                        utf8.c_str(), (int)utf8.size(),
                                        nullptr, 0);
            if (n > 0) {
                std::wstring tmp(n, L'\0');
                MultiByteToWideChar(CP_UTF8, 0,
                                    utf8.c_str(), (int)utf8.size(),
                                    &tmp[0], n);
                out += tmp;
            }
            utf8.clear();
        };

        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == L'%' && i + 2 < s.size() &&
                iswxdigit(s[i+1]) && iswxdigit(s[i+2]))
            {
                wchar_t hex[3] = { s[i+1], s[i+2], L'\0' };
                unsigned code  = wcstoul(hex, nullptr, 16);
                i += 2;

                if (code <= 0x7F) {
                    flushUtf8();
                    out += (wchar_t)code;
                } else {
                    utf8 += (char)(unsigned char)code;
                }
            } else {
                flushUtf8();
                out += s[i];
            }
        }
        flushUtf8();
        return out;
    }

    // URL 끝 구두점 제거 + 퍼센트 디코딩
    static std::wstring CleanUrl(const std::wstring& url)
    {
        std::wstring s = url;
        while (!s.empty()) {
            wchar_t c = s.back();
            if (c == L'.' || c == L')' || c == L',' || c == L';' || c == L'\'')
                s.pop_back();
            else break;
        }
        return PercentDecode(s);
    }

    // 텍스트에서 https?:// 패턴 직접 탐지
    static std::vector<std::wstring> FindDirectUrls(const std::wstring& text)
    {
        std::vector<std::wstring> urls;
        static const wchar_t* schemes[] = { L"https://", L"http://", nullptr };

        for (int si = 0; schemes[si]; ++si) {
            const std::wstring scheme = schemes[si];
            size_t pos = 0;
            while ((pos = text.find(scheme, pos)) != std::wstring::npos) {
                size_t end = pos + scheme.size();
                while (end < text.size()) {
                    wchar_t c = text[end];
                    if (iswspace(c) || c == L'"' || c == L'\'' ||
                        c == L'<'  || c == L'>') break;
                    ++end;
                }
                // scheme 이후 1자 이상 있어야 유효
                if (end > pos + scheme.size())
                    urls.push_back(text.substr(pos, end - pos));
                pos = end;
            }
        }
        return urls;
    }

    // 변수 대입 파싱: "varname = rhs" → true
    static bool ParseAssignment(const std::wstring& line,
                                 std::wstring& outVar, std::wstring& outRhs)
    {
        size_t i = 0;
        while (i < line.size() && iswspace(line[i])) ++i;

        size_t nameStart = i;
        while (i < line.size() && (iswalnum(line[i]) || line[i] == L'_')) ++i;
        if (i == nameStart) return false;
        std::wstring name = line.substr(nameStart, i - nameStart);

        while (i < line.size() && iswspace(line[i])) ++i;
        if (i >= line.size() || line[i] != L'=') return false;
        ++i;
        while (i < line.size() && iswspace(line[i])) ++i;

        if (i >= line.size()) return false;
        outVar = name;
        outRhs = line.substr(i);
        return true;
    }

    // resolved 기준 중복 체크
    static bool HasUrl(const std::vector<KeyDecoder::UrlInfo>& urls,
                       const std::wstring& resolved)
    {
        for (auto& u : urls)
            if (u.resolved == resolved) return true;
        return false;
    }

    // ── 메인 ──────────────────────────────────────────────────────────────────

    void Extract(std::vector<KeyDecoder::DecodedLine>& lines)
    {
        // 변수 저장소: 변수명(소문자) → 평가된 값
        std::map<std::wstring, std::wstring> vars;

        for (size_t lineIdx = 0; lineIdx < lines.size(); ++lineIdx) {
            auto& dl              = lines[lineIdx];
            const std::wstring& text = dl.text;

            // ① 직접 URL 탐지
            for (auto& raw : FindDirectUrls(text)) {
                std::wstring resolved = CleanUrl(raw);
                if (!HasUrl(dl.urls, resolved))
                    dl.urls.push_back({ raw, resolved, lineIdx });
            }

            // ② Chr() 치환 후 재탐지
            std::wstring expanded = ExpandChrTokens(text);
            if (expanded != text) {
                for (auto& raw : FindDirectUrls(expanded)) {
                    std::wstring resolved = CleanUrl(raw);
                    if (!HasUrl(dl.urls, resolved))
                        dl.urls.push_back({ raw, resolved, lineIdx });
                }
            }

            // ③ 변수 대입 추적
            std::wstring varName, rhs;
            if (ParseAssignment(text, varName, rhs)) {
                std::wstring key = varName;
                for (auto& c : key) c = (wchar_t)towlower(c);

                std::wstring val = EvalExpr(rhs, vars);
                if (!val.empty()) {
                    vars[key] = val;

                    // 평가된 변수 값에서 URL 탐지
                    for (auto& raw : FindDirectUrls(val)) {
                        std::wstring resolved = CleanUrl(raw);
                        if (!HasUrl(dl.urls, resolved))
                            dl.urls.push_back({ rhs, resolved, lineIdx });
                    }
                }
            }
        }
    }
}

#include "entropy_filter.h"
#include <map>
#include <cmath>

namespace UrlClassifier::EntropyFilter
{
    static constexpr double ENTROPY_THRESHOLD = 3.5;
    static constexpr size_t MIN_LABEL_LENGTH  = 8;

    // 메인 라벨 추출: 마지막에서 두 번째 점 다음 ~ 마지막 점 사이
    // 예: "kj3xq8a9z.com"       → "kj3xq8a9z"
    //     "sub.evil-domain.com" → "evil-domain"
    static std::wstring ExtractMainLabel(const std::wstring& host)
    {
        size_t lastDot = host.rfind(L'.');
        if (lastDot == std::wstring::npos) return host;
        size_t prevDot = host.rfind(L'.', lastDot - 1);
        if (prevDot == std::wstring::npos)
            return host.substr(0, lastDot);
        return host.substr(prevDot + 1, lastDot - prevDot - 1);
    }

    static double ShannonEntropy(const std::wstring& s)
    {
        if (s.empty()) return 0.0;
        std::map<wchar_t, int> freq;
        for (auto c : s) freq[c]++;
        double H = 0.0;
        for (auto& kv : freq) {
            double p = static_cast<double>(kv.second) / s.size();
            H -= p * std::log2(p);
        }
        return H;
    }

    uint32_t Check(const ParsedUrl& p)
    {
        if (p.host.empty()) return 0;

        std::wstring label = ExtractMainLabel(p.host);
        if (label.size() < MIN_LABEL_LENGTH) return 0;

        double H = ShannonEntropy(label);
        if (H > ENTROPY_THRESHOLD)
            return URL_FLAG_HIGH_ENTROPY;
        return 0;
    }
}

// CapstoneTest — 알고리즘 파이프라인 단독 테스트
// 사용법:
//   CapstoneTest.exe                          # 하드코딩 샘플 실행
//   CapstoneTest.exe --seq "Win R N O T E P A D Enter"
//   CapstoneTest.exe path/to/input.txt        # 단일 파일 (UTF-8)
//   CapstoneTest.exe path/to/inputDir/        # 배치 (서브폴더 = 라벨)
//   CapstoneTest.exe --interval 5             # 키 간격 ms (기본 50)
//
// 배치 모드:
//   inputDir/<labelName>/<file>.txt 구조 가정.
//   서브폴더 이름이 라벨 (BadUSB / GoodUSB 등).
//   라벨에 "Bad" 가 포함되면 악성 가정, 외 정상 가정.
//   최종 통계: 라벨별 정탐률 / 오탐률.

#include "test_key_parser.h"
#include "test_runner.h"
#include "threat_matcher.h"
#include "compound_scorer.h"

#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>

namespace fs = std::filesystem;

static std::wstring ReadFile(const std::wstring& path)
{
    // _wfopen 으로 wide path 직접 사용 (narrow 변환 회피)
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"rb") != 0 || !fp) return {};

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) { fclose(fp); return {}; }

    std::string utf8(size, '\0');
    size_t got = fread(utf8.data(), 1, (size_t)size, fp);
    fclose(fp);
    if (got == 0) return {};

    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)got,
                                 nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)got,
                        out.data(), n);
    return out;
}

static const wchar_t* HARDCODED_SAMPLE =
    L"Win R P O W E R S H E L L Space - E P Space B Y P A S S Enter";

// 단일 입력 시퀀스 실행 + 출력. 헤더 라인은 호출자가 미리 출력.
// 결과 Verdict 만 반환 (배치 통계용).
static CompoundScorer::Verdict RunSingle(const std::wstring& inputSeq,
                                          DWORD intervalMs)
{
    TestKeyParser::Options opts;
    opts.intervalMs = intervalMs;
    auto keys = TestKeyParser::ParseSequence(inputSeq, opts);
    if (keys.empty()) {
        std::wcout << L"  (no keys parsed)\r\n";
        return {};
    }
    auto result = TestRunner::Run(keys);
    std::wcout << TestRunner::FormatResult(keys, result);
    return result.verdict;
}

// 배치 결과 1건
struct FileResult {
    std::wstring label;
    std::wstring filename;
    CompoundScorer::Level level;
    double                final_score;
};

static bool IsMalLabel(const std::wstring& label)
{
    // 라벨에 "Bad" 가 들어가면 악성 샘플로 간주 (대소문자 무관)
    std::wstring l;
    for (auto c : label)
        l.push_back((c >= L'A' && c <= L'Z') ? (c - L'A' + L'a') : c);
    return l.find(L"bad") != std::wstring::npos;
}

static void PrintStatistics(const std::vector<FileResult>& results)
{
    std::map<std::wstring, std::vector<const FileResult*>> byLabel;
    for (const auto& r : results) byLabel[r.label].push_back(&r);

    std::wcout << L"\r\n\r\n"
               << std::wstring(60, L'=') << L"\r\n"
               << L" 최종 통계 (Final Statistics)\r\n"
               << std::wstring(60, L'=') << L"\r\n";

    for (const auto& [label, list] : byLabel) {
        int total = (int)list.size();
        int dangerous = 0;  // WARN 이상
        int safe = 0;
        for (const auto* fr : list) {
            if (fr->level >= CompoundScorer::WARN) ++dangerous;
            else ++safe;
        }

        bool isMal = IsMalLabel(label);
        double rate = (total > 0) ? (100.0 * dangerous / total) : 0.0;

        wchar_t line[256];
        if (isMal) {
            _snwprintf_s(line, _TRUNCATE,
                L"  [%s] 악성 샘플 %d개 중 %d개 위험 판정 → 정탐률: %.2f%%\r\n",
                label.c_str(), total, dangerous, rate);
        } else {
            _snwprintf_s(line, _TRUNCATE,
                L"  [%s] 정상 샘플 %d개 중 %d개 위험 판정 → 오탐률: %.2f%%\r\n",
                label.c_str(), total, dangerous, rate);
        }
        std::wcout << line;
    }
    std::wcout << std::wstring(60, L'=') << L"\r\n";
}

// 배치 모드 실행: rootDir/<label>/<file>.txt 순회
static int RunBatch(const std::wstring& rootDir, DWORD intervalMs)
{
    std::vector<FileResult> results;

    std::vector<std::wstring> subdirs;
    try {
        for (auto& e : fs::directory_iterator(rootDir)) {
            if (e.is_directory()) subdirs.push_back(e.path().wstring());
        }
    } catch (const std::exception&) {
        std::wcerr << L"Cannot scan directory: " << rootDir << L"\r\n";
        return 4;
    }
    std::sort(subdirs.begin(), subdirs.end());

    for (const auto& subdir : subdirs) {
        std::wstring label = fs::path(subdir).filename().wstring();

        std::vector<std::wstring> files;
        for (auto& e : fs::directory_iterator(subdir)) {
            if (!e.is_regular_file()) continue;
            if (e.path().extension() != L".txt") continue;
            files.push_back(e.path().wstring());
        }
        std::sort(files.begin(), files.end());

        for (const auto& filepath : files) {
            std::wstring filename = fs::path(filepath).filename().wstring();

            std::wcout << L"\r\n" << std::wstring(60, L'#') << L"\r\n"
                       << L"# [" << label << L"] " << filename << L"\r\n"
                       << std::wstring(60, L'#') << L"\r\n";

            std::wstring seq = ReadFile(filepath);
            if (seq.empty()) {
                std::wcout << L"  (cannot read file)\r\n";
                continue;
            }

            auto verdict = RunSingle(seq, intervalMs);

            FileResult fr;
            fr.label       = label;
            fr.filename    = filename;
            fr.level       = verdict.level;
            fr.final_score = verdict.final_score;
            results.push_back(fr);
        }
    }

    PrintStatistics(results);
    return 0;
}

// wstring → UTF-8 변환
static std::string ToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                 nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                        out.data(), n, nullptr, nullptr);
    return out;
}

int wmain(int argc, wchar_t* argv[])
{
    // 콘솔 UTF-16 출력 설정 (한글 정상 표시)
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stderr), _O_U16TEXT);

    // 인자 파싱
    std::wstring inputArg;       // 파일 또는 폴더
    std::wstring inputSeq;       // --seq 로 받은 시퀀스
    std::wstring outPath;        // --out 으로 받은 출력 파일
    DWORD intervalMs = 50;

    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--seq" && i + 1 < argc) {
            inputSeq = argv[++i];
        } else if (a == L"--interval" && i + 1 < argc) {
            intervalMs = (DWORD)_wtoi(argv[++i]);
        } else if (a == L"--out" && i + 1 < argc) {
            outPath = argv[++i];
        } else if (a.size() > 2 && a.substr(0, 2) == L"--") {
            std::wcerr << L"Unknown option: " << a << L"\n";
            return 1;
        } else {
            inputArg = a;
        }
    }

    // --out 지정 시: stdout 을 파일로 리다이렉트 (UTF-8 BOM 자동)
    if (!outPath.empty()) {
        FILE* fp = nullptr;
        if (_wfreopen_s(&fp, outPath.c_str(), L"w", stdout) != 0 || !fp) {
            std::wcerr << L"Cannot open output file: " << outPath << L"\n";
            return 5;
        }
        // wchar 출력 → UTF-8 변환 + BOM 자동 기록
        _setmode(_fileno(stdout), _O_U8TEXT);
    }

    // ThreatMatcher DB 경로 (실행 디렉토리)
    ThreatMatcher::SetDatabasePath(L"matchingDB.db");

    // 배치 모드: 폴더 인자
    if (!inputArg.empty() && fs::exists(inputArg) && fs::is_directory(inputArg)) {
        std::wcout << L"Batch mode: " << inputArg << L"\r\n";
        std::wcout << L"Interval: " << intervalMs << L"ms\r\n";
        int rc = RunBatch(inputArg, intervalMs);
        ThreatMatcher::Shutdown();
        return rc;
    }

    // 단일 파일 모드
    if (!inputArg.empty()) {
        inputSeq = ReadFile(inputArg);
        if (inputSeq.empty()) {
            std::wcerr << L"Cannot read file: " << inputArg << L"\n";
            return 2;
        }
    }

    if (inputSeq.empty()) {
        std::wcout << L"(no input given, running hardcoded sample)\r\n\r\n";
        inputSeq = HARDCODED_SAMPLE;
    }

    std::wcout << L"Input sequence: " << inputSeq << L"\r\n";
    std::wcout << L"Interval: " << intervalMs << L"ms\r\n\r\n";

    RunSingle(inputSeq, intervalMs);

    ThreatMatcher::Shutdown();
    return 0;
}

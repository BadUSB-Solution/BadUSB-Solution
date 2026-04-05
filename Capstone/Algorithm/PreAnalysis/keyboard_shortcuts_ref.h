#pragma once
#include "framework.h"

// =============================================================================
// keyboard_shortcuts_ref.h
// Windows 키보드 단축키 레퍼런스 테이블
// KeyNormalizer 에서 HotkeyEvent 감지·레이블링에 사용
//
// 참고 출처: BadUSB-Solution 프로젝트 내부 단축키 문서
// =============================================================================

namespace KeyShortcutRef {

// -----------------------------------------------------------------------------
// Modifier 플래그 (OR 조합 가능)
// -----------------------------------------------------------------------------
static constexpr uint8_t M_NONE  = 0x00;
static constexpr uint8_t M_WIN   = 0x01;
static constexpr uint8_t M_CTRL  = 0x02;
static constexpr uint8_t M_ALT   = 0x04;
static constexpr uint8_t M_SHIFT = 0x08;

// -----------------------------------------------------------------------------
// 단축키 정의 구조체
// -----------------------------------------------------------------------------
struct ShortcutDef {
    uint8_t        mods;    // Modifier 조합 (M_WIN | M_CTRL | ...)
    DWORD          vk;      // 가상 키코드 (VK_*). 0 = 키 없음(modifier만)
    const wchar_t* combo;   // 표시 이름  예) L"Win+R"
    const wchar_t* action;  // 동작 설명
};

// =============================================================================
// 1. Win 단독 단축키
// =============================================================================
static constexpr ShortcutDef WIN_SHORTCUTS[] = {
    { M_WIN, 0,           L"Win",          L"시작 메뉴 열기/닫기" },
    { M_WIN, 'D',         L"Win+D",        L"바탕화면 보기/숨기기 토글" },
    { M_WIN, 'E',         L"Win+E",        L"파일 탐색기 열기" },
    { M_WIN, 'R',         L"Win+R",        L"실행(Run) 대화상자 열기" },        // BadUSB 핵심
    { M_WIN, 'S',         L"Win+S",        L"검색 열기" },
    { M_WIN, 'L',         L"Win+L",        L"화면 잠금" },
    { M_WIN, 'I',         L"Win+I",        L"Windows 설정 열기" },
    { M_WIN, 'A',         L"Win+A",        L"알림 센터 열기" },
    { M_WIN, 'N',         L"Win+N",        L"알림 패널 열기 (Windows 11)" },
    { M_WIN, 'K',         L"Win+K",        L"캐스트/연결 패널 열기" },
    { M_WIN, 'P',         L"Win+P",        L"프레젠테이션 디스플레이 모드 전환" },
    { M_WIN, 'V',         L"Win+V",        L"클립보드 기록 열기" },
    { M_WIN, 'T',         L"Win+T",        L"작업 표시줄 앱 순환" },
    { M_WIN, 'B',         L"Win+B",        L"시스템 트레이로 포커스 이동" },
    { M_WIN, 'G',         L"Win+G",        L"Xbox 게임 바 열기" },
    { M_WIN, 'H',         L"Win+H",        L"음성 받아쓰기 열기" },
    { M_WIN, 'U',         L"Win+U",        L"접근성 설정 열기" },
    { M_WIN, 'X',         L"Win+X",        L"고급 사용자 메뉴(파워 메뉴) 열기" }, // BadUSB 핵심
    { M_WIN, 'M',         L"Win+M",        L"모든 창 최소화" },
    { M_WIN, 'F',         L"Win+F",        L"피드백 허브 열기" },
    { M_WIN, 'W',         L"Win+W",        L"Windows 잉크 작업 영역 / 위젯 보드 열기" },
    { M_WIN, 'C',         L"Win+C",        L"Microsoft Copilot" },
    { M_WIN, 'Z',         L"Win+Z",        L"스냅 레이아웃 메뉴 열기 (Windows 11)" },
    { M_WIN, VK_TAB,      L"Win+Tab",      L"작업 보기(타임라인) 열기" },
    { M_WIN, VK_SPACE,    L"Win+Space",    L"입력 언어/키보드 레이아웃 전환" },
    { M_WIN, VK_UP,       L"Win+Up",       L"창 최대화" },
    { M_WIN, VK_DOWN,     L"Win+Down",     L"창 최소화 / 이전 크기로 복원" },
    { M_WIN, VK_LEFT,     L"Win+Left",     L"창을 화면 왼쪽 절반으로 스냅" },
    { M_WIN, VK_RIGHT,    L"Win+Right",    L"창을 화면 오른쪽 절반으로 스냅" },
    { M_WIN, VK_HOME,     L"Win+Home",     L"현재 창 제외 모두 최소화" },
    { M_WIN, VK_PAUSE,    L"Win+Pause",    L"시스템 속성 열기" },
    { M_WIN, VK_OEM_PERIOD, L"Win+.",      L"이모지·특수문자 피커 열기" },
    { M_WIN, VK_OEM_COMMA, L"Win+,",       L"바탕화면 잠깐 보기" },
    // 숫자 1-9: 작업 표시줄 n번째 앱 실행/전환
    { M_WIN, '1', L"Win+1", L"작업 표시줄 1번째 앱 실행/전환" },
    { M_WIN, '2', L"Win+2", L"작업 표시줄 2번째 앱 실행/전환" },
    { M_WIN, '3', L"Win+3", L"작업 표시줄 3번째 앱 실행/전환" },
    { M_WIN, '4', L"Win+4", L"작업 표시줄 4번째 앱 실행/전환" },
    { M_WIN, '5', L"Win+5", L"작업 표시줄 5번째 앱 실행/전환" },
    { M_WIN, '6', L"Win+6", L"작업 표시줄 6번째 앱 실행/전환" },
    { M_WIN, '7', L"Win+7", L"작업 표시줄 7번째 앱 실행/전환" },
    { M_WIN, '8', L"Win+8", L"작업 표시줄 8번째 앱 실행/전환" },
    { M_WIN, '9', L"Win+9", L"작업 표시줄 9번째 앱 실행/전환" },
    // 돋보기
    { M_WIN, VK_OEM_PLUS,  L"Win++",  L"돋보기 켜기 / 확대" },
    { M_WIN, VK_OEM_MINUS, L"Win+-",  L"돋보기 축소" },
    { M_WIN, VK_ESCAPE,    L"Win+Esc", L"돋보기 끄기" },
    { M_WIN, VK_RETURN,    L"Win+Enter", L"내레이터 시작/중지" },
};

// =============================================================================
// 2. Win+Shift 단축키
// =============================================================================
static constexpr ShortcutDef WIN_SHIFT_SHORTCUTS[] = {
    { M_WIN|M_SHIFT, 'M',      L"Win+Shift+M",   L"최소화된 창 모두 복원" },
    { M_WIN|M_SHIFT, 'S',      L"Win+Shift+S",   L"캡처 도구 열기" },           // BadUSB 관련
    { M_WIN|M_SHIFT, 'R',      L"Win+Shift+R",   L"화면 녹화 열기" },
    { M_WIN|M_SHIFT, VK_UP,    L"Win+Shift+Up",  L"창을 수직으로 최대화" },
    { M_WIN|M_SHIFT, VK_DOWN,  L"Win+Shift+Down", L"수직 최대화 복원" },
    { M_WIN|M_SHIFT, VK_LEFT,  L"Win+Shift+Left", L"창을 왼쪽 모니터로 이동" },
    { M_WIN|M_SHIFT, VK_RIGHT, L"Win+Shift+Right", L"창을 오른쪽 모니터로 이동" },
    // 작업 표시줄 n번째 앱 새 인스턴스
    { M_WIN|M_SHIFT, '1', L"Win+Shift+1", L"작업 표시줄 1번째 앱 새 인스턴스 시작" },
    { M_WIN|M_SHIFT, '2', L"Win+Shift+2", L"작업 표시줄 2번째 앱 새 인스턴스 시작" },
    { M_WIN|M_SHIFT, '3', L"Win+Shift+3", L"작업 표시줄 3번째 앱 새 인스턴스 시작" },
};

// =============================================================================
// 3. Win+Ctrl 단축키
// =============================================================================
static constexpr ShortcutDef WIN_CTRL_SHORTCUTS[] = {
    { M_WIN|M_CTRL, 'D',      L"Win+Ctrl+D",      L"새 가상 데스크톱 추가" },
    { M_WIN|M_CTRL, 'F',      L"Win+Ctrl+F",      L"네트워크 상의 PC 검색" },
    { M_WIN|M_CTRL, 'Q',      L"Win+Ctrl+Q",      L"Quick Assist (원격 지원) 열기" },
    { M_WIN|M_CTRL, 'V',      L"Win+Ctrl+V",      L"사운드 출력 페이지(볼륨 믹서)" },
    { M_WIN|M_CTRL, VK_RIGHT, L"Win+Ctrl+Right",  L"오른쪽 가상 데스크톱으로 이동" },
    { M_WIN|M_CTRL, VK_LEFT,  L"Win+Ctrl+Left",   L"왼쪽 가상 데스크톱으로 이동" },
    { M_WIN|M_CTRL, VK_F4,    L"Win+Ctrl+F4",     L"현재 가상 데스크톱 닫기" },
};

// =============================================================================
// 4. Win+Ctrl+Shift 단축키
// =============================================================================
static constexpr ShortcutDef WIN_CTRL_SHIFT_SHORTCUTS[] = {
    { M_WIN|M_CTRL|M_SHIFT, 'B',      L"Win+Ctrl+Shift+B", L"그래픽 드라이버 재시작" },
    { M_WIN|M_CTRL|M_SHIFT, VK_LEFT,  L"Win+Ctrl+Shift+Left",  L"현재 창을 왼쪽 데스크톱으로 이동" },
    { M_WIN|M_CTRL|M_SHIFT, VK_RIGHT, L"Win+Ctrl+Shift+Right", L"현재 창을 오른쪽 데스크톱으로 이동" },
};

// =============================================================================
// 5. Win+Alt 단축키
// =============================================================================
static constexpr ShortcutDef WIN_ALT_SHORTCUTS[] = {
    { M_WIN|M_ALT, 'R',        L"Win+Alt+R",      L"게임 화면 녹화 시작/중지" },
    { M_WIN|M_ALT, 'G',        L"Win+Alt+G",      L"최근 30초 게임 클립 저장" },
    { M_WIN|M_ALT, VK_UP,      L"Win+Alt+Up",     L"창을 위쪽 절반으로 스냅" },
    { M_WIN|M_ALT, VK_DOWN,    L"Win+Alt+Down",   L"창을 아래쪽 절반으로 스냅" },
    { M_WIN|M_ALT, VK_SNAPSHOT, L"Win+Alt+PrtSc", L"현재 창 캡처 → 파일 자동 저장" },
    // 작업 표시줄 n번째 앱 점프 리스트
    { M_WIN|M_ALT, '1', L"Win+Alt+1", L"작업 표시줄 1번째 앱 점프 리스트 열기" },
    { M_WIN|M_ALT, '2', L"Win+Alt+2", L"작업 표시줄 2번째 앱 점프 리스트 열기" },
    { M_WIN|M_ALT, '3', L"Win+Alt+3", L"작업 표시줄 3번째 앱 점프 리스트 열기" },
};

// =============================================================================
// 6. Ctrl 단독 단축키
// =============================================================================
static constexpr ShortcutDef CTRL_SHORTCUTS[] = {
    // ── 기본 편집 ──
    { M_CTRL, 'C',          L"Ctrl+C",   L"복사" },
    { M_CTRL, 'X',          L"Ctrl+X",   L"잘라내기" },
    { M_CTRL, 'V',          L"Ctrl+V",   L"붙여넣기" },
    { M_CTRL, 'Z',          L"Ctrl+Z",   L"실행 취소" },
    { M_CTRL, 'Y',          L"Ctrl+Y",   L"다시 실행" },
    { M_CTRL, 'A',          L"Ctrl+A",   L"모두 선택" },
    { M_CTRL, 'S',          L"Ctrl+S",   L"저장" },                // BadUSB 관련
    { M_CTRL, 'N',          L"Ctrl+N",   L"새 파일 / 새 창" },
    { M_CTRL, 'O',          L"Ctrl+O",   L"열기" },
    { M_CTRL, 'P',          L"Ctrl+P",   L"인쇄" },
    { M_CTRL, 'F',          L"Ctrl+F",   L"찾기" },
    { M_CTRL, 'H',          L"Ctrl+H",   L"찾기 및 바꾸기" },
    { M_CTRL, 'W',          L"Ctrl+W",   L"현재 창 또는 탭 닫기" },
    { M_CTRL, 'B',          L"Ctrl+B",   L"굵게 (Bold)" },
    { M_CTRL, 'I',          L"Ctrl+I",   L"기울임 (Italic)" },
    { M_CTRL, 'U',          L"Ctrl+U",   L"밑줄 (Underline)" },
    { M_CTRL, 'K',          L"Ctrl+K",   L"하이퍼링크 삽입" },
    { M_CTRL, 'L',          L"Ctrl+L",   L"주소창으로 이동 (브라우저)" },
    { M_CTRL, 'R',          L"Ctrl+R",   L"페이지 새로고침 (브라우저)" },
    { M_CTRL, 'D',          L"Ctrl+D",   L"즐겨찾기 추가 (브라우저)" },
    { M_CTRL, 'G',          L"Ctrl+G",   L"줄 이동 (메모장) / 다음 검색 결과 (브라우저)" },
    { M_CTRL, 'E',          L"Ctrl+E",   L"검색창으로 포커스 이동 (탐색기)" },
    { M_CTRL, 'T',          L"Ctrl+T",   L"새 탭 열기" },
    { M_CTRL, 'M',          L"Ctrl+M",   L"마크 모드 (터미널) / 모두 선택 (터미널)" },
    { M_CTRL, VK_TAB,       L"Ctrl+Tab",  L"다음 탭으로 이동" },
    { M_CTRL, VK_INSERT,    L"Ctrl+Ins",  L"복사 (대체 키)" },
    { M_CTRL, VK_HOME,      L"Ctrl+Home", L"문서 맨 처음으로 / 터미널 맨 위로 스크롤" },
    { M_CTRL, VK_END,       L"Ctrl+End",  L"문서 맨 끝으로 / 터미널 맨 아래로 스크롤" },
    { M_CTRL, VK_LEFT,      L"Ctrl+Left",  L"한 단어씩 왼쪽 이동" },
    { M_CTRL, VK_RIGHT,     L"Ctrl+Right", L"한 단어씩 오른쪽 이동" },
    { M_CTRL, VK_BACK,      L"Ctrl+Backspace", L"커서 앞 단어 삭제" },
    { M_CTRL, VK_DELETE,    L"Ctrl+Delete",    L"커서 뒤 단어 삭제" },
    { M_CTRL, VK_OEM_PLUS,  L"Ctrl++",   L"확대" },
    { M_CTRL, VK_OEM_MINUS, L"Ctrl+-",   L"축소" },
    { M_CTRL, '0',          L"Ctrl+0",   L"원래 크기로 복원" },
    { M_CTRL, VK_F1,        L"Ctrl+F1",  L"리본 메뉴 표시/숨기기 (탐색기)" },
    // 숫자 탭 이동 (브라우저)
    { M_CTRL, '1', L"Ctrl+1", L"1번째 탭으로 이동" },
    { M_CTRL, '2', L"Ctrl+2", L"2번째 탭으로 이동" },
    { M_CTRL, '3', L"Ctrl+3", L"3번째 탭으로 이동" },
    { M_CTRL, '4', L"Ctrl+4", L"4번째 탭으로 이동" },
    { M_CTRL, '5', L"Ctrl+5", L"5번째 탭으로 이동" },
    { M_CTRL, '6', L"Ctrl+6", L"6번째 탭으로 이동" },
    { M_CTRL, '7', L"Ctrl+7", L"7번째 탭으로 이동" },
    { M_CTRL, '8', L"Ctrl+8", L"8번째 탭으로 이동" },
    { M_CTRL, '9', L"Ctrl+9", L"마지막 탭으로 이동" },
};

// =============================================================================
// 7. Ctrl+Shift 단축키
// =============================================================================
static constexpr ShortcutDef CTRL_SHIFT_SHORTCUTS[] = {
    { M_CTRL|M_SHIFT, 'V',      L"Ctrl+Shift+V", L"서식 없이 붙여넣기" },
    { M_CTRL|M_SHIFT, 'S',      L"Ctrl+Shift+S", L"다른 이름으로 저장" },
    { M_CTRL|M_SHIFT, 'N',      L"Ctrl+Shift+N", L"새 폴더 만들기 (탐색기) / 시크릿 창 (브라우저)" },
    { M_CTRL|M_SHIFT, 'T',      L"Ctrl+Shift+T", L"최근 닫은 탭 복원 / 새 탭 (터미널)" },
    { M_CTRL|M_SHIFT, 'W',      L"Ctrl+Shift+W", L"탭 닫기 (터미널)" },
    { M_CTRL|M_SHIFT, 'D',      L"Ctrl+Shift+D", L"모든 탭 즐겨찾기에 추가" },
    { M_CTRL|M_SHIFT, 'B',      L"Ctrl+Shift+B", L"즐겨찾기 바 표시/숨기기" },
    { M_CTRL|M_SHIFT, 'R',      L"Ctrl+Shift+R", L"강력 새로고침 (캐시 무시)" },
    { M_CTRL|M_SHIFT, 'I',      L"Ctrl+Shift+I", L"개발자 도구 열기" },
    { M_CTRL|M_SHIFT, 'J',      L"Ctrl+Shift+J", L"콘솔 열기 (크롬)" },
    { M_CTRL|M_SHIFT, 'G',      L"Ctrl+Shift+G", L"이전 검색 결과 (브라우저)" },
    { M_CTRL|M_SHIFT, 'C',      L"Ctrl+Shift+C", L"복사 (터미널)" },
    { M_CTRL|M_SHIFT, VK_TAB,   L"Ctrl+Shift+Tab", L"이전 탭으로 이동" },
    { M_CTRL|M_SHIFT, VK_HOME,  L"Ctrl+Shift+Home", L"문서 처음까지 선택" },
    { M_CTRL|M_SHIFT, VK_END,   L"Ctrl+Shift+End",  L"문서 끝까지 선택" },
    { M_CTRL|M_SHIFT, VK_LEFT,  L"Ctrl+Shift+Left",  L"단어 단위 왼쪽 선택" },
    { M_CTRL|M_SHIFT, VK_RIGHT, L"Ctrl+Shift+Right", L"단어 단위 오른쪽 선택" },
    { M_CTRL|M_SHIFT, VK_ESCAPE, L"Ctrl+Shift+Esc",  L"작업 관리자 바로 열기" }, // BadUSB 관련
};

// =============================================================================
// 8. Ctrl+Alt 단축키
// =============================================================================
static constexpr ShortcutDef CTRL_ALT_SHORTCUTS[] = {
    { M_CTRL|M_ALT, VK_DELETE, L"Ctrl+Alt+Del", L"보안 옵션 화면 (잠금·로그아웃·작업관리자)" },
    { M_CTRL|M_ALT, VK_TAB,    L"Ctrl+Alt+Tab", L"Alt+Tab 목록 고정 표시" },
    { M_CTRL|M_ALT, 'F',       L"Ctrl+Alt+F",   L"돋보기 전체 화면 보기로 전환" },
    { M_CTRL|M_ALT, 'L',       L"Ctrl+Alt+L",   L"돋보기 렌즈 보기로 전환" },
    { M_CTRL|M_ALT, 'D',       L"Ctrl+Alt+D",   L"돋보기 도킹 보기로 전환" },
};

// =============================================================================
// 9. Alt 단독 단축키
// =============================================================================
static constexpr ShortcutDef ALT_SHORTCUTS[] = {
    { M_ALT, VK_F4,     L"Alt+F4",      L"현재 앱 종료 / 종료·재시작 대화상자 (바탕화면)" }, // BadUSB 핵심
    { M_ALT, VK_TAB,    L"Alt+Tab",     L"열린 창 전환" },
    { M_ALT, VK_ESCAPE, L"Alt+Esc",     L"창 Z순서 순환 전환" },
    { M_ALT, VK_SPACE,  L"Alt+Space",   L"창 시스템 메뉴 열기" },
    { M_ALT, VK_RETURN, L"Alt+Enter",   L"선택 항목 속성 보기 (탐색기)" },
    { M_ALT, VK_UP,     L"Alt+Up",      L"상위 폴더로 이동 (탐색기)" },
    { M_ALT, VK_LEFT,   L"Alt+Left",    L"뒤로 가기" },
    { M_ALT, VK_RIGHT,  L"Alt+Right",   L"앞으로 가기" },
    { M_ALT, VK_HOME,   L"Alt+Home",    L"홈 페이지로 이동 (브라우저)" },
    { M_ALT, 'D',       L"Alt+D",       L"주소 표시줄로 포커스 이동 (탐색기/브라우저)" },
    { M_ALT, VK_SNAPSHOT, L"Alt+PrtSc", L"현재 활성 창만 캡처 → 클립보드" },
};

// =============================================================================
// 10. Alt+Shift 단축키
// =============================================================================
static constexpr ShortcutDef ALT_SHIFT_SHORTCUTS[] = {
    { M_ALT|M_SHIFT, VK_TAB,       L"Alt+Shift+Tab",   L"열린 창 역방향 전환" },
    { M_ALT|M_SHIFT, VK_OEM_PLUS,  L"Alt+Shift++",     L"창 수평 분할 (터미널)" },
    { M_ALT|M_SHIFT, VK_OEM_MINUS, L"Alt+Shift+-",     L"창 수직 분할 (터미널)" },
};

// =============================================================================
// 11. Shift 단독 특수키
// =============================================================================
static constexpr ShortcutDef SHIFT_SHORTCUTS[] = {
    { M_SHIFT, VK_DELETE,   L"Shift+Delete", L"완전 삭제 (휴지통 건너뜀)" },
    { M_SHIFT, VK_INSERT,   L"Shift+Ins",    L"붙여넣기 (대체 키)" },
    { M_SHIFT, VK_TAB,      L"Shift+Tab",    L"내어쓰기 / 이전 컨트롤로 포커스 이동" },
    { M_SHIFT, VK_SPACE,    L"Shift+Space",  L"페이지 위로 스크롤" },
    { M_SHIFT, VK_HOME,     L"Shift+Home",   L"줄 처음까지 선택" },
    { M_SHIFT, VK_END,      L"Shift+End",    L"줄 끝까지 선택" },
    { M_SHIFT, VK_UP,       L"Shift+Up",     L"한 줄 위로 선택 확장" },
    { M_SHIFT, VK_DOWN,     L"Shift+Down",   L"한 줄 아래로 선택 확장" },
    { M_SHIFT, VK_LEFT,     L"Shift+Left",   L"한 글자씩 왼쪽 선택" },
    { M_SHIFT, VK_RIGHT,    L"Shift+Right",  L"한 글자씩 오른쪽 선택" },
    { M_SHIFT, VK_F10,      L"Shift+F10",    L"선택 항목 우클릭 메뉴 열기" },
    { M_SHIFT, VK_F3,       L"Shift+F3",     L"이전 찾기 (메모장)" },
};

// =============================================================================
// 12. Win+X 이후 서브메뉴 키 매핑
//     Win+X 감지 후 다음 단일 키 입력으로 메뉴 항목 선택
//     (KeyNormalizer에서 WIN_X 상태 진입 후 처리)
// =============================================================================
struct WinXMenuEntry {
    wchar_t        key;     // 선택 키 (대소문자 무관)
    const wchar_t* action;  // 실행 항목
    bool           isAdmin; // 관리자 권한 여부
};

static constexpr WinXMenuEntry WIN_X_MENU[] = {
    { 'B', L"모바일 센터",                    false },
    { 'O', L"전원 옵션",                      false },
    { 'V', L"이벤트 뷰어",                    false },
    { 'Y', L"시스템",                         false },
    { 'M', L"장치 관리자",                    false },
    { 'W', L"네트워크 연결",                  false },
    { 'K', L"디스크 관리",                    false },
    { 'G', L"컴퓨터 관리",                    false },
    { 'J', L"터미널",                         false },
    { 'A', L"터미널 (관리자)",                true  }, // BadUSB 핵심: 관리자 PowerShell 즉시 열기
    { 'T', L"작업 관리자",                    false },
    { 'N', L"설정",                           false },
    { 'E', L"파일 탐색기",                    false },
    { 'S', L"검색",                           false },
    { 'R', L"실행(Run) 창",                   false },
    { 'U', L"종료 또는 로그아웃",             false },
    { 'D', L"데스크톱",                       false },
};

// =============================================================================
// 13. 단독 기능키 (Modifier 없음)
// =============================================================================
static constexpr ShortcutDef SOLO_KEYS[] = {
    { M_NONE, VK_F2,       L"F2",       L"선택 항목 이름 바꾸기" },
    { M_NONE, VK_F5,       L"F5",       L"새로고침 / 날짜·시간 삽입 (메모장)" },
    { M_NONE, VK_F10,      L"F10",      L"메뉴 표시줄 활성화" },
    { M_NONE, VK_F11,      L"F11",      L"전체 화면 모드 토글" },
    { M_NONE, VK_F12,      L"F12",      L"다른 이름으로 저장 (메모장) / 개발자 도구" },
    { M_NONE, VK_SNAPSHOT, L"PrtSc",    L"전체 화면 캡처 → 클립보드" },
    { M_NONE, VK_ESCAPE,   L"Esc",      L"현재 작업 취소" },
    { M_NONE, VK_DELETE,   L"Delete",   L"삭제 (휴지통으로)" },
    { M_NONE, VK_INSERT,   L"Insert",   L"삽입/덮어쓰기 모드 전환" },
    { M_NONE, VK_CAPITAL,  L"CapsLock", L"대문자 잠금 토글" },
    { M_NONE, VK_SCROLL,   L"ScrollLock", L"스크롤 잠금 토글" },
    { M_NONE, VK_NUMLOCK,  L"NumLock",  L"숫자 키패드 잠금 토글" },
};

} // namespace KeyShortcutRef

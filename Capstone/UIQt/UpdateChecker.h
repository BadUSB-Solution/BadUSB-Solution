#pragma once

#include <QWidget>

// ============================================================================
//  UpdateChecker — 시작 시 latest.json 을 받아 새 버전이 있으면 안내.
//
//  흐름:
//   1) https://abang-all.vercel.app/latest.json 을 비동기 GET
//   2) { "version", "url", "notes" } 파싱 → 현재 버전(ABANGALL_VERSION)과 비교
//   3) 원격이 더 높으면 QMessageBox 로 안내 → [업데이트] 누르면 브라우저로 url 열기
//
//  네트워크 오류/파싱 실패/동일 버전은 조용히 무시한다(사용자 방해 X).
// ============================================================================

namespace UpdateChecker
{
    // 비동기 업데이트 확인. parent 는 안내 다이얼로그의 부모(없으면 nullptr).
    void checkAsync(QWidget* parent);
}

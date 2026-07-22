#ifndef BUNDLE_SCANNER_H
#define BUNDLE_SCANNER_H

#include "scan_report.h"   // ← ScanReport 정의 사용

/**
 * 번들 디렉터리 전체 스캔 (정적 분석, 배포 전 점검용)
 * @param rootDir  .app 번들들이 들어있는 루트 디렉터리 (예: "./eval-apps")
 * @param verbose  상세 로그 출력 여부
 * @return ScanReport (호출자 해제 책임: scan_report_free)
 */
ScanReport *scan_bundle_directory(const char *rootDir, bool verbose);

#endif // BUNDLE_SCANNER_H
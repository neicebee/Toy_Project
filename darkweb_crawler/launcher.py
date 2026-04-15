#!/usr/bin/env python3
"""
Darkweb Crawler - 실행 선택 스크립트
CLI 모드 또는 웹 모드 선택 가능
"""

import sys
import os

def show_menu():
    print("""
╔════════════════════════════════════════════════╗
║   🔍 Darkweb Crawler - 실행 모드 선택         ║
╚════════════════════════════════════════════════╝

1️⃣  CLI 모드 (명령줄)
    사용: python launcher.py 1 [도메인]
    예: python launcher.py 1 example.onion

2️⃣  웹 모드 (브라우저)
    사용: python launcher.py 2
    접속: http://localhost:5000

3️⃣  도움말

0️⃣  종료

선택: """)

def run_cli_mode():
    """CLI 모드 실행"""
    print("\n🔄 CLI 모드 시작...\n")
    
    if len(sys.argv) > 2:
        domain = sys.argv[2]
        print(f"도메인 분석: {domain}\n")
        os.system(f"python agent.py -d {domain}")
    else:
        print("도메인을 지정해주세요")
        print("사용: python launcher.py 1 example.onion\n")

def run_web_mode():
    """웹 모드 실행"""
    print("\n🌐 웹 모드 시작...\n")
    print("웹 브라우저에서 다음 주소로 접속하세요:")
    print("📍 http://localhost:5000\n")
    print("Ctrl+C를 눌러 종료할 수 있습니다\n")
    
    os.system("python web/app.py")

def show_help():
    """도움말 표시"""
    help_text = """
📖 사용 방법

1. CLI 모드 (명령줄)
   ─────────────────
   python launcher.py 1 example.onion
   
   - 한 번에 도메인 하나 분석
   - 빠른 테스트에 유용
   - 결과는 콘솔에 출력 + HTML 보고서 생성

2. 웹 모드 (브라우저)
   ──────────────────
   python launcher.py 2
   
   - 로컬 웹 서버 실행 (http://localhost:5000)
   - 여러 도메인 순차적으로 분석 가능
   - 웹 UI에서 결과 확인
   - 보고서 다운로드 가능

📋 분석 결과 정보
─────────────────
- 신뢰도 점수 (0-100)
- 접근성 상태
- 색인 정보 (Ahmia, DuckDuckGo)
- 사이트 카테고리 분류
- 콘텐츠 분석
- 추출된 상대 경로 목록

🛑 종료 방법
───────────
- CLI: 분석 완료 후 자동 종료
- 웹: Ctrl+C 입력

🔐 보안 참고
───────────
- Tor를 통한 익명 연결 자동 설정
- 로컬호스트에서만 실행
- 로그는 logs/ 디렉토리에 저장

💡 팁
────
- 첫 분석 시 초기화 과정으로 시간이 걸릴 수 있습니다
- 최근 분석 기록은 웹 모드의 localStorage에 저장됩니다
- 자세한 분석은 HTML 보고서를 다운로드하여 확인하세요
"""
    print(help_text)

def main():
    if len(sys.argv) < 2:
        while True:
            try:
                show_menu()
                choice = input().strip()
                
                if choice == '1':
                    if len(sys.argv) > 2:
                        run_cli_mode()
                    else:
                        domain = input("분석할 도메인을 입력하세요: ").strip()
                        if domain:
                            os.system(f"python agent.py -d {domain}")
                        else:
                            print("도메인을 입력해주세요\n")
                
                elif choice == '2':
                    run_web_mode()
                
                elif choice == '3':
                    show_help()
                
                elif choice == '0':
                    print("\n👋 프로그램을 종료합니다")
                    sys.exit(0)
                
                else:
                    print("❌ 잘못된 선택입니다\n")
            
            except KeyboardInterrupt:
                print("\n\n👋 프로그램을 종료합니다")
                sys.exit(0)
            except Exception as e:
                print(f"❌ 오류 발생: {str(e)}\n")
    else:
        choice = sys.argv[1]
        if choice == '1':
            run_cli_mode()
        elif choice == '2':
            run_web_mode()
        elif choice == '3':
            show_help()
        else:
            print("❌ 잘못된 인자입니다")
            print("사용: python launcher.py [1|2|3]")

if __name__ == '__main__':
    main()

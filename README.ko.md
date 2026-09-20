# ELRSChat

[English](README.md) | **한국어**

**ExpressLRS 기반의 독립적인 실험용 채팅 포크**

## ELRSChat 변경 경로 한눈에 보기

ExpressLRS 4.1.0에서 추가하거나 수정한 주요 경로입니다.

```text
ELRSChat/
├── patches/
│   └── elrs-4.1.0-chat.patch      # 채팅 소스 전체 diff
├── src/
│   ├── lua/
│   │   ├── ELRSChat.lua          # EdgeTX 실행 스크립트
│   │   └── ELRSChat/r16/         # 보조 파일 네 개 모두 필요
│   │       ├── common.lua
│   │       ├── core.lua
│   │       ├── transport.lua
│   │       └── view.lua
│   ├── lib/
│   │   ├── ElrsChat/            # 신규 채팅 엔진·프로토콜·RF 어댑터
│   │   ├── DEVICE/              # 일반 RC 일시 정지·재개
│   │   ├── tx-crsf/             # 채팅 파라미터 인터페이스
│   │   ├── LR1121Driver/        # 무선 드라이버 연동
│   │   ├── SX127xDriver/        # 무선 드라이버 연동
│   │   ├── SX1280Driver/        # 무선 드라이버 연동
│   │   └── SX12xxDriverCommon/  # 공통 무선 드라이버 연동
│   ├── include/deferred.h      # 지연 실행 작업 조정
│   ├── src/
│   │   ├── rxtx_common.cpp      # RF 모드 연동
│   │   └── tx_main.cpp          # 채팅 진입·종료 및 일반 RC 복귀
│   └── test/test_elrs_chat/     # native 테스트
├── tools/                      # 빌드·검사·소스 확인 도구
└── docs/chat-development.md     # 빌드·테스트 안내
```

바로가기: [펌웨어 빌드](#펌웨어-빌드) · [Lua 실행 파일](src/lua/ELRSChat.lua) · [필수 Lua 보조 파일](src/lua/ELRSChat/r16) · [소스 전체 diff](patches/elrs-4.1.0-chat.patch)

## 프로젝트 소개

공식 ExpressLRS 배포본이나 공식 승인 기능이 아닙니다.

ELRS 송신 모듈 두 대 이상이 짧은 공개 메시지를 주고받는 기능을 추가합니다.
EdgeTX 본체를 수정하지 않고, 수정된 ELRS 모듈 코드와 Lua 도구를 사용합니다.
**Lua만 설치하면 동작하는 기능은 아니며, 채팅을 지원하는 모듈 펌웨어가 필요합니다.**

## 현재 공개 범위

- ExpressLRS **4.1.0 원본 이력과 소스**에 채팅 기능을 반영했습니다.
- [우리 패치](patches/elrs-4.1.0-chat.patch)와 [채팅 C++ 코드](src/lib/ElrsChat)를 공개합니다.
- [ELRSChat.lua](src/lua/ELRSChat.lua) 및 [r16 보조 Lua 파일](src/lua/ELRSChat/r16)을 공개합니다.
- 현재는 **소스와 스크립트만 공개**합니다. 미리 빌드한 펌웨어나 별도 배포 ZIP은 제공하지 않습니다.

기반 커밋: [`a9d4a9cb`](https://github.com/ExpressLRS/ExpressLRS/commit/a9d4a9cb5b5687c4c9d7e9e7fbdf44ad93651da6).
ELRSChat 변경 공개일: **2026-09-19**. 출처와 라이선스는 [NOTICE.md](NOTICE.md)에 기록했습니다.

## 동작

**일반 RC 조종 → Lua에서 채팅 진입 → 채팅 종료 시 일반 RC 복귀**

채팅 중에는 해당 모듈의 일반 RC 링크가 중단됩니다. 조종과 채팅을 동시에 사용하는 기능이 아닙니다.

- 공개 브로드캐스트. RF 수신 확인, 자동 재전송, 메시 중계는 없습니다.
- 메시지 최대 32바이트, 실행 중 최근 송수신 기록 50개, 정형 문구 20개.
- 직접 입력과 ENTER 길게 누르기 전송, 대화 기록 스크롤.
- 128×64 흑백부터 컬러 화면까지 해상도와 글자 폭에 맞춰 배치.
- `Tx:메시지`, `Rx(ID):메시지` 표시. 네 자리 ID는 장치 ID의 화면용 축약값입니다.
- 주파수·출력은 Lua 설정으로 요청하고, 모듈에서 검증한 후 채팅 동안만 적용합니다.

## 펌웨어 빌드

코드를 수정하거나 패치를 따로 적용할 필요는 없습니다. 아래 명령을 사용하세요.
기본 `platformio.ini`로 일반 ELRS 빌드만 하면 **채팅 기능이 활성화되지 않습니다**.

### 1. 소스 내려받기

[Python](https://www.python.org/downloads/) 3.10 이상과
[Git](https://git-scm.com/downloads/)을 설치합니다. Windows에서는 Python 설치 시
**Add to PATH**를 선택하세요. Windows는 PowerShell, macOS/Linux는 터미널을 열고 실행합니다.

```sh
git clone https://github.com/kim2160/ELRSChat.git
cd ELRSChat
python -m venv .venv
```

macOS/Linux에서는 마지막 명령의 `python`을 `python3`로 바꿉니다.
이어서 운영체제에 맞는 명령 하나로 빌드 환경을 활성화합니다.

| 운영체제 | 명령 |
|---|---|
| Windows PowerShell | `.\.venv\Scripts\Activate.ps1` |
| Windows 명령 프롬프트 | `.venv\Scripts\activate.bat` |
| macOS/Linux | `source .venv/bin/activate` |

PowerShell에서 활성화가 차단되면 명령 프롬프트를 열어 해당 활성화 명령을 사용하세요.
이후 명령도 활성화한 터미널의 `ELRSChat` 폴더에서 실행합니다.

### 2. 빌드 준비

```sh
python -m pip install platformio==6.1.19
python tools/prepare_chat.py
```

빌드에 필요한 파일을 내려받으므로 인터넷 연결이 필요합니다.

### 3. 내 모듈용 펌웨어 만들기

**자기 모델에 맞는 명령 하나만** 실행합니다. 아래 예제는 **내장 2.4 GHz ELRS**
모듈용이며, Lua 기본값과 같은 **2440 MHz / 25 mW**로 설정합니다.

**Jumper T15:**

```sh
python tools/build_chat.py --mcu ESP32 --radio 2400 --board jumper.tx_2400.t-15 --frequency 2440000000 --power-mw 25 --experimental-rf
```

**HelloRadio V14:**

```sh
python tools/build_chat.py --mcu ESP32 --radio 2400 --board helloradio.tx_2400.v14 --frequency 2440000000 --power-mw 25 --experimental-rf
```

`--experimental-rf`는 실험용 채팅 무선 모드를 켜는 옵션이므로 빼지 마세요.
처음에는 추가 파일을 내려받아 빌드에 몇 분 이상 걸릴 수 있습니다.
여러 모델을 빌드할 때는 하나씩 순서대로 실행합니다.

<details>
<summary>다른 ELRS 송신 모듈을 사용한다면?</summary>

다른 **ESP32 + 2.4 GHz** 모듈은 먼저 보드 목록을 확인합니다.

```sh
python tools/build_chat.py --mcu ESP32 --radio 2400 --list-boards
```

위 빌드 명령의 `--board` 뒤 값을 목록에 나온 자기 모듈의 정확한 이름으로 바꿉니다.
`--board`를 생략하거나 이름이 비슷한 다른 모델을 선택하지 마세요.
내장 모듈과 외장 모듈의 보드는 서로 다를 수 있습니다.

칩 계열이 다르면 `src/hardware/targets.json`에서 해당 모듈의 `firmware` 값을 확인합니다.
예를 들어 `Unified_ESP32S3_2400_TX`는 `--mcu ESP32S3 --radio 2400`에 해당합니다.
보드 목록 조회와 빌드 명령 모두 이 값으로 바꿉니다.
900 MHz/LR1121 설정은 [상세 빌드 안내](docs/chat-development.md)를 참고하세요.
Gemini/듀얼 RF 모듈은 지원하지 않으며, 목록에 있다는 것만으로 해당 모델의
ELRSChat 동작이 검증된 것은 아닙니다.

</details>

### 4. 결과 파일 확인 및 Lua 설치

빌드가 끝나면 터미널에 `Build saved:`와 저장 폴더가 표시됩니다.
위 예제의 결과 파일은 다음과 같습니다. 자기 모델의 파일을 사용하세요.

```text
dist/Unified_ESP32_2400_TX_CHAT/jumper.tx_2400.t-15/firmware.bin
dist/Unified_ESP32_2400_TX_CHAT/helloradio.tx_2400.v14/firmware.bin
```

빌드는 파일만 만들며 조종기에 자동으로 설치하지 않습니다. 생성한 `firmware.bin`은
[ELRS Wi-Fi 업데이트 화면](https://www.expresslrs.org/software/updating/wifi-updating/) 등
해당 기기의 일반적인 **ELRS 모듈 업데이트 방법**으로 설치합니다.
EdgeTX 본체 펌웨어 업데이트가 아닙니다. 이후 아래 안내대로 Lua 파일을 복사합니다.

## Lua 설치

같은 파일을 두 조종기에 사용합니다. 저장소의 다음 파일을 SD 카드에 복사합니다.

| 저장소 경로 | SD 카드 경로 |
|---|---|
| `src/lua/ELRSChat.lua` | `/SCRIPTS/TOOLS/ELRSChat.lua` |
| `src/lua/ELRSChat/r16/*.lua` | `/SCRIPTS/TOOLS/ELRSChat/r16/*.lua` |

보조 파일 네 개도 함께 필요합니다. 이전 `ELRSChat.luac` 캐시가 있다면 삭제하고 실행합니다.
현재 일반 설정용 `elrs.lua`가 정상 동작하면 그대로 사용할 수 있습니다.

기본 Lua 설정은 `2440 MHz / 25 mW`입니다.

```lua
local CHAT_FREQUENCY_KHZ = 2440000
local CHAT_POWER_MW = 25
```

같은 공개 채널을 사용하려는 조종기끼리는 주파수와 호환 RF 프로필을 맞춥니다.
지원하지 않는 대역·출력은 거부되며, 기본 RC 모델 설정을 영구 변경하지 않습니다.

### 채팅 시작과 종료

1. 사용할 모듈 하나를 CRSF로 활성화합니다. T15/V14 내장 모듈은 Internal RF = CRSF,
   External RF = OFF로 설정합니다.
2. Tools에서 `ELRSChat`을 실행하고 ENTER로 채팅에 진입합니다.
3. `PUBLIC CHAT READY`에서 ENTER를 눌러 정형 문구를 선택하거나 직접 입력합니다.
   직접 입력 중에는 ENTER를 길게 눌러 전송하고, 대화 화면의 휠 또는 +/-로 기록을 스크롤합니다.
4. 대화 화면에서 EXIT로 종료하고 모듈의 일반 RC 복귀 응답을 기다립니다.

Lua가 중단되면 모듈은 세션 만료 후 일반 RC 복귀를 시도합니다.
복귀 시 수신기가 다시 연결되는 시간이 필요하므로, 전환 동작은 비행 전에 확인해야 합니다.

## 개발과 검증

[개발·빌드·테스트 안내](docs/chat-development.md)에서 소스 구조와 명령을 확인할 수 있습니다.
패치는 이 저장소에 **이미 적용되어 있습니다**.

Jumper T15와 HelloRadio V14의 내장 2.4 GHz 모듈을 기준으로 개발했습니다.
공개 전 개발 코드의 C++/Lua 회귀 테스트 117개, native 테스트 5개,
두 모델 빌드와 채팅 비활성 일반 빌드를 통과했습니다.
GitHub에서는 소스 검사와 포함된 native 테스트 5개를 실행하며 펌웨어를 업로드하지 않습니다.

시험용 구현입니다. 모든 모델의 실제 RF 송수신·RC 복귀·거리·출력은 검증된 것이 아닙니다.
Gemini/듀얼 RF는 지원하지 않으며, 다른 칩 계열 간 호환성도 별도 검증이 필요합니다.

## 라이선스와 출처

- 기반 프로젝트: [ExpressLRS](https://github.com/ExpressLRS/ExpressLRS), ExpressLRS LLC와 기여자들.
- 원본 [GPLv3 LICENSE](LICENSE)와 파일별 저작권·라이선스 고지를 유지합니다.
- ELRSChat의 신규 코드와 Lua는 [NOTICE.md에 명시한 범위](NOTICE.md)에서 GPL-3.0-or-later로 제공합니다.
- 원본 `elrs.lua`의 GPLv2 등 별도 라이선스는 그대로 유지합니다. [라이선스 목록](LICENSES/README.md).
- [원본 프로젝트 소개](README.upstream.md)를 별도 보존했습니다.

문의와 수정 제안은 [이 저장소의 Issues](https://github.com/kim2160/ELRSChat/issues)로 남겨 주세요.

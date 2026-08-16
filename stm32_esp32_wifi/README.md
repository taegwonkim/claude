# STM32L562CET6 + ESP32-C3-WROOM(ESP-AT) WiFi 연결 라이브러리

STM32L562CET6이 UART로 연결된 ESP32-C3-WROOM(ESP-AT 펌웨어)을 AT 명령으로
제어하여 AP 접속, TCP 서버 접속, DHCP on/off(정적 IP) 설정, 그리고 AP/서버
접속이 끊겼을 때 자동 재접속까지 수행하는 논블로킹 상태 머신입니다.

## 파일 구성

- `Inc/esp32_at.h`, `Src/esp32_at.c`
  UART 기반 저수준 AT 명령 송수신 드라이버. 1바이트 인터럽트 수신을 라인
  단위로 조립해 큐에 저장하고, 명령 전송 후 기대 응답("OK" 등)이 올 때까지
  타임아웃 기반으로 대기합니다.

- `Inc/wifi_manager.h`, `Src/wifi_manager.c`
  실제 연결 절차(AP 접속 → DHCP/정적 IP 설정 → 서버 접속)와 장애 감지·재접속
  로직을 담은 상태 머신. `WiFi_Manager_Process()`를 메인 루프에서 주기적으로
  호출하면 됩니다.

- `Src/main_example.c`
  CubeMX로 생성한 프로젝트에 통합하는 예시.

## 하드웨어 연결

| STM32L562CET6 | ESP32-C3-WROOM |
|---|---|
| USARTx_TX | RX |
| USARTx_RX | TX |
| GND | GND |
| 3.3V (충분한 전류 공급) | 3V3 |

- CubeMX에서 사용할 USART(예: USART1)를 Asynchronous 모드로 생성하고,
  ESP-AT 펌웨어의 기본 보레이트(보통 115200bps)와 맞춥니다.
- 해당 USART의 전역 인터럽트(NVIC)를 활성화해야 합니다(1바이트 인터럽트
  수신 방식이므로 `HAL_UART_Receive_IT`가 계속 재무장됩니다).

## 연결 절차 (상태 머신 흐름)

1. `AT` - 모듈 응답 확인
2. `ATE0` - 에코 끄기
3. `AT+CWMODE=1` - Station 모드
4. `AT+CWDHCP=1,<0|1>` - DHCP on/off
5. (DHCP off인 경우) `AT+CIPSTA="ip","gateway","netmask"` - 정적 IP 설정
6. `AT+CWJAP="ssid","password"` - AP 접속 (`WIFI GOT IP`까지 대기)
7. `AT+CIPMUX=0` - 단일 연결 모드
8. `AT+CIPSTART="TCP","server_ip",port` - TCP 서버 접속
9. 이후 `CONNECTED` 상태에서 5초마다 `AT+CIPSTATUS`로 상태를 점검하고,
   비동기 URC(`WIFI DISCONNECT`, `+CIPCLOSED` 등)도 함께 감시합니다.

## 재접속 로직

- **AP 연결이 끊긴 경우**(`WIFI DISCONNECT` URC 수신, 또는 `AT+CIPSTATUS`가
  `STATUS:5` 반환): `AT+CWJAP`부터 다시 시도합니다(정적 IP는 이미 설정되어
  있으므로 재설정하지 않습니다).
- **서버 연결만 끊긴 경우**(`STATUS:4`, 또는 `+CIPCLOSED` URC): AP는 그대로
  둔 채 `AT+CIPSTART`만 재시도합니다.
- 재시도는 2초부터 시작해 실패할 때마다 2배씩 늘어나 최대 60초까지 커지는
  지수 백오프(exponential backoff)를 사용하며, 성공하면 다시 2초로
  초기화됩니다. 재시도 횟수 제한은 없고, 연결이 복구될 때까지 계속
  시도합니다(무제한 재시도가 부담스러우면 `wifi_manager.c`의
  `RETRY_BACKOFF_MAX_MS` 근처에 실패 횟수 카운터를 추가해 상한을 두면
  됩니다).

## 사용 예시

```c
wifi_ap_config_t ap_cfg = {
    .ssid = "MyHomeAP",
    .password = "MyAPPassword123",
};

wifi_ip_config_t ip_cfg = {
    .dhcp_enable = false,
    .ip = "192.168.0.50",
    .gateway = "192.168.0.1",
    .netmask = "255.255.255.0",
};

wifi_server_config_t server_cfg = {
    .ip = "192.168.0.100",
    .port = 8080,
};

WiFi_Manager_Init(&huart1, &ap_cfg, &ip_cfg, &server_cfg);

while (1) {
    WiFi_Manager_Process();

    if (WiFi_Manager_IsConnected()) {
        WiFi_Manager_Send((uint8_t *)"hello\r\n", 7);
    }

    HAL_Delay(10);
}
```

DHCP를 사용하려면 `ip_cfg.dhcp_enable = true`로 두면 되고, 이때
`ip`/`gateway`/`netmask` 필드는 사용되지 않습니다. 서버 접속이 필요 없다면
`WiFi_Manager_Init()`의 `server_cfg` 인자에 `NULL`을 넘기면 AP 접속까지만
수행합니다.

## 참고 / 제한 사항

- `AT+CWJAP`, `AT+CIPSTART`처럼 응답까지 시간이 걸리는 명령은 해당 상태
  전이 시점에서 최대 타임아웃만큼 블로킹됩니다(`AT+CWJAP`은 최대 20초,
  `AT+CIPSTART`는 최대 10초). 일반적인 폴링 주기(수십 ms 단위 메인 루프)를
  쓰는 임베디드 애플리케이션이라면 문제되지 않지만, 실시간성이 매우
  중요한 다른 태스크와 같은 루프에서 돌린다면 RTOS 태스크로 분리하는 것을
  권장합니다.
- ESP-AT 펌웨어 버전에 따라 명령 문법이 조금씩 다를 수 있습니다(특히
  `AT+CWDHCP`, `AT+CIPSTA`). 사용 중인 ESP-AT 릴리스의 AT 명령 레퍼런스를
  확인하고 필요시 `wifi_manager.c`의 명령 문자열을 맞춰주세요.

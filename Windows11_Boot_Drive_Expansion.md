# Windows 11 부팅 드라이브(C:) 영역 확장 가이드

## 개요

Windows 11에서 부팅 드라이브(C:)의 파티션이 부족할 경우, 디스크 관리 도구 또는 디스크파트(DiskPart) 명령어를 이용해 용량을 확장할 수 있습니다.

---

## 방법 1: 디스크 관리(GUI) 사용

### 사전 조건
- C: 드라이브 **바로 오른쪽**에 **할당되지 않은 공간(Unallocated space)**이 있어야 합니다.
- 할당되지 않은 공간이 없다면 [방법 3](#방법-3-shrink--확장-할당되지-않은-공간이-없는-경우)을 먼저 진행하세요.

### 단계

1. **디스크 관리 열기**
   - `Win + X` → **디스크 관리(Disk Management)** 선택
   - 또는 `Win + R` → `diskmgmt.msc` 입력 후 Enter

2. **C: 드라이브 확인**
   - 하단 디스크 맵에서 C: 파티션 오른쪽에 **할당되지 않은 공간**이 있는지 확인

3. **볼륨 확장**
   - C: 드라이브 파티션을 **우클릭** → **볼륨 확장(Extend Volume)** 클릭

4. **볼륨 확장 마법사**
   - **다음(Next)** 클릭
   - 확장할 공간 크기(MB)를 입력하거나 최대값을 그대로 사용
   - **다음(Next)** → **마침(Finish)** 클릭

5. **완료 확인**
   - C: 드라이브 용량이 늘어났는지 확인

---

## 방법 2: DiskPart 명령어 사용

### 관리자 권한 명령 프롬프트 실행

```
Win + S → "cmd" 검색 → 우클릭 → 관리자로 실행
```

### 명령어 순서

```cmd
diskpart
```

```diskpart
DISKPART> list disk
DISKPART> select disk 0          ← 부팅 디스크 번호 선택 (보통 Disk 0)
DISKPART> list partition
DISKPART> select partition 3     ← C: 드라이브에 해당하는 파티션 번호 선택
DISKPART> extend                 ← 인접한 할당되지 않은 공간 전체를 확장
```

특정 크기(예: 20GB)만 확장하려면:

```diskpart
DISKPART> extend size=20480      ← 크기 단위는 MB (20480MB = 20GB)
```

완료 후 종료:

```diskpart
DISKPART> exit
```

---

## 방법 3: Shrink → 확장 (할당되지 않은 공간이 없는 경우)

C: 드라이브 오른쪽에 바로 다른 파티션(D: 등)이 있는 경우, 해당 파티션을 축소하여 공간을 확보해야 합니다.

### 단계

1. **디스크 관리** 열기 (`Win + R` → `diskmgmt.msc`)

2. **D: 드라이브 축소**
   - D: 드라이브 우클릭 → **볼륨 축소(Shrink Volume)**
   - 축소할 크기 입력 후 **축소(Shrink)** 클릭

3. **C: 드라이브 확장**
   - C: 드라이브 우클릭 → **볼륨 확장(Extend Volume)**
   - 마법사 진행 → **마침(Finish)**

> **주의**: 만약 C: 바로 오른쪽이 **복구 파티션(Recovery Partition)**이면 GUI에서 확장이 안 됩니다. 이 경우 [방법 4](#방법-4-복구-파티션이-방해하는-경우)를 사용하세요.

---

## 방법 4: 복구 파티션이 방해하는 경우

Windows 복구 파티션이 C:와 미할당 공간 사이에 위치하면 GUI 확장이 차단됩니다.

### DiskPart로 복구 파티션 이동 (고급)

> **⚠️ 주의**: 이 작업은 복구 파티션을 삭제하거나 이동합니다. 반드시 **시스템 전체 백업** 후 진행하세요.

```cmd
diskpart
```

```diskpart
DISKPART> list disk
DISKPART> select disk 0
DISKPART> list partition
```

파티션 목록 예시:
```
파티션 ###  종류              크기     오프셋
----------  ----------------  -------  -------
파티션 1    시스템              100 MB  1024 KB
파티션 2    예약됨               16 MB   101 MB
파티션 3    기본               200 GB   117 MB    ← C: 드라이브
파티션 4    복구               1000 MB  200 GB    ← 복구 파티션 (이게 문제)
```

**복구 파티션 삭제 후 C: 확장 → 복구 파티션 재생성** 순서:

```diskpart
DISKPART> select partition 4      ← 복구 파티션 선택
DISKPART> delete partition override
DISKPART> select partition 3      ← C: 드라이브 선택
DISKPART> extend
```

복구 환경 재설정 (PowerShell 관리자 권한):

```powershell
reagentc /disable
reagentc /enable
```

---

## 방법 5: 타사 도구 사용 (권장 - 간편함)

복구 파티션 이동 등 복잡한 작업이 필요할 때는 아래 무료 도구를 사용하면 편리합니다.

| 도구 | 특징 |
|------|------|
| **MiniTool Partition Wizard Free** | 복구 파티션 이동 지원, 직관적 UI |
| **AOMEI Partition Assistant** | 파티션 병합, 이동, 확장 지원 |
| **GParted (Live USB)** | 오픈소스, 부팅 미디어로 실행 |

---

## 자주 발생하는 문제

### "볼륨 확장" 옵션이 비활성화됨
- **원인**: C: 오른쪽에 할당되지 않은 공간이 없거나, 복구 파티션이 사이에 있음
- **해결**: 방법 3 또는 방법 4 참고

### DiskPart `extend` 실패
- **원인**: 파티션이 RAW이거나 파일시스템 오류
- **해결**: `chkdsk C: /f /r` 실행 후 재시도

### 확장 후 부팅 안 됨
- **원인**: BCD(부트 구성 데이터) 손상
- **해결**: Windows 설치 미디어로 부팅 → 복구 → 명령 프롬프트에서 `bootrec /fixmbr` 및 `bootrec /fixboot` 실행

---

## 요약 흐름도

```
C: 확장 필요
     │
     ▼
C: 오른쪽에 할당되지 않은 공간이 있나?
     ├─ YES → 디스크 관리 또는 DiskPart로 extend (방법 1, 2)
     └─ NO
          │
          ▼
     오른쪽 파티션이 D: 등 일반 파티션?
          ├─ YES → D: 축소 후 C: 확장 (방법 3)
          └─ NO (복구 파티션)
                    │
                    ▼
               복구 파티션 삭제 후 확장 (방법 4)
               또는 타사 도구 사용 (방법 5)
```

---

## 참고 사항

- 파티션 작업 전 **중요 데이터 백업** 필수
- SSD의 경우 작업 속도가 빠르지만, HDD는 파티션 이동 시 수 시간 소요 가능
- Windows 11 Home/Pro 모두 위 방법 적용 가능

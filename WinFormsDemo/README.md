# WinFormsDemo (Windows 11 / C# WinForms)

메인 폼에 `TEXT` 라벨과 초기값이 `1234`인 텍스트 상자를 배치한 예제입니다.

```
┌─ Main Form ──────────────────────┐
│      TEXT  [1234              ]  │
└──────────────────────────────────┘
```

## 요구 사항
- Windows 10 / 11
- **Visual Studio 2022** (17.8 이상)
  - Visual Studio Installer → **.NET 데스크톱 개발** 워크로드 설치
  - .NET 8 SDK는 이 워크로드에 포함되어 있음

## 열기 / 실행
1. `WinFormsDemo.sln`을 더블클릭해 Visual Studio에서 엽니다.
2. `F5`를 누르면 실행됩니다.
3. 솔루션 탐색기에서 `Form1.cs`를 더블클릭하면 디자이너가 열립니다.
   컨트롤을 마우스로 끌어 위치와 크기를 조절하거나 속성 창(`F4`)에서 값을 바꿀 수 있습니다.

명령줄에서 실행하려면:
```
dotnet run --project WinFormsDemo
```

## 파일 구성
| 파일 | 설명 |
|---|---|
| `Form1.Designer.cs` | 디자이너가 관리하는 코드 (라벨/텍스트 상자의 Location, Size, Font, Text 등) |
| `Form1.cs` | 사용자 코드 (실행 중 위치·크기를 바꾸는 도우미 메서드, `Value` 속성) |
| `Program.cs` | 진입점 |
| `app.manifest` | Windows 10/11 호환성, 공용 컨트롤 v6 |
| `WinFormsDemo.csproj` | .NET 8 WinForms, PerMonitorV2 고DPI 설정 |

## 참고
- 솔루션 파일은 Visual Studio 2022 형식(`.sln`, Format Version 12.00)입니다.
- 대상 프레임워크는 `net8.0-windows`입니다. VS2022에서는 .NET 8 또는 .NET 9까지 사용할 수 있으며,
  .NET 9로 바꾸려면 `WinFormsDemo.csproj`의 `<TargetFramework>`를 `net9.0-windows`로 수정합니다.

#!/usr/bin/env bash
# CubeMX 가 생성한 STM32CubeIDE 프로젝트에 LAN8770 파일을 복사합니다.
# CubeMX 에서 코드를 다시 생성하면 LWIP/Target/ethernetif.c 가 덮어써지므로
# 재생성 후에 이 스크립트를 다시 실행하세요.
#
#   사용법: tools/install_to_cubeide.sh <CubeIDE 프로젝트 폴더>
set -euo pipefail

SRC="$(cd "$(dirname "$0")/.." && pwd)"
DST="${1:?usage: $0 <CubeIDE project dir>}"

[ -d "$DST/Core" ] && [ -d "$DST/LWIP/Target" ] || {
  echo "ERROR: $DST 에 Core/ 와 LWIP/Target/ 이 없습니다. CubeMX 에서 LWIP 를 켜고 코드를 생성했는지 확인하세요." >&2
  exit 1
}

mkdir -p "$DST/Drivers/BSP/Components/lan8770"
cp -v "$SRC/Drivers/BSP/Components/lan8770/lan8770.c" \
      "$SRC/Drivers/BSP/Components/lan8770/lan8770.h" \
      "$DST/Drivers/BSP/Components/lan8770/"

cp -v "$SRC/LWIP/Target/ethernetif.c" "$SRC/LWIP/Target/ethernetif.h" "$DST/LWIP/Target/"
cp -v "$SRC/Core/Inc/app_net.h" "$DST/Core/Inc/"
cp -v "$SRC/Core/Src/app_net.c" "$DST/Core/Src/"

# 사용자 설정 파일은 이미 있으면 보존
if [ -f "$DST/Core/Inc/lan8770_conf.h" ]; then
  echo "keep: $DST/Core/Inc/lan8770_conf.h (기존 설정 유지)"
else
  cp -v "$SRC/Core/Inc/lan8770_conf.h" "$DST/Core/Inc/"
fi

# 링커 스크립트에 .lwip_sec 추가 (없을 때만)
for LD in "$DST"/*_FLASH.ld; do
  [ -f "$LD" ] || continue
  if grep -q '\.lwip_sec' "$LD"; then
    echo "linker: $(basename "$LD") 에 이미 .lwip_sec 가 있습니다 (건너뜀)"
    continue
  fi
  grep -q 'RAM_D2' "$LD" || echo "WARNING: $(basename "$LD") 의 MEMORY 에 RAM_D2 가 없습니다. 직접 추가하세요."
  cp "$LD" "$LD.bak"
  awk -v snip="$SRC/linker/lwip_sec_snippet.ld" '
    /\/DISCARD\// && !done {
      inbody = 0
      while ((getline line < snip) > 0) {
        if (line ~ /^  \.lwip_sec/) inbody = 1
        if (inbody) print line
      }
      print ""
      done = 1
    }
    { print }
  ' "$LD.bak" > "$LD"
  echo "linker: $(basename "$LD") 에 .lwip_sec 추가 (원본: $(basename "$LD").bak)"
done

cat <<'EOF'

남은 수동 작업 (README 5장 참고):
  1) Project Properties > C/C++ Build > Settings > MCU GCC Compiler > Include paths 에
     ../Drivers/BSP/Components/lan8770 추가
  2) main.c 의 USER CODE 영역에 APP_Net_Init() / MX_LWIP_Process() / APP_Net_Process() 추가
  3) Core/Inc/lan8770_conf.h 에서 PHY 역할(MASTER/SLAVE) 확인
EOF

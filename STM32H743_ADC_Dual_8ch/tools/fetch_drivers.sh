#!/usr/bin/env bash
# ST 공식 GitHub 저장소에서 HAL 드라이버와 CMSIS 헤더를 Drivers/ 아래로 받습니다.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p Drivers/CMSIS/Device/ST

[ -d Drivers/STM32H7xx_HAL_Driver ] || \
  git clone --depth 1 https://github.com/STMicroelectronics/stm32h7xx-hal-driver.git Drivers/STM32H7xx_HAL_Driver

[ -d Drivers/CMSIS/Device/ST/STM32H7xx ] || \
  git clone --depth 1 https://github.com/STMicroelectronics/cmsis-device-h7.git Drivers/CMSIS/Device/ST/STM32H7xx

if [ ! -d Drivers/CMSIS/Core/Include ]; then
  git clone --depth 1 --filter=blob:none --sparse https://github.com/STMicroelectronics/cmsis_core.git Drivers/cmsis_core_tmp
  git -C Drivers/cmsis_core_tmp sparse-checkout set CMSIS/Core/Include
  mkdir -p Drivers/CMSIS/Core
  mv Drivers/cmsis_core_tmp/CMSIS/Core/Include Drivers/CMSIS/Core/Include
  rm -rf Drivers/cmsis_core_tmp
fi
echo "Drivers ready."

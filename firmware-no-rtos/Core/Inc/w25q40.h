/**
 * w25q40.h
 *
 * Winbond W25Q40CLSNIG (4Mbit SPI NOR Flash) 드라이버. SPI2(Master, CS=GPIO 소프트웨어 제어) 사용.
 * CubeMX가 생성하는 hspi2 핸들과 EEP_NSS_GPIO_Port/EEP_NSS_Pin 매크로(main.h, PB12, 핀 라벨을
 * "EEP_NSS"로 지정 시 자동 생성)를 사용한다고 가정한다.
 */
#ifndef W25Q40_H
#define W25Q40_H

#include <stdint.h>
#include <stdbool.h>
#include "spi.h" /* CubeMX 생성: hspi2 선언 */

#ifdef __cplusplus
extern "C" {
#endif

#define W25Q40_PAGE_SIZE     (256U)
#define W25Q40_SECTOR_SIZE   (4096U)
#define W25Q40_JEDEC_ID      (0xEF4013U) /* Winbond(0xEF), Memory Type 0x40, Capacity 0x13(4Mbit) */

/**
 * @brief 플래시 초기화 및 JEDEC ID 검증
 * @return true: 정상 인식, false: ID 불일치/통신 실패
 */
bool W25Q40_Init(void);

/**
 * @brief 임의 주소에서 len바이트 읽기 (페이지/섹터 경계 제약 없음)
 */
bool W25Q40_Read(uint32_t addr, uint8_t *data, uint32_t len);

/**
 * @brief 4KB 섹터 삭제 (addr은 섹터 내 임의 주소 가능, 내부적으로 정렬)
 */
bool W25Q40_EraseSector(uint32_t addr);

/**
 * @brief 페이지 프로그램 (len<=256, addr~addr+len-1이 동일 페이지 내에 있어야 함).
 *        여러 페이지에 걸친 쓰기는 W25Q40_Write()를 사용할 것.
 */
bool W25Q40_ProgramPage(uint32_t addr, const uint8_t *data, uint32_t len);

/**
 * @brief 페이지 경계를 자동으로 나눠 임의 길이를 프로그램 (대상 영역은 미리 Erase 되어 있어야 함).
 */
bool W25Q40_Write(uint32_t addr, const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* W25Q40_H */

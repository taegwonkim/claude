/*
 * C언어 goto 사용법 튜토리얼
 *
 * goto 문은 프로그램의 실행 흐름을 특정 레이블(label)로 무조건 이동시킵니다.
 * 남용하면 코드 가독성이 떨어지지만, 특정 상황에서 유용하게 사용됩니다.
 *
 * 문법:
 *   goto 레이블명;
 *   ...
 *   레이블명:
 *     실행할 코드;
 */

#include <stdio.h>
#include <stdlib.h>

/* ============================================================
 * 예제 1: 기본 goto 사용법
 * ============================================================ */
void example1_basic(void) {
    printf("=== 예제 1: 기본 goto ===\n");

    printf("1단계\n");
    goto skip;         /* skip 레이블로 이동 */
    printf("이 줄은 실행되지 않습니다\n");

skip:
    printf("2단계 (skip 레이블)\n");
    printf("3단계\n\n");
}

/* ============================================================
 * 예제 2: 중첩 루프 탈출 (goto의 대표적 활용)
 *
 * 일반 break는 가장 안쪽 루프만 탈출합니다.
 * goto를 사용하면 중첩된 루프 전체를 한 번에 빠져나올 수 있습니다.
 * ============================================================ */
void example2_nested_loop_exit(void) {
    printf("=== 예제 2: 중첩 루프 탈출 ===\n");

    int i, j;
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
            if (i == 2 && j == 3) {
                printf("i=%d, j=%d 에서 전체 루프 탈출\n", i, j);
                goto loop_exit;  /* 중첩 루프 전체 탈출 */
            }
            printf("i=%d, j=%d\n", i, j);
        }
    }

loop_exit:
    printf("루프 종료 후 계속 실행\n\n");
}

/* ============================================================
 * 예제 3: 에러 처리 패턴 (실무에서 가장 권장되는 goto 활용)
 *
 * 여러 자원(메모리, 파일 등)을 할당한 후 에러 발생 시
 * 깔끔하게 정리(cleanup)하는 패턴입니다.
 * Linux 커널 코드에서도 이 패턴을 사용합니다.
 * ============================================================ */
void example3_error_handling(void) {
    printf("=== 예제 3: 에러 처리 패턴 ===\n");

    char *buf1 = NULL;
    char *buf2 = NULL;
    int error_code = 0;

    buf1 = (char *)malloc(100);
    if (buf1 == NULL) {
        error_code = -1;
        goto cleanup;   /* 메모리 할당 실패 시 정리 단계로 이동 */
    }
    printf("buf1 할당 성공\n");

    buf2 = (char *)malloc(200);
    if (buf2 == NULL) {
        error_code = -2;
        goto cleanup;   /* buf2 할당 실패 시, buf1은 정리 필요 */
    }
    printf("buf2 할당 성공\n");

    /* 정상 처리 */
    printf("정상 처리 완료\n");

cleanup:
    /* 역순으로 자원 해제 */
    if (buf2) {
        free(buf2);
        printf("buf2 해제\n");
    }
    if (buf1) {
        free(buf1);
        printf("buf1 해제\n");
    }

    if (error_code != 0) {
        printf("에러 발생: 코드 %d\n", error_code);
    }
    printf("\n");
}

/* ============================================================
 * 예제 4: goto로 간단한 루프 구현
 *
 * goto로 while/for 루프와 동일한 동작을 구현할 수 있습니다.
 * (이해를 돕기 위한 예시 - 실제로는 루프 사용 권장)
 * ============================================================ */
void example4_loop_with_goto(void) {
    printf("=== 예제 4: goto로 루프 구현 ===\n");

    int count = 0;

loop_start:
    if (count >= 5) goto loop_end;  /* 조건 검사 */

    printf("count = %d\n", count);
    count++;
    goto loop_start;  /* 루프 반복 */

loop_end:
    printf("루프 종료\n\n");
}

/* ============================================================
 * 예제 5: goto 사용 시 주의사항 - 변수 초기화 건너뜀
 *
 * goto로 변수 선언 및 초기화 코드를 건너뛰면
 * 컴파일 에러 또는 미정의 동작이 발생할 수 있습니다.
 * ============================================================ */
void example5_caution(void) {
    printf("=== 예제 5: goto 주의사항 ===\n");

    int x = 10;
    goto after_decl;  /* 아래 y 선언을 건너뜀 */

    /* goto가 건너뛴 영역에 초기화가 있는 변수 선언은 위험합니다.
     * int y = 20;  <-- 이런 코드는 건너뛰면 컴파일 경고/에러 발생
     */

after_decl:
    printf("x = %d\n", x);
    /* y는 사용 불가 - 초기화되지 않았을 수 있음 */
    printf("주의: goto로 초기화 코드를 건너뛰지 마세요!\n\n");
}

/* ============================================================
 * goto 사용 권장/비권장 정리
 * ============================================================
 *
 * 권장 사용 상황:
 *   1. 중첩 루프 전체 탈출
 *   2. 에러 발생 시 자원 정리 (cleanup 패턴)
 *   3. 상태 머신(state machine) 구현
 *
 * 비권장 / 피해야 할 상황:
 *   1. 단순 조건 분기 (if-else 사용)
 *   2. 반복문 대체 (for/while 사용)
 *   3. 위쪽 레이블로 무한 이동 (무한루프 위험)
 *   4. 함수 경계를 넘나드는 이동 (불가능하며 컴파일 에러)
 *
 * ============================================================ */

int main(void) {
    example1_basic();
    example2_nested_loop_exit();
    example3_error_handling();
    example4_loop_with_goto();
    example5_caution();

    printf("=== goto 사용법 요약 ===\n");
    printf("문법: goto 레이블명;\n");
    printf("레이블 선언: 레이블명:\n");
    printf("규칙:\n");
    printf("  - 레이블은 같은 함수 내에만 사용 가능\n");
    printf("  - 레이블명은 변수명 규칙과 동일\n");
    printf("  - 전방(forward) 및 후방(backward) 이동 모두 가능\n");

    return 0;
}

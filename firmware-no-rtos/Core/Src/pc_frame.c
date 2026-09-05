#include "pc_frame.h"
#include <string.h>

int PcFrame_Parse(char *raw_line, char *out_fields[], size_t max_fields)
{
    if (raw_line[0] != PC_FRAME_STX) {
        return -1;
    }

    char *cursor = raw_line + 1; /* STX 건너뜀 */
    char *saveptr = NULL;
    size_t count = 0;
    char *tok = strtok_r(cursor, ",", &saveptr);

    while (tok != NULL && count < max_fields) {
        out_fields[count++] = tok;
        tok = strtok_r(NULL, ",", &saveptr);
    }

    return (int)count;
}

int PcFrame_Wrap(char *out, size_t out_size, const char *csv_payload)
{
    /* 최소한 STX + CR + LF + NUL 이 들어갈 공간은 있어야 한다 */
    if (out_size < 4U) {
        return 0;
    }

    size_t n = 0;
    out[n++] = PC_FRAME_STX;

    size_t payload_len = strlen(csv_payload);
    size_t max_payload = out_size - n - 3U; /* CR, LF, NUL 여유분 제외 */
    if (payload_len > max_payload) {
        payload_len = max_payload;
    }
    memcpy(&out[n], csv_payload, payload_len);
    n += payload_len;

    out[n++] = '\r';
    out[n++] = '\n';
    out[n] = '\0';

    return (int)n;
}

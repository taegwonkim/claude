/**
 * protocol.c — PC<->MCU 설정 프레임 CRC16/인코딩/디코딩
 */
#include "protocol.h"
#include <string.h>

uint16_t Protocol_CRC16(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFu;

    for (uint32_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++)
        {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
        }
    }

    return crc;
}

bool Protocol_DecodePcFrame(const uint8_t *raw, uint32_t rawLen, PcFrame_t *out, PcErrCode_t *errCode)
{
    /* 최소 길이: STX+LEN+CMD+CRC(2)+ETX = 6 */
    if (rawLen < 6u || raw[0] != PC_FRAME_STX || raw[rawLen - 1] != PC_FRAME_ETX)
    {
        if (errCode) *errCode = ERR_BAD_LEN;
        return false;
    }

    uint8_t len = raw[1]; /* CMD + PAYLOAD 길이 */
    if ((uint32_t)(len + 4u) != rawLen || len == 0u || len > (PC_FRAME_MAX_PAYLOAD + 1u))
    {
        if (errCode) *errCode = ERR_BAD_LEN;
        return false;
    }

    const uint8_t *cmdPayload = &raw[2];
    uint16_t crcRx = ((uint16_t)raw[2 + len] << 8) | raw[2 + len + 1];
    uint16_t crcCalc = Protocol_CRC16(cmdPayload, len);

    if (crcRx != crcCalc)
    {
        if (errCode) *errCode = ERR_BAD_CRC;
        return false;
    }

    out->cmd = cmdPayload[0];
    out->len = (uint8_t)(len - 1u);
    memcpy(out->payload, &cmdPayload[1], out->len);

    if (errCode) *errCode = ERR_NONE;
    return true;
}

uint32_t Protocol_EncodePcFrame(const PcFrame_t *frame, uint8_t *out, uint32_t outCap)
{
    uint32_t total = 1u + 1u + 1u + frame->len + 2u + 1u;

    if (outCap < total || frame->len > PC_FRAME_MAX_PAYLOAD)
    {
        return 0u;
    }

    uint32_t idx = 0;
    out[idx++] = PC_FRAME_STX;
    out[idx++] = (uint8_t)(frame->len + 1u); /* CMD + PAYLOAD */
    out[idx++] = frame->cmd;
    memcpy(&out[idx], frame->payload, frame->len);
    idx += frame->len;

    uint16_t crc = Protocol_CRC16(&out[2], frame->len + 1u);
    out[idx++] = (uint8_t)(crc >> 8);
    out[idx++] = (uint8_t)(crc & 0xFFu);
    out[idx++] = PC_FRAME_ETX;

    return idx;
}

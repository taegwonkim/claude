#include "measurement_msg.h"
#include <stdio.h>

int MeasurementMsg_BuildDataLine(char *out, size_t out_size, const MeasurementMsg_t *msg)
{
    int n = snprintf(out, out_size, "DATA %lu %lu",
                      (unsigned long)msg->seq, (unsigned long)msg->timestamp_ms);

    if (n < 0) {
        return n;
    }

    for (uint32_t i = 0; i < FPGA_ADC_SAMPLE_COUNT && (size_t)n < out_size; i++) {
        int m = snprintf(out + n, out_size - (size_t)n, " %u", (unsigned)msg->samples[i]);
        if (m < 0) {
            break;
        }
        n += m;
    }
    return n;
}

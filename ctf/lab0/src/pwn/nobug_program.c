#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

struct hbpkt
{
    uint32_t size;
    uint32_t timestamp;
    uint32_t index;
    uint32_t cred;
    char data[];
};

/* Minimum valid packet: header(16) + at least 1 byte of data */
#define HBPKT_HEADER_SIZE  sizeof(struct hbpkt)
#define BUFFER_SIZE        0x1000

struct hbpkt *get_heart_beat()
{
    uint8_t buffer[BUFFER_SIZE] = {0};

    /* 1) Read the fixed header first */
    size_t n = fread(buffer, 1, HBPKT_HEADER_SIZE, stdin);
    if (n != HBPKT_HEADER_SIZE)
        return NULL;

    struct hbpkt *tmp = (struct hbpkt *)buffer;

    /* 2) Validate size: must be >= header and <= buffer */
    if (tmp->size < HBPKT_HEADER_SIZE || tmp->size > BUFFER_SIZE)
        return NULL;

    /* 3) Calculate how much data to read */
    uint32_t data_len = tmp->size - HBPKT_HEADER_SIZE;

    /* 4) Read data payload */
    if (data_len > 0)
    {
        n = fread(tmp->data, 1, data_len, stdin);
        if (n != data_len)
            return NULL;
    }

    /* 5) Use tmp->size as the authoritative total size (NOT strlen!) */
    uint32_t real_size = tmp->size;

    struct hbpkt *res = malloc(real_size);
    if (!res)
        return NULL;

    memcpy(res, buffer, real_size);

    res->index += 1;

    return res;
}

int reply_heart_beat(struct hbpkt *pkt)
{
    int err = 0;        /* FIX: initialize err */
    int written = 0;    /* FIX: initialize written */

    if (pkt->size > 0)
    {
        written = fwrite(pkt, 1, pkt->size, stdout);
        fflush(stdout);

        if (written == 0 || (uint32_t)written != pkt->size)
        {
            err = -1;
        }
    }

    return err;
}

int main()
{
    int err;
    while (true)
    {
        struct hbpkt *p = get_heart_beat();
        if (!p)
            continue;

        err = reply_heart_beat(p);

        if (err)
        {
            free(p);
            continue;
        }

        /* FIX: also free p on success path (was missing before) */
        free(p);
    }
}

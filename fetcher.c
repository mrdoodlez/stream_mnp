/*!
 * \brief    Binary data fetcher module
 * \details  Fetcher takes data from 2 binary on-disk files
 *           and writes them continously to raw binary streams
 *           that are then handled by Matcher
 * \todo     In order to get closer to real world we can make Fetcher
 *           in parallel treads (and provide Matcher with two separate
 *           IRQs (cond. vars))
 * \author   Stan Raskov a.k.a mrdoodlez
 */

#include "globals.h"
#include "rawstream.h"
#include "fetcher.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

/*!
 * \brief     Fetcher thread worker function
 * @param arg Thread argument
 * @return
 */

void* fetcher_work(void * arg) {
    if (arg == 0) {
        //TODO: handle error
    }

    fetcher_context_t *ctx = (fetcher_context_t*)arg;

    struct {
        uint32_t *buff;
        uint32_t *top;
        size_t len;
    } fetcher_buffs[NSTREAMS];

    uint32_t fetcher_state;

    for (uint32_t stream_num = 0; stream_num < NSTREAMS; stream_num++) {
        FILE *fi;
        char fname[16];
        sprintf(fname, "s%d.dat", stream_num);
        if ((fi = fopen(fname, "rb")) != NULL) {
            fseek(fi, 0L, SEEK_END);
            size_t sz = ftell(fi);
            rewind(fi);

            uint8_t *buff;
            buff = (uint8_t*)malloc(sz);
            fread(buff, 1, sz, fi);
            fclose(fi);

            fetcher_buffs[stream_num].buff = (uint32_t*)buff;
            fetcher_buffs[stream_num].top  = (uint32_t*)buff;
            fetcher_buffs[stream_num].len = sz / sizeof(uint32_t);

            fetcher_state |= (fetcher_buffs[stream_num].len > 0) ? 1 << stream_num : 0;
        }
    }

    while (fetcher_state) {
        //Write data chunk to random input port
        uint32_t stream_num = rand() % NSTREAMS;
        if ((fetcher_state & (1 << stream_num)) != 0) {
            //Data is written in 32-bit words
            uint32_t word = *(fetcher_buffs[stream_num].top);
            size_t sz = sizeof(word);
            if (rawstream_write(ctx->rd[stream_num], (uint8_t*)&word, sz) != sz) {
                //TODO: handle error;
            }

            //Done to simulate some delays between data chunks (as it happens in real systems)
            usleep(FETCHER_SLEEP_USEC);

            if (--fetcher_buffs[stream_num].len == 0) {
                fetcher_state &= ~(1 << stream_num);
            } else {
                fetcher_buffs[stream_num].top++;
            }
        }
    }

    return NULL;
}
#ifndef _FETCHER_H_
#define _FETCHER_H_

#include "globals.h"
#include "rawstream.h"

typedef struct fetcher_context {
    rawstream_d rd[NSTREAMS];
} fetcher_context_t;

void* fetcher_work(void *);

#endif //_FETCHER_H_
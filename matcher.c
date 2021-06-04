/*!
 * \brief   Matcher app main module
 * \details Matcher asynchronusly reads data from two
 *          input binary streams and feeds it Proto module
 * \author   Stan Raskov a.k.a mrdoodlez
 */

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "globals.h"
#include "fetcher.h"
#include "rawstream.h"
#include "proto.h"

static struct {
    fetcher_context_t fc;
    rawstream_d rd[NSTREAMS];
    pthread_cond_t new_data_evt;
} _app_data;

static void _on_new_data(rawstream_dev_id dev_id);
static void* _matcher_work(void *);
static int _process_new_data();

/*!
 * \brief Init application and run worker threads
 * @return
 */

int main() {
    printf("matcher demo start\n");

    memset(&_app_data, 0, sizeof(_app_data));

    for (uint32_t stream_id = 0; stream_id < NSTREAMS; stream_id++) {
        rawstream_d s = rawstream_open((rawstream_dev_id)stream_id, 1024, _on_new_data);
        if (s != NULL) {
            _app_data.rd   [stream_id] = s;
            _app_data.fc.rd[stream_id] = s;
        } else {
            //TODO: handle error
        }
    }

    _app_data.new_data_evt = PTHREAD_COND_INITIALIZER;

    pthread_t matcher;
    if (pthread_create(&matcher, NULL, _matcher_work, NULL) != 0) {
        perror("failed to create matcher thread. terminating");
        return -1;
    }

    pthread_t fetcher;
    if (pthread_create(&fetcher, NULL, fetcher_work, (void*)&_app_data.fc) != 0) {
        perror("failed to create fecther thread. terminating");
        return -1;
    }

    pthread_join(fetcher, NULL);
    pthread_join(matcher, NULL);

    return 0;
}

/********************************************************************/

/*!
 * \brief Matcher worker thread.
 *        Blocked on cond. variable awaiting for new data
 * @return
 */

static void* _matcher_work(void *) {
    pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    uint32_t stop = 0;
    do {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += SHUTDOWN_TMO_SEC;
        pthread_mutex_lock(&lock);
        int err = pthread_cond_timedwait(&_app_data.new_data_evt, &lock, &ts);
        if (err == 0) {
            _process_new_data();
        } else if (err == ETIMEDOUT) {
            stop = 1;
        } else {
            //TODO: handle other errors
            stop = 1;
        }
        pthread_mutex_unlock(&lock);
    } while (!stop);

    return NULL;
}

/*!
 * \brief New data ISR
 * @return
 */

static int _process_new_data() {
    for (uint32_t stream_id = 0; stream_id < NSTREAMS; stream_id++) {
        uint8_t data;
        while(rawstream_read(_app_data.rd[stream_id], &data, sizeof(data)) != 0) {
            proto_work(stream_id, data);
        }
    }
    return 0;
}

/********************************************************************/

/*!
 * \brief Doorbell function called from Fetcher thread
 * \attention No data processing must be done here!
 *            Just unlock cond. variable and exit
 * @param dev_id
 */

static void _on_new_data(rawstream_dev_id dev_id) {
    //printf("_on_new_data\n");
    dev_id = dev_id; //TODO: remove ?
    pthread_cond_signal(&_app_data.new_data_evt);
}
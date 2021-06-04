/*!
 * \brief   PV + T streams align and output
 * \details Some simple RT & PV messages are implemented
 *          Proto waits both streams to provide their timestamps via RT messages
 *          After both RTs are provided Proto starts to output aligned
 *          ( = one of them delayed) streams to stdout
 * \todo    Critical assumption here is that data rates of input streams are _equal_
 *          That may be untrue thus leading to data time-domain misalignment
 *          and must be handled
 * \author   Stan Raskov a.k.a mrdoodlez
 */

#include "proto.h"
#include "globals.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NSAMPLES        1024
#define CMD_MAX_LEN     32

typedef struct proto_pv {
    //position
    struct {
        int32_t u;
        int32_t w;
        int32_t v;
    } p;
    //velocity
    struct {
        int32_t u;
        int32_t w;
        int32_t v;
    } v;
} proto_pv_t;

//timestamp type (TODO: 32-bit is obviously not enough)
typedef uint32_t timestamp_t;

//TODO: provide common header & crc

typedef struct proto_cmd_rt {
    timestamp_t ts;
} proto_cmd_rt_t;

typedef struct proto_cmd_pv {
    proto_pv_t cmd_data;
} proto_cmd_pv_t;

/**********************************************************/

typedef enum {
    PROTO_STATE_OUT_OF_SYNC,
    PROTO_STATE_CMD_RT,
    PROTO_STATE_CMD_PV
} proto_state_t;

typedef enum {
    CMD_ID_RT = 0,
    CMD_ID_PV = 1
} proto_cmd_id_t;

/**********************************************************/

static struct {
    proto_pv_t pv_samples[NSAMPLES];
    size_t top;
    size_t count;
    int clk_offset;
} _pvt_streams[NSTREAMS];

static struct {
    proto_state_t state;
    struct {
        uint8_t cmd_buff[CMD_MAX_LEN];
        size_t bytes;
    } cmd_context;
} _proto_state[NSTREAMS];

/**********************************************************/

static void _format_plot_data(timestamp_t ts, uint32_t stream_id_0, proto_pv_t *pv0,
                              uint32_t stream_id_1, proto_pv_t *pv1);

static int _route_command(uint32_t stream_id, uint8_t cmd);
static int _work_rt(uint32_t stream_id);
static int _work_pv(uint32_t stream_id);

static int _set_rt(uint32_t stream_id);
static int _append_pv(uint32_t stream_id);
static int _evict_aligned_data();

static timestamp_t _get_local_timestamp();
static size_t _skew_to_delay(uint32_t skew);

/**********************************************************/

static void _format_plot_data(timestamp_t ts, uint32_t stream_id_0, proto_pv_t *pv0,
                              uint32_t stream_id_1, proto_pv_t *pv1) {
    // "####" is to separate plottable data from console messages
    printf("##### %d : {%d : {%d, %d, %d, %d, %d, %d} %d : {%d, %d, %d, %d, %d, %d}}\n",
                ts,
                stream_id_0, pv0->p.u, pv0->p.v, pv0->p.w, pv0->v.u, pv0->v.v, pv0->v.w,
                stream_id_1, pv1->p.u, pv1->p.v, pv1->p.w, pv1->v.u, pv1->v.v, pv1->v.w);
}

/**********************************************************/

/*!
 * \brief Work Proto cmd & data
 * @param stream_id Stream ID
 * @param data New data byte
 * @return
 */

int proto_work(uint32_t stream_id, uint8_t data) {
    //printf("proto_work: %d\n", data);
    int result = -1;
    switch(_proto_state[stream_id].state) {
        case PROTO_STATE_CMD_RT:
            _proto_state[stream_id].cmd_context.cmd_buff[_proto_state[stream_id].cmd_context.bytes++] = data;
            result = _work_rt(stream_id);
            break;
        case PROTO_STATE_CMD_PV:
            _proto_state[stream_id].cmd_context.cmd_buff[_proto_state[stream_id].cmd_context.bytes++] = data;
            result = _work_pv(stream_id);
            break;
        default:
        case PROTO_STATE_OUT_OF_SYNC:
            result = _route_command(stream_id, data);
            break;
    }
    return result;
}

static int _route_command(uint32_t stream_id, uint8_t cmd) {
    //printf("stream: %d, route_command: %d\n", stream_id, cmd);
    int result = -1;
    memset(&(_proto_state[stream_id].cmd_context), 0, sizeof(_proto_state[stream_id].cmd_context));
    switch(cmd) {
        case CMD_ID_RT:
            _proto_state[stream_id].state = PROTO_STATE_CMD_RT;
            result = 0;
            break;
        case CMD_ID_PV:
            _proto_state[stream_id].state = PROTO_STATE_CMD_PV;
            result = 0;
            break;
        default:
            break;
    }
    return result;
}

static int _work_rt(uint32_t stream_id) {
    int result = 0;
    if(_proto_state[stream_id].cmd_context.bytes == sizeof(proto_cmd_rt_t)) {
        //printf("stream %d, work_rt\n", stream_id);
        result = _set_rt(stream_id);
        _proto_state[stream_id].state = PROTO_STATE_OUT_OF_SYNC;
    }
    return result;
}

static int _work_pv(uint32_t stream_id) {
    int result = 0;
    if(_proto_state[stream_id].cmd_context.bytes == sizeof(proto_cmd_pv_t)) {
        //printf("stream %d, work_pv\n", stream_id);
        result = _append_pv(stream_id);
        _proto_state[stream_id].state = PROTO_STATE_OUT_OF_SYNC;
    }
    return result;
}

/*!
 * \brief Work RT message and estimate clock offset between input streams
 * @param stream_id Stream ID
 * @return
 */

static int _set_rt(uint32_t stream_id) {
    timestamp_t ts = 0;
    memcpy(&ts, _proto_state[stream_id].cmd_context.cmd_buff, sizeof(ts));

    //Local clock is a master
    timestamp_t lts = _get_local_timestamp();

    if (ts > lts) {
        _pvt_streams[stream_id].clk_offset = ts - lts;
    } else {
        _pvt_streams[stream_id].clk_offset = lts - ts;
        _pvt_streams[stream_id].clk_offset = -_pvt_streams[stream_id].clk_offset;
    }

    printf("set RT fot stream %d: %d\n", stream_id, _pvt_streams[stream_id].clk_offset);

    return 0;
}

/*!
 * \brief Work PV message and evict aligned data
 * @param stream_id
 * @return
 */

static int _append_pv(uint32_t stream_id) {
    if(_pvt_streams[stream_id].count == NSAMPLES) {
        return -1;
    }

    proto_pv_t pv;
    memcpy(&pv, _proto_state[stream_id].cmd_context.cmd_buff, sizeof(pv));

    _pvt_streams[stream_id].pv_samples[_pvt_streams[stream_id].top] = pv;

    _pvt_streams[stream_id].top++;
    _pvt_streams[stream_id].top %= NSAMPLES;
    _pvt_streams[stream_id].count++;

    _evict_aligned_data();

    return 0;
}

/*!
 * \brief Alignes input stream based on their clock skew
 * @return
 */

static int _evict_aligned_data() {
    //TODO: rework me. need to support NSTREAMS != 2

    //Data only gets evicted if both timestamps present (!= 0) TODO: provide some "unset" value
    if ((_pvt_streams[0].clk_offset != 0) && (_pvt_streams[1].clk_offset != 0)) {
        //Which of streams is in "future" and need to be delayed?
        uint32_t stream_id_delayed = (_pvt_streams[0].clk_offset > _pvt_streams[1].clk_offset) ? 0 : 1;
        uint32_t stream_id_in_time = 1 - stream_id_delayed;
        uint32_t skew  = _pvt_streams[stream_id_delayed].clk_offset - _pvt_streams[stream_id_in_time].clk_offset;

        //Clock skew is scaled to delay in samples here
        size_t stream_delay = _skew_to_delay(skew);

        //printf("delayed: %d skew: %d delay: %d\n", stream_id_delayed, skew, (int)stream_delay);

        //If both samples are available output them
        if ((_pvt_streams[stream_id_in_time].count > 0) && (_pvt_streams[stream_id_delayed].count > stream_delay)) {
            size_t rd_index = (_pvt_streams[stream_id_in_time].top + NSAMPLES - 1) % NSAMPLES;
            proto_pv_t pv0    = _pvt_streams[stream_id_in_time].pv_samples[rd_index];

            //printf("idx0: %d\n", (int)rd_index);

            rd_index = (_pvt_streams[stream_id_delayed].top + NSAMPLES - 1 - stream_delay) % NSAMPLES;
            proto_pv_t pv1    = _pvt_streams[stream_id_delayed].pv_samples[rd_index];

            //printf("idx1: %d\n", (int)rd_index);

            _pvt_streams[stream_id_in_time].count = 0;
            _pvt_streams[stream_id_delayed].count = stream_delay;

            _format_plot_data(_get_local_timestamp(), stream_id_in_time, &pv0, stream_id_delayed, &pv1);
        }
    }
    return 0;
}

/********************************************************************/

//TODO: some brute mocking of time service here

#define MAX_MOCK_TIMESTAMP_DELTA    1024

static timestamp_t _get_local_timestamp() {
    static timestamp_t local_clk = 0;
    local_clk += rand() % MAX_MOCK_TIMESTAMP_DELTA;
    return local_clk;
}

//TODO: and here
static size_t _skew_to_delay(uint32_t skew) {
    return 4; //skew / 4;
}
/**
 * @file    cascade_fsm.c
 * @brief   转发状态机的转移表与查表函数（纯逻辑）
 *
 * 改状态流转只改这张表；cascade.c 只负责"每个状态做哪个 485 事务"和
 * "进入状态时复位哪些会话变量"。
 */

#include "cascade_fsm.h"

/* 状态转移表：from + event -> to
 *
 *   ENTER   --0x14 ACK--> PREPARE
 *   ENTER   --0x14 超时--> FAILED
 *   PREPARE --等待中----> PREPARE（原地）
 *   PREPARE --0x11 OK---> BLOCKS
 *   PREPARE --0x11 失败-> FAILED
 *   BLOCKS  --单块 OK---> BLOCKS（块号在 run 里 +1）
 *   BLOCKS  --单块失败--> BLOCKS（重试次数在 run 里 +1）
 *   BLOCKS  --末块 OK---> VERIFY
 *   BLOCKS  --重试耗尽--> FAILED
 *   VERIFY  --CRC 一致--> IDLE
 *   VERIFY  --CRC 不一致/无应答--> FAILED
 */
static const sync_transition_t s_sync_table[] = {
    {SYNC_ENTER, SYNC_EV_ACK, SYNC_PREPARE},
    {SYNC_ENTER, SYNC_EV_FAIL, SYNC_FAILED},

    {SYNC_PREPARE, SYNC_EV_WAIT, SYNC_PREPARE},
    {SYNC_PREPARE, SYNC_EV_ACK, SYNC_BLOCKS},
    {SYNC_PREPARE, SYNC_EV_FAIL, SYNC_FAILED},

    {SYNC_BLOCKS, SYNC_EV_ACK, SYNC_BLOCKS},
    {SYNC_BLOCKS, SYNC_EV_RETRY, SYNC_BLOCKS},
    {SYNC_BLOCKS, SYNC_EV_DONE, SYNC_VERIFY},
    {SYNC_BLOCKS, SYNC_EV_FAIL, SYNC_FAILED},

    {SYNC_VERIFY, SYNC_EV_DONE, SYNC_IDLE},
    {SYNC_VERIFY, SYNC_EV_FAIL, SYNC_FAILED},
};

#define SYNC_TABLE_LEN (sizeof(s_sync_table) / sizeof(s_sync_table[0]))

sync_state_t sync_fsm_next(sync_state_t from, sync_event_t event)
{
    for (uint32_t i = 0; i < SYNC_TABLE_LEN; i++) {
        if (s_sync_table[i].from == from && s_sync_table[i].event == event)
            return s_sync_table[i].to;
    }
    return from; /* 没有这条转移：原地不动，调用方不执行进入动作 */
}

const char *sync_state_name(sync_state_t state)
{
    static const char *const names[SYNC_STATE_COUNT] = {
        [SYNC_IDLE]    = "idle",
        [SYNC_ENTER]   = "enter",
        [SYNC_PREPARE] = "prepare",
        [SYNC_BLOCKS]  = "blocks",
        [SYNC_VERIFY]  = "verify",
        [SYNC_FAILED]  = "failed",
    };
    return (state < SYNC_STATE_COUNT) ? names[state] : "?";
}

const char *sync_event_name(sync_event_t event)
{
    static const char *const names[SYNC_EV_COUNT] = {
        [SYNC_EV_NONE]  = "none",
        [SYNC_EV_WAIT]  = "wait",
        [SYNC_EV_ACK]   = "ack",
        [SYNC_EV_DONE]  = "done",
        [SYNC_EV_RETRY] = "retry",
        [SYNC_EV_FAIL]  = "fail",
    };
    return (event < SYNC_EV_COUNT) ? names[event] : "?";
}

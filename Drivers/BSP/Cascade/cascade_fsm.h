#ifndef DRIVERS_BSP_CASCADE_CASCADE_FSM_H
#define DRIVERS_BSP_CASCADE_CASCADE_FSM_H

/**
 * @file    cascade_fsm.h
 * @brief   主卡→从卡转发状态机的状态/事件与转移表（纯逻辑，无 HAL 依赖）
 *
 * 表驱动：状态动作在 cascade.c，状态怎么走由本文件的 s_sync_table[] 决定。
 * 单独拆出来是为了让转移表可以被 host 单测直接覆盖，不必搬 HAL/lwIP。
 *
 * 事件由当前状态的 run 函数返回：
 *   WAIT  ：还没到时间（prepare 的 1.5s 等待），原地不动
 *   ACK   ：事务成功且还有后续步骤
 *   DONE  ：本次阶段全部完成（最后一个块写完 / 0x13 校验通过）
 *   RETRY ：事务失败但还有重试次数
 *   FAIL  ：事务失败且不再重试 / 0x11 被从卡拒绝
 */

#include <stdint.h>
#include <stdbool.h>

/** @brief 转发状态 */
typedef enum {
    SYNC_IDLE = 0, /**< 未在转发；主卡空闲查询从卡状态 */
    SYNC_ENTER,    /**< 发内部 0x14，确保从卡在 Recovery */
    SYNC_PREPARE,  /**< 等 1.5s 后发内部 0x11，让从卡擦除 */
    SYNC_BLOCKS,   /**< 逐块发内部 0x12 */
    SYNC_VERIFY,   /**< 发内部 0x13，校验从卡整包 CRC */
    SYNC_FAILED,   /**< 转发失败；锁住 slave_update=2，等主机重新 0x04/0x06 */
    SYNC_STATE_COUNT,
} sync_state_t;

/** @brief 状态机事件（由当前状态的 run 函数产生） */
typedef enum {
    SYNC_EV_NONE = 0, /**< 无事件（idle/failed 状态不产生） */
    SYNC_EV_WAIT,     /**< 条件未到，原地等待 */
    SYNC_EV_ACK,      /**< 事务成功，继续 */
    SYNC_EV_DONE,     /**< 当前阶段完成 */
    SYNC_EV_RETRY,    /**< 事务失败，还有重试机会 */
    SYNC_EV_FAIL,     /**< 事务失败，终止转发 */
    SYNC_EV_COUNT,
} sync_event_t;

/** @brief 一条状态转移 */
typedef struct {
    sync_state_t from;  /**< 源状态 */
    sync_event_t event; /**< 事件 */
    sync_state_t to;    /**< 目标状态 */
} sync_transition_t;

/** @brief 查表：没有匹配项时原地返回 from（调用方可据此判断"没动"） */
sync_state_t sync_fsm_next(sync_state_t from, sync_event_t event);

/** @brief 状态/事件名，日志与单测失败信息用 */
const char *sync_state_name(sync_state_t state);
const char *sync_event_name(sync_event_t event);

/** @brief 状态是否处于"正在转发"（idle/failed 都不算） */
static inline bool sync_state_active(sync_state_t state)
{
    return state != SYNC_IDLE && state != SYNC_FAILED && state < SYNC_STATE_COUNT;
}

#endif // !DRIVERS_BSP_CASCADE_CASCADE_FSM_H

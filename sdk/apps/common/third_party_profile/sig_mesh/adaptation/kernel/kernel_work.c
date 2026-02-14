#include "adaptation.h"
#include "system/timer.h"
#include "app_config.h"

#define LOG_TAG             "[MESH-kwork]"
/* #define LOG_INFO_ENABLE */
#define LOG_DEBUG_ENABLE
#define LOG_WARN_ENABLE
#define LOG_ERROR_ENABLE
#define LOG_DUMP_ENABLE
#include "mesh_log.h"


#if ADAPTATION_COMPILE_DEBUG

u32 k_uptime_get(void)
{
    return 0;
}

u32 k_uptime_get_32(void)
{
    return 0;
}

u32 k_work_delayable_remaining_get(struct k_work_delayable *timer)
{
    return 0;
}

void k_work_cancel_delayable(struct k_work_delayable *timer) {}

void k_work_schedule(struct k_work_delayable *timer, u32 timeout) {}

void k_work_submit(struct k_work *work) {}

void k_work_init_delayable(struct k_work_delayable *timer, void *callback) {}

#else

#define WORK_CALLBACK(_func, _param)   ((void (*)(struct k_work *))_func)(_param)

u32 k_uptime_get(void)
{
    return sys_timer_get_ms();
}

u32 k_uptime_get_32(void)
{
    return sys_timer_get_ms();
}

u32 k_work_delayable_remaining_get(struct k_work_delayable *timer)
{
    return timer->end_time - k_uptime_get_32();
}

/**
 * @brief Convert system ticks to milliseconds (64-bit version).
 *
 * This function converts system ticks to milliseconds, using 64-bit arithmetic
 * to ensure accuracy and handle large tick values.
 *
 * @param ticks System ticks.
 * @return Converted milliseconds.
 */
uint64_t k_ticks_to_ms_near64(uint64_t ticks)
{
    // 计算每个tick代表的毫秒数
    uint64_t ms = (ticks * 1000) / TCFG_CLOCK_SYS_HZ;
    return ms;
}

uint32_t k_ticks_to_ms_ceil32(uint32_t t)
{
    // 计算每个tick代表的毫秒数
    uint32_t ms = (t * 1000 + TCFG_CLOCK_SYS_HZ - 1) / TCFG_CLOCK_SYS_HZ;
    return ms;
}

void k_work_cancel_delayable(struct k_work_delayable *timer)
{
    LOG_INF("--func=%s", __FUNCTION__);

    if (timer->work.systimer) {
        sys_timeout_del(timer->work.systimer);
        LOG_INF("timer remove id = %u, 0x%x", timer->work.systimer, timer);
    }

    timer->work.systimer = 0;
}

/**
 * @brief Get elapsed time.
 *
 * This routine computes the elapsed time between the current system uptime
 * and an earlier reference time, in milliseconds.
 *
 * @param reftime Pointer to a reference time, which is updated to the current
 *                uptime upon return.
 *
 * @return Elapsed time.
 */
int64_t k_uptime_delta(int64_t *reftime)
{
    int64_t uptime, delta;

    uptime = k_uptime_get();
    delta = uptime - *reftime;
    *reftime = uptime;

    return delta;
}



static void k_work_delayable_cb_entry(struct k_work_delayable *timer)
{
    LOG_INF("--func=%s, 0x%x", __FUNCTION__, timer);

    k_work_cancel_delayable(timer);

    WORK_CALLBACK(timer->work.callback, &timer->work);
}

void k_work_schedule(struct k_work_delayable *timer, u32 timeout)
{
    LOG_INF("--func=%s", __FUNCTION__);
    LOG_INF("timeout= %d ms", timeout);

    /* k_work_cancel_delayable(timer); */

    if (0 == timer->work.systimer) {
        /* timer->work.systimer = sys_timer_register(timeout, k_work_delayable_cb_entry); */
        /* sys_timer_set_context(timer->work.systimer, timer); */
        timer->work.systimer = sys_timer_add(timer, k_work_delayable_cb_entry, timeout);
        LOG_INF("reg new id = %u, 0x%x", timer->work.systimer, timer);
    } else {
        sys_timer_modify(timer->work.systimer, timeout);
        LOG_INF("only change");
    }

    timer->end_time = k_uptime_get_32() + timeout;

    ASSERT(timer->work.systimer);
}

void k_work_submit(struct k_work *work)
{
    WORK_CALLBACK(work->callback, work);
}

void k_work_init_delayable(struct k_work_delayable *timer, void *callback)
{
    timer->work.callback = callback;
}

// mesh v1.1 update
#define SYS_PORT_TRACING_OBJ_INIT(obj_type, obj, ...) do { } while (false)
void k_work_init(struct k_work *work,
                 k_work_handler_t handler)
{
    __ASSERT_NO_MSG(work != NULL);
    __ASSERT_NO_MSG(handler != NULL);

    *work = (struct k_work)Z_WORK_INITIALIZER(handler);

    SYS_PORT_TRACING_OBJ_INIT(k_work, work);
}

int k_work_reschedule(struct k_work_delayable *dwork, u32_t delay)
{
    k_work_schedule(dwork, delay);
}

void net_buf_simple_clone(const struct net_buf_simple *original,
                          struct net_buf_simple *clone)
{

}

bool k_work_delayable_is_pending(const struct k_work_delayable *dwork)
{
    return 1;
}
#endif /* ADAPTATION_COMPILE_DEBUG */

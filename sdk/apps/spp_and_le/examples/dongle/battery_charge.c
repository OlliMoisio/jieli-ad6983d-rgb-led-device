#include "app_charge.h"
#include "app_power_manage.h"
#include "battery_charge.h"
#include "system/includes.h"

static u8 is_app_dongle_active = 0;

void dongle_power_event_to_user(u8 event)
{
    struct sys_event e;
    e.type = SYS_DEVICE_EVENT;
    e.arg  = (void *)DEVICE_EVENT_FROM_POWER;
    e.u.dev.event = event;
    e.u.dev.value = 0;
    sys_event_notify(&e);
}

static void dongle_set_soft_poweroff(void)
{
    is_app_dongle_active = 1;
    sys_timeout_add(NULL, power_set_soft_poweroff, 300);
}

static u8 dongle_state_idle_query(void)
{
    return !is_app_dongle_active;
}

#include "system/app_core.h"
#include "system/includes.h"
#include "server/server_core.h"
#include "app_config.h"
#include "app_action.h"
#include "config/config_transport.h"
#include "rcsp_user_update.h"
#include "usb/device/hid.h"

#define LOG_TAG_CONST       DONGLE
#define LOG_TAG             "[DONGLE]"
#include "debug.h"

#include "led.h"
#include "battery_charge.h"

// handles the 64 bytes of data from the PC
u8 usb_data_handler(u8 *rx_data, u32 rx_len)
{
    return led_data_handler(rx_data, rx_len);
}

static void dongle_app_start()
{

    usb_init();
    led_init();

    log_info("=======================================");
    log_info("---------   RGB LED DEVICE   ---------");
    log_info("=======================================");

    usb_hid_set_rx_callback(usb_data_handler);

    log_info("app_file: %s", __FILE__);

    clk_set("sys", BT_NORMAL_HZ);

    usb_start();

}

// Jieli boilerplate

static int dongle_state_machine(struct application *app, enum app_state state, struct intent *it)
{
    switch (state) {
    case APP_STA_CREATE:
        break;
    case APP_STA_START:
        if (!it) {
            break;
        }
        switch (it->action) {
        case ACTION_DONGLE_MAIN:
            dongle_app_start();
            break;
        }
        break;
    case APP_STA_PAUSE:
        break;
    case APP_STA_RESUME:
        break;
    case APP_STA_STOP:
        break;
    case APP_STA_DESTROY:
        log_info("APP_STA_DESTROY\n");
        break;
    }

    return 0;
}

static int dongle_event_handler(struct application *app, struct sys_event *event)
{
    switch (event->type) {

    case SYS_DEVICE_EVENT:
        if ((u32)event->arg == DEVICE_EVENT_FROM_POWER) {
            return app_power_event_handler(&event->u.dev, dongle_set_soft_poweroff);
        }

        return 0;

    default:
        return FALSE;
    }
    return FALSE;
}

static const struct application_operation app_dongle_ops = {
    .state_machine  = dongle_state_machine,
    .event_handler 	= dongle_event_handler,
};

REGISTER_APPLICATION(app_dongle) = {
    .name 	= "dongle",
    .action	= ACTION_DONGLE_MAIN,
    .ops 	= &app_dongle_ops,
    .state  = APP_STA_DESTROY,
};

REGISTER_LP_TARGET(dongle_state_lp_target) = {
    .name = "dongle_state_deal",
    .is_idle = dongle_state_idle_query,
};

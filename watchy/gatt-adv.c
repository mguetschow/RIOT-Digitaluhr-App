/*
 * Copyright (C) 2018 Freie Universität Berlin
 *               2018 Codecoup
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       BLE peripheral example using NimBLE
 *
 * Have a more detailed look at the api here:
 * https://mynewt.apache.org/latest/tutorials/ble/bleprph/bleprph.html
 *
 * More examples (not ready to run on RIOT) can be found here:
 * https://github.com/apache/mynewt-nimble/tree/master/apps
 *
 * Test this application e.g. with Nordics "nRF Connect"-App
 * iOS: https://itunes.apple.com/us/app/nrf-connect/id1054362403
 * Android: https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Andrzej Kaczmarek <andrzej.kaczmarek@codecoup.pl>
 * @author      Hendrik van Essen <hendrik.ve@fu-berlin.de>
 *
 * @}
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <nimble_riot.h>
#include <nimble_autoadv.h>

#include <host/ble_hs.h>
#include <host/util/util.h>
#include <host/ble_gatt.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#include "watchy.h"
#include "watchy_events.h"

#define ENABLE_DEBUG 0
#include "debug.h"


#define GATT_DEVICE_INFO_UUID                   0x180A
#define GATT_MANUFACTURER_NAME_UUID             0x2A29
#define GATT_MODEL_NUMBER_UUID                  0x2A24

#define STR_ANSWER_BUFFER_SIZE 100

static uint16_t _conn_handle=0;
static uint16_t _nus_val_handle;

/* Nordic UART Service - NUS */
/* UUID = 6E400001-B5A3-F393-E0A9-E50E24DCCA9E */
static const ble_uuid128_t gatt_svr_svc_nus_uuid
        = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93,
                0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

/* NUS RX characteristic */
/* UUID = 6E400002-B5A3-F393-E0A9-E50E24DCCA9E */
static const ble_uuid128_t gatt_svr_chr_nus_rx_uuid
        = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93,
                0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

/* NUS TX characteristic */
/* UUID = 6E400003-B5A3-F393-E0A9-E50E24DCCA9E */
static const ble_uuid128_t gatt_svr_chr_nus_tx_uuid
        = BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93,
                0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

/* --------------------- */

/* UUID = 1bce38b3-d137-48ff-a13e-033e14c7a335 */
static const ble_uuid128_t gatt_svr_svc_rw_demo_uuid
        = BLE_UUID128_INIT(0x35, 0xa3, 0xc7, 0x14, 0x3e, 0x03, 0x3e, 0xa1, 0xff,
                0x48, 0x37, 0xd1, 0xb3, 0x38, 0xce, 0x1b);

/* UUID = 35f28386-3070-4f3b-ba38-27507e991762 */
static const ble_uuid128_t gatt_svr_chr_rw_demo_write_uuid
        = BLE_UUID128_INIT(0x62, 0x17, 0x99, 0x7e, 0x50, 0x27, 0x38, 0xba, 0x3b,
                0x4f, 0x70, 0x30, 0x86, 0x83, 0xf2, 0x35);

/* UUID = ccdd113f-40d5-4d68-86ac-a728dd82f4aa */
static const ble_uuid128_t gatt_svr_chr_rw_demo_readonly_uuid
        = BLE_UUID128_INIT(0xaa, 0xf4, 0x82, 0xdd, 0x28, 0xa7, 0xac, 0x86, 0x68,
                0x4d, 0xd5, 0x40, 0x3f, 0x11, 0xdd, 0xcc);

static char rm_demo_write_data[64] = "This characteristic is read- and writeable!";

static int gatt_svr_chr_access_device_info_manufacturer(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svr_chr_access_device_info_model(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svr_chr_access_rw_demo(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg);

static char str_answer[STR_ANSWER_BUFFER_SIZE];


static int gatt_svr_nus_rxtx(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg);


/* define several bluetooth services for our device */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    /*
     * access_cb defines a callback for read and write access events on
     * given characteristics
     */
    {
        /* Service: Device Information */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_INFO_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /* Characteristic: * Manufacturer name */
            .uuid = BLE_UUID16_DECLARE(GATT_MANUFACTURER_NAME_UUID),
            .access_cb = gatt_svr_chr_access_device_info_manufacturer,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /* Characteristic: Model number string */
            .uuid = BLE_UUID16_DECLARE(GATT_MODEL_NUMBER_UUID),
            .access_cb = gatt_svr_chr_access_device_info_model,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* No more characteristics in this service */
        }, }
    },
    {
        /* Service: Read/Write Demo */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = (ble_uuid_t*) &gatt_svr_svc_rw_demo_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /* Characteristic: Read/Write Demo write */
            .uuid = (ble_uuid_t*) &gatt_svr_chr_rw_demo_write_uuid.u,
            .access_cb = gatt_svr_chr_access_rw_demo,
            .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
        }, {
            /* Characteristic: Read/Write Demo read only */
            .uuid = (ble_uuid_t*) &gatt_svr_chr_rw_demo_readonly_uuid.u,
            .access_cb = gatt_svr_chr_access_rw_demo,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* No more characteristics in this service */
        }, }
    },
    {
        /* Service: Nordic UART - NUS */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = (ble_uuid_t*) &gatt_svr_svc_nus_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /* Characteristic: NUS RX */
            .uuid = (ble_uuid_t*) &gatt_svr_chr_nus_rx_uuid.u,
            .access_cb = gatt_svr_nus_rxtx,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
        }, {
            /* Characteristic: NUS TX */
            .uuid = (ble_uuid_t*) &gatt_svr_chr_nus_tx_uuid.u,
            .access_cb = gatt_svr_nus_rxtx,
            .val_handle = &_nus_val_handle,
            .flags = BLE_GATT_CHR_F_NOTIFY,
        }, {
            0, /* No more characteristics in this service */
        }, }
    },
    {
        0, /* No more services */
    },
};

static int gatt_svr_chr_access_device_info_manufacturer(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    DEBUG("service 'device info: manufacturer' callback triggered");

    (void) conn_handle;
    (void) attr_handle;
    (void) arg;

    snprintf(str_answer, STR_ANSWER_BUFFER_SIZE,
             "This is RIOT! (Version: %s)\n", RIOT_VERSION);
    DEBUG("%s", str_answer);

    int rc = os_mbuf_append(ctxt->om, str_answer, strlen(str_answer));

    return rc;
}

static int gatt_svr_chr_access_device_info_model(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    DEBUG("service 'device info: model' callback triggered");

    (void) conn_handle;
    (void) attr_handle;
    (void) arg;

    snprintf(str_answer, STR_ANSWER_BUFFER_SIZE,
             "You are running RIOT on a(n) %s board, "
             "which features a(n) %s MCU.", RIOT_BOARD, RIOT_MCU);
    DEBUG("%s", str_answer);

    int rc = os_mbuf_append(ctxt->om, str_answer, strlen(str_answer));

    return rc;
}

int gatt_svr_nus_tx(char *buf, int len)
{
    struct os_mbuf *om;
    int res=-1;

    om = ble_hs_mbuf_from_flat(buf, len);
    res = ble_gatts_notify_custom(_conn_handle, _nus_val_handle, om);

    return res;
}

static char _nus_rx_buf[21];
static uint8_t _nus_rx_len=0;

int watchy_gatt_nus_get_rx(char *buf, int len)
{
    if (len > _nus_rx_len)
        len = _nus_rx_len;
    memcpy(buf, _nus_rx_buf, len);

    return len;
}

static int gatt_svr_nus_rxtx(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    //DEBUG("service 'NUS' callback triggered\n");

    (void) conn_handle;
    (void) attr_handle;
    (void) arg;

    int rc = 0;

    ble_uuid_t* rx_uuid = (ble_uuid_t*) &gatt_svr_chr_nus_rx_uuid.u;
    // ble_uuid_t* tx_uuid = (ble_uuid_t*) &gatt_svr_chr_nus_tx_uuid.u;

    if (ble_uuid_cmp(ctxt->chr->uuid, rx_uuid) == 0) {

        // DEBUG("access to characteristic 'NUS rx'\n");

        switch (ctxt->op) {
            case BLE_GATT_ACCESS_OP_READ_CHR:
                //DEBUG("read from characteristic\n");
                break;

            case BLE_GATT_ACCESS_OP_WRITE_CHR: {
                // DEBUG("write to characteristic\n");
                uint16_t om_len;
                om_len = OS_MBUF_PKTLEN(ctxt->om);

                if (om_len > 20)
                    DEBUG("NUS RX>20!\n");
                /* read sent data */
                rc = ble_hs_mbuf_to_flat(ctxt->om, _nus_rx_buf,
                                         sizeof _nus_rx_buf, &om_len);
                /* we need to null-terminate the received string */
                _nus_rx_buf[om_len] = '\0';
                _nus_rx_len = om_len;

                //DEBUG("NUS RX: '%s'\n", _nus_rx_buf);
                watchy_event_queue_add(EV_BT_NUS);
                break;
            }
            default:
                DEBUG("unhandled operation! %d\n", ctxt->op);
                rc = 1;
                break;
        }
        // gatt_svr_nus_tx("Dies sind mehr als 20 Zeichen!", 30);
        return rc;
    }

    DEBUG("unhandled uuid!");
    return 1;
}

static int gatt_svr_chr_access_rw_demo(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    DEBUG("service 'rw demo' callback triggered");

    (void) conn_handle;
    (void) attr_handle;
    (void) arg;

    int rc = 0;

    ble_uuid_t* write_uuid = (ble_uuid_t*) &gatt_svr_chr_rw_demo_write_uuid.u;
    ble_uuid_t* readonly_uuid = (ble_uuid_t*) &gatt_svr_chr_rw_demo_readonly_uuid.u;

    if (ble_uuid_cmp(ctxt->chr->uuid, write_uuid) == 0) {

        DEBUG("access to characteristic 'rw demo (write)'");

        switch (ctxt->op) {

            case BLE_GATT_ACCESS_OP_READ_CHR:
                DEBUG("read from characteristic\n");
                DEBUG("current value of rm_demo_write_data: '%s'\n",
                       rm_demo_write_data);

                /* send given data to the client */
                rc = os_mbuf_append(ctxt->om, &rm_demo_write_data,
                                    strlen(rm_demo_write_data));

                break;

            case BLE_GATT_ACCESS_OP_WRITE_CHR:
                DEBUG("write to characteristic\n");

                DEBUG("old value of rm_demo_write_data: '%s'\n",
                       rm_demo_write_data);

                uint16_t om_len;
                om_len = OS_MBUF_PKTLEN(ctxt->om);

                /* read sent data */
                rc = ble_hs_mbuf_to_flat(ctxt->om, &rm_demo_write_data,
                                         sizeof rm_demo_write_data, &om_len);
                /* we need to null-terminate the received string */
                rm_demo_write_data[om_len] = '\0';

                DEBUG("new value of rm_demo_write_data: '%s'\n",
                       rm_demo_write_data);

                break;

            case BLE_GATT_ACCESS_OP_READ_DSC:
                DEBUG("read from descriptor\n");
                break;

            case BLE_GATT_ACCESS_OP_WRITE_DSC:
                DEBUG("write to descriptor\n");
                break;

            default:
                DEBUG("unhandled operation!\n");
                rc = 1;
                break;
        }

        return rc;
    }
    else if (ble_uuid_cmp(ctxt->chr->uuid, readonly_uuid) == 0) {

        DEBUG("access to characteristic 'rw demo (read-only)'\n");

        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
            char random_digit;
            /* get random char between '0' and '9' */
            random_digit = 48 + (rand() % 10);

            snprintf(str_answer, STR_ANSWER_BUFFER_SIZE,
                     "new random number: %c", random_digit);
            DEBUG("%s\n", str_answer);

            rc = os_mbuf_append(ctxt->om, &str_answer, strlen(str_answer));

            return rc;
        }

        return 0;
    }

    DEBUG("unhandled uuid!\n");
    return 1;
}

static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status) {
                nimble_autoadv_start(NULL);
                return 0;
            }
            _conn_handle = event->connect.conn_handle;
            watch_state.bluetooth_pwr = BT_CONN;
            DEBUG("connected\n");
            watchy_event_queue_add(EV_BT_CONN);
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            _conn_handle = 0;
            nimble_autoadv_start(NULL);
            watch_state.bluetooth_pwr = BT_ON;
            DEBUG("disconnected\n");
            watchy_event_queue_add(EV_BT_CONN);
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            DEBUG("subscribe\n");
            if (event->subscribe.attr_handle == _nus_val_handle) {
                if (event->subscribe.cur_notify == 1) {
                    DEBUG("start notif\n");
                }
                else {
                    DEBUG("stop notif\n");
                }
            }
            break;
    }

    return 0;
}

int watchy_gatt_init (void)
{
    int rc = 0;
    (void)rc;

    _conn_handle = 0;

    /* verify and add our custom services */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    assert(rc == 0);
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    assert(rc == 0);

    /* set the device name */
    ble_svc_gap_device_name_set("Watchy");
    /* reload the GATT server to link our added services */
    ble_gatts_start();

    nimble_autoadv_set_gap_cb(&gap_event_cb, NULL);

    return rc;
}

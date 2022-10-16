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
#include <tsrb.h>

#include <nimble_riot.h>
#include <nimble_autoadv.h>
#include "nimble/nimble_port.h"
//#include "net/bluetil/ad.h"


#include <host/ble_hs.h>
#include <host/util/util.h>
#include <host/ble_gatt.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#include <rtc_utils.h>

#include "watchy.h"
#include "watchy_events.h"
#include "gatt-adv.h"

#define ENABLE_DEBUG 0
#include "debug.h"

#if IS_USED(MODULE_STDIO_NUS)
#include <isrpipe.h>
extern isrpipe_t _isrpipe_stdin;
#endif

#if IS_USED(MODULE_SHELL_LOCK)
#include <shell_lock.h>
#endif

#define GATT_DEVICE_INFO_UUID                   0x180A
#define GATT_MANUFACTURER_NAME_UUID             0x2A29
#define GATT_MODEL_NUMBER_UUID                  0x2A24

#define STR_ANSWER_BUFFER_SIZE 100

static uint16_t _conn_handle=0;
static uint16_t _nus_val_handle;
static uint16_t _bas_battery_handle;

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


static int gatt_svr_chr_access_device_info(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg);


static char str_answer[STR_ANSWER_BUFFER_SIZE];


static int gatt_svr_nus_rxtx(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svc_bas_access(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svc_time_access(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svc_alert_notification(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svc_immediate_alert(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg);

/* define several bluetooth services for our device */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    /*
     * access_cb defines a callback for read and write access events on
     * given characteristics
     */
    {	
		// 0x2a29 Manufacturer Name String
		// 0x2a24 Model Number String
    	// 0x2a25 Serial Number String
    	// 0x2a26 Firmware Revision String
    	// 0x2a27 Hardware Revision String
    	// 0x2a28 Software Revision String
        /* Service: Device Information */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_INFO_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) { {
            /* Characteristic: * Manufacturer name */
            .uuid = BLE_UUID16_DECLARE(GATT_MANUFACTURER_NAME_UUID),
            .access_cb = gatt_svr_chr_access_device_info,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /* Characteristic: Model number string */
            .uuid = BLE_UUID16_DECLARE(GATT_MODEL_NUMBER_UUID),
            .access_cb = gatt_svr_chr_access_device_info,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /* Characteristic: Serial Number String */
            .uuid = BLE_UUID16_DECLARE(0x2a25),
            .access_cb = gatt_svr_chr_access_device_info,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /* Characteristic: Firmware Revision String */
            .uuid = BLE_UUID16_DECLARE(0x2a26),
            .access_cb = gatt_svr_chr_access_device_info,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /* Characteristic: Hardware Revision String */
            .uuid = BLE_UUID16_DECLARE(0x2a27),
            .access_cb = gatt_svr_chr_access_device_info,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            /* Characteristic: Software Revision String */
            .uuid = BLE_UUID16_DECLARE(0x2a28),
            .access_cb = gatt_svr_chr_access_device_info,
            .flags = BLE_GATT_CHR_F_READ,
        }, {
            0, /* No more characteristics in this service */
        }, }
    },
	{
		/*** Battery Service. */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0x180F),
		.characteristics = (struct ble_gatt_chr_def[]) { {
			/*** Battery level characteristic */
			.uuid = BLE_UUID16_DECLARE(0x2A19),
			.access_cb = gatt_svc_bas_access,
			.val_handle = &_bas_battery_handle,
			.flags = BLE_GATT_CHR_F_READ |
				BLE_GATT_CHR_F_NOTIFY |
				0,
		}, {
			0, /* No more characteristics in this service. */
		} },
    },
	{
		/*** Current Time Service. */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0x1805),
		.characteristics = (struct ble_gatt_chr_def[]) { {
			/*** Current Time Characteristic Read */
			.uuid = BLE_UUID16_DECLARE(0x2a2b),
			.access_cb = gatt_svc_time_access,
			.flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
		}, {
			0, /* No more characteristics in this service. */
		} },
    },
	{
		/*** Immediate Alert */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0x1802),
		.characteristics = (struct ble_gatt_chr_def[]) { {
			/*** Current Time Characteristic Read */
			.uuid = BLE_UUID16_DECLARE(0x2a06),
			.access_cb = gatt_svc_immediate_alert,
			.flags = BLE_GATT_CHR_F_WRITE,
		}, {
			0, /* No more characteristics in this service. */
		} },
    },
	{
		/*** Alert Notification Service. */
		.type = BLE_GATT_SVC_TYPE_PRIMARY,
		.uuid = BLE_UUID16_DECLARE(0x1811),
		.characteristics = (struct ble_gatt_chr_def[]) { {
			/*** Current Time Characteristic Read */
			.uuid = BLE_UUID16_DECLARE(0x2a46),
			.access_cb = gatt_svc_alert_notification,
			.flags = BLE_GATT_CHR_F_WRITE,
		}, {
			0, /* No more characteristics in this service. */
		} },
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

static alert_t _last_alert;

	// 0x1802 Immediate Alert
	//   0x2a06 Alert Level, only one byte, no text or data
	//     0x00 No Alert
	//     0x01 Mild Alert
	//     0x02 Hihg Alert
static int gatt_svc_immediate_alert(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	(void) conn_handle;
	(void) attr_handle;
	(void) arg;
	uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
	int rc;
	uint8_t ialert=0;

	switch (uuid16) {
		case 0x2a06:
			assert(ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR);
			uint16_t om_len;
			om_len = OS_MBUF_PKTLEN(ctxt->om);
			rc = ble_hs_mbuf_to_flat(ctxt->om, &ialert,
				sizeof ialert, &om_len);
			return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
			break;
		default:
			assert(0);
			return BLE_ATT_ERR_UNLIKELY;
	}
	return BLE_ATT_ERR_UNLIKELY;
}

// Alert notification Service UID 0x1811
// New Alert 0x2a46, write
// uint8 alert category
//   1-email, 2-news, 3-call, 4-missed-call, 5-sms/mms, 6-voice-mail, 7-schedule
//   8-high-priority-alert, 9-instant-message, 251-service-specific - 255
// uint8 number of new alerts
// char *  optional text
static int gatt_svc_alert_notification(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	(void) conn_handle;
	(void) attr_handle;
	(void) arg;
	uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
	int rc;
	static uint8_t alertbuf[128];

	switch (uuid16) {
		case 0x2a46:
			if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
				uint16_t om_len;
				om_len = OS_MBUF_PKTLEN(ctxt->om);
	
				/* read sent data */
				memset(alertbuf, 0, sizeof alertbuf);
				rc = ble_hs_mbuf_to_flat(ctxt->om, alertbuf,
					sizeof alertbuf, &om_len);
				memcpy(&_last_alert.when, &watch_state.clock, sizeof(watch_state.clock));
				_last_alert.type = alertbuf[0];
				_last_alert.num_new = alertbuf[1];
				_last_alert.text = (char *)alertbuf+2;
				watchy_event_queue_add(EV_BT_ALERT);
				return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
			}
			break;

		default:
			assert(0);
			return BLE_ATT_ERR_UNLIKELY;
	}
	return BLE_ATT_ERR_UNLIKELY;
}

//0 Year
//1 Year
//2 Month
//3 Day
//4 Hours
//5 Minutes
//6 Seconds
//7 Day of Week (0 = unknown)
//8 Fractions256 (0 = uknown)
//9 Adjust Reason (0x03 = Manual Update => External Reference => No Time Zone Change => No DST Change)
static int gatt_svc_time_access(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	(void) conn_handle;
	(void) attr_handle;
	(void) arg;
    uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
    int rc;
    uint8_t btime[10];

    switch (uuid16) {
	    case 0x2a2b:
	        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
	        	*(uint16_t *)&btime[0] = (uint16_t)(watch_state.clock.tm_year + TM_YEAR_OFFSET);
	        	btime[2] = (uint8_t)watch_state.clock.tm_mon + 1;
	        	btime[3] = (uint8_t)watch_state.clock.tm_mday;
	        	btime[4] = (uint8_t)watch_state.clock.tm_hour;
	        	btime[5] = (uint8_t)watch_state.clock.tm_min;
	        	btime[6] = (uint8_t)watch_state.clock.tm_sec;
	        	btime[7] = (uint8_t)watch_state.clock.tm_wday;
	        	btime[8] = 0x00;
	        	btime[9] = 0x00;
				rc = os_mbuf_append(ctxt->om, btime,
					sizeof btime);
				return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
			}
			if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
				uint16_t om_len;
				om_len = OS_MBUF_PKTLEN(ctxt->om);
	
				if (om_len > 10)
					DEBUG("time RX>10!\n");
				/* read sent data */
				rc = ble_hs_mbuf_to_flat(ctxt->om, btime,
					sizeof btime, &om_len);
				for (int i=0; i<10; i++)
					printf("%02x ", btime[i]);
				printf("\n");
				watch_state.clock.tm_year = *(uint16_t *)&btime[0] - TM_YEAR_OFFSET;
				watch_state.clock.tm_mon = btime[2] - 1;
				watch_state.clock.tm_mday = btime[3];
				watch_state.clock.tm_hour = btime[4];
				watch_state.clock.tm_min = btime[5];
				watch_state.clock.tm_sec = btime[6];
				rtc_tm_normalize(&watch_state.clock);
				return (rc == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
			}
			break;
		default:
			assert(0);
			return BLE_ATT_ERR_UNLIKELY;
	}
	return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_svc_bas_access(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	(void) conn_handle;
	(void) attr_handle;
	(void) arg;
	uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
	int rc;

	switch (uuid16) {
	case 0x2A19:
		assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
		rc = os_mbuf_append(ctxt->om, &watch_state.pwr_stat.battery_percent,
					sizeof watch_state.pwr_stat.battery_percent);
		return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
		break;
	default:
		assert(0);
		return BLE_ATT_ERR_UNLIKELY;
	}
}

static int gatt_svr_chr_access_device_info(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	(void) conn_handle;
	(void) attr_handle;
	(void) arg;
	uint16_t uuid16 = ble_uuid_u16(ctxt->chr->uuid);
	int rc=BLE_ATT_ERR_UNLIKELY;

	switch (uuid16) {
		case 0x2A29:
			assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
			snprintf(str_answer, STR_ANSWER_BUFFER_SIZE,
				"This is RIOT! (Version: %s)\n", RIOT_VERSION);

			rc = os_mbuf_append(ctxt->om, str_answer, strlen(str_answer));
			break;
		case 0x2a24:
			assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
			snprintf(str_answer, STR_ANSWER_BUFFER_SIZE,
				"You are running RIOT on a(n) %s board, "
				"which features a(n) %s MCU.", RIOT_BOARD, RIOT_MCU);

			rc = os_mbuf_append(ctxt->om, str_answer, strlen(str_answer));
			break;
		case 0x2a25: // Serial Number String
			assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
			snprintf(str_answer, STR_ANSWER_BUFFER_SIZE, "2210420123");
			rc = os_mbuf_append(ctxt->om, str_answer, strlen(str_answer));
			break;
		case 0x2a26: // Firmware Revision String
			assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
			snprintf(str_answer, STR_ANSWER_BUFFER_SIZE, "FW V0.01");
			rc = os_mbuf_append(ctxt->om, str_answer, strlen(str_answer));
			break;
		case 0x2a27: // Hardware Revision String
			assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
			snprintf(str_answer, STR_ANSWER_BUFFER_SIZE, "Bangle.JS2");
			rc = os_mbuf_append(ctxt->om, str_answer, strlen(str_answer));
			break;
		case 0x2a28: // Software Revision String
			assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);
			snprintf(str_answer, STR_ANSWER_BUFFER_SIZE, "SW V0.01");
			rc = os_mbuf_append(ctxt->om, str_answer, strlen(str_answer));
			break;
		default:
			DEBUG("BT dev info unhandled UUID 0x%04x\n", uuid16);
			break;
	}
    return rc;
}


static uint8_t _tsrb_nus_tx_mem[512];
static tsrb_t _tsrb_nus_tx = TSRB_INIT(_tsrb_nus_tx_mem);

#if IS_USED(MODULE_STDIO_NUS)
#else
static uint8_t _tsrb_nus_rx_mem[80];
static tsrb_t _tsrb_nus_rx = TSRB_INIT(_tsrb_nus_rx_mem);
#endif

static struct ble_npl_callout _send_nus_tx_callout;
#define CALLOUT_TICKS_MS    1

ssize_t gatt_svr_nus_tx_buf(char *buf, unsigned int len)
{
    //int res=-1;

	ble_npl_callout_reset(&_send_nus_tx_callout, CALLOUT_TICKS_MS);

	if (len > tsrb_free(&_tsrb_nus_tx))
		len = tsrb_free(&_tsrb_nus_tx);

    return tsrb_add(&_tsrb_nus_tx, (uint8_t *)(buf), len);
}


static void _npl_tx_cb(struct ble_npl_event *ev)
{
	(void)ev;
    struct os_mbuf *om;
    int res=-1;
    ssize_t len;
    uint8_t txb[21];

	//DEBUG("tx %d\n", len);

	len = tsrb_peek(&_tsrb_nus_tx, txb, 20);

	if (len == 0)
		return;

	ble_npl_callout_reset(&_send_nus_tx_callout, CALLOUT_TICKS_MS);

    om = ble_hs_mbuf_from_flat(txb, len);
    res = ble_gatts_notify_custom(_conn_handle, _nus_val_handle, om);
    if (res == 0)
    	tsrb_drop(&_tsrb_nus_tx, len);
}

int watchy_gatt_nus_get_rx(char *buf, unsigned int len)
{
#if IS_USED(MODULE_STDIO_NUS)
	(void) buf;
	(void) len;
	return 0;
#else
	return tsrb_get(&_tsrb_nus_rx, (uint8_t *)buf, len);
#endif
}

static int gatt_svr_nus_rxtx(
        uint16_t conn_handle, uint16_t attr_handle,
        struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    //DEBUG("service 'NUS' callback triggered\n");

    (void) conn_handle;
    (void) attr_handle;
    (void) arg;
	char _nus_rx_buf[21];
	uint8_t _nus_rx_len=0;

    int rc = 0;

    ble_uuid_t* rx_uuid = (ble_uuid_t*) &gatt_svr_chr_nus_rx_uuid.u;
    // ble_uuid_t* tx_uuid = (ble_uuid_t*) &gatt_svr_chr_nus_tx_uuid.u;

    if (ble_uuid_cmp(ctxt->chr->uuid, rx_uuid) == 0) {

        // DEBUG("access to characteristic 'NUS rx'\n");

        switch (ctxt->op) {
            case BLE_GATT_ACCESS_OP_READ_CHR:
                DEBUG("read from characteristic\n");
                break;

            case BLE_GATT_ACCESS_OP_WRITE_CHR: {
                DEBUG("write to characteristic\n");
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
#if IS_USED(MODULE_STDIO_NUS)
				isrpipe_write(&_isrpipe_stdin, (uint8_t *)_nus_rx_buf, _nus_rx_len);
#else
                tsrb_add(&_tsrb_nus_rx,  (uint8_t *)_nus_rx_buf, _nus_rx_len);
                DEBUG("NUS RX: '%s'\n", _nus_rx_buf);
                watchy_event_queue_add(EV_BT_NUS);
#endif
                break;
            }
            default:
                DEBUG("unhandled operation! %d\n", ctxt->op);
                rc = 1;
                break;
        }
        return rc;
    }

    DEBUG("unhandled uuid!");
    return 1;
}


static int gap_event_cb(struct ble_gap_event *event, void *arg)
{
    (void)arg;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            DEBUG("GAP connect\n");
            if (event->connect.status) {
                nimble_autoadv_start(NULL);
                return 0;
            }
            // if (_conn_handle != 0)
            // 		ble_gap_terminate(event->connect.conn_handle, uint8_t hci_reason)
            _conn_handle = event->connect.conn_handle;
            watch_state.bluetooth_pwr = BT_CONN;
            watchy_event_queue_add(EV_BT_CONN);
#if IS_USED(MODULE_STDIO_NUS)
			tsrb_clear(&_isrpipe_stdin.tsrb);
            isrpipe_write_one(&_isrpipe_stdin, '\x03');
#endif
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            DEBUG("GAP disconnect\n");
            _conn_handle = 0;
            nimble_autoadv_start(NULL);
#if IS_USED(MODULE_SHELL_LOCK)
#if IS_USED(MODULE_STDIO_NUS)
            isrpipe_write_one(&_isrpipe_stdin, '\x03');
#endif
			shell_lock_now();
#endif
            watch_state.bluetooth_pwr = BT_ON;
            watchy_event_queue_add(EV_BT_CONN);
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            DEBUG("GAP subscribe\n");
            if (event->subscribe.attr_handle == _nus_val_handle) {
                if (event->subscribe.cur_notify == 1) {
                    DEBUG("start notif\n");
                }
                else {
                    DEBUG("stop notif\n");
                }
            }
            break;

        case BLE_GAP_EVENT_NOTIFY_TX:
            DEBUG("GAP notify TX\n");
            if (event->notify_tx.indication == 1 && (event->notify_tx.conn_handle == _conn_handle)) {
            }
            break;

        case BLE_GAP_EVENT_MTU:
            DEBUG("GAP MTU: %d\n", event->mtu.value);
            break;

		case BLE_GAP_EVENT_ENC_CHANGE:
            DEBUG("GAP ENC CHANGE\n");
			break;
		case BLE_GAP_EVENT_PASSKEY_ACTION:
            DEBUG("GAP PASSKEY ACTION\n");
			break;
		case BLE_GAP_EVENT_IDENTITY_RESOLVED:
            DEBUG("GAP IDENTITY RESOLVED\n");
			break;
		case BLE_GAP_EVENT_REPEAT_PAIRING:
            DEBUG("GAP REPEAT PAIRING\n");
			break;
		case BLE_GAP_EVENT_CONN_UPDATE:
            DEBUG("GAP CONN UPDATE\n");
			break;
		case BLE_GAP_EVENT_CONN_UPDATE_REQ:
            DEBUG("GAP CONN UPDATE REQ\n");
			break;
		case BLE_GAP_EVENT_L2CAP_UPDATE_REQ:
            DEBUG("GAP L2CAP UPDATE REQ\n");
			break;
        default:
            DEBUG("GAP unhandled: %d\n", event->type);
            break;
    }

    return 0;
}

int watchy_gatt_init (void)
{
    int rc = 0;

	_conn_handle = 0;
	ble_npl_callout_init(&_send_nus_tx_callout, nimble_port_get_dflt_eventq(),
                         _npl_tx_cb, NULL);

    /* verify and add our custom services */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    assert(rc == 0);
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    assert(rc == 0);

	/* set the device name */
	ble_svc_gap_device_name_set("Watchy");
	// Wearable computer (watch size)
	// according to
	// https://specificationrefs.bluetooth.com/assigned-values/Appearance%20Values.pdf
	ble_svc_gap_device_appearance_set(0x0086);
	/* reload the GATT server to link our added services */
	ble_gatts_start();

	nimble_autoadv_set_gap_cb(&gap_event_cb, NULL);

    return rc;
}

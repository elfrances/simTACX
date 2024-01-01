/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOSConfig.h"
/* BLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "ble.h"

static const char *tag = "NimBLE";

#define NOTIFY_TASK_PRIORITY    (configMAX_PRIORITIES - 4)
#define NOTIFY_TASK_STACK_SIZE  4096

static TaskHandle_t notifyTaskHandle;

static bool cpsCpmNotify = false;
static bool cpsPwrVecNotify = false;
static bool fec2Notify = false;

static uint16_t connHandle;

static const char *device_name = "TACX FLUX2 NNNN";

static int bleGapEvent(struct ble_gap_event *event, void *arg);

static uint8_t ble_cps_addr_type;

void print_addr(const void *addr)
{
    const uint8_t *u8p;

    u8p = addr;
    MODLOG_DFLT(INFO, "%02x:%02x:%02x:%02x:%02x:%02x",
                u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

// GET signed values

int8_t getSINT8(const uint8_t *data)
{
    uint8_t value = data[0];
    return (int8_t) value;
}

int16_t getSINT16(const uint8_t *data)
{
    uint16_t value = ((uint16_t) data[1] << 8) | (uint16_t) data[0];
    return (int16_t) value;
}

int32_t getSINT24(const uint8_t *data)
{
    uint32_t value = ((uint32_t) data[2] <<16) | ((uint32_t) data[1] << 8) | (uint32_t) data[0];
    return (int32_t) value;
}

int32_t getSINT32(const uint8_t *data)
{
    uint32_t value = ((uint32_t) data[3] << 24) | ((uint32_t) data[2] <<16) | ((uint32_t) data[1] << 8) | (uint32_t) data[0];
    return (int32_t) value;
}

// GET unsigned values

uint8_t getUINT8(const uint8_t *data)
{
    uint8_t value = data[0];
    return value;
}

uint16_t getUINT16(const uint8_t *data)
{
    uint16_t value = ((uint16_t) data[1] << 8) | (uint16_t) data[0];
    return value;
}

uint32_t getUINT24(const uint8_t *data)
{
    uint32_t value = ((uint32_t) data[2] <<16) | ((uint32_t) data[1] << 8) | (uint32_t) data[0];
    return value;
}

uint32_t getUINT32(const uint8_t *data)
{
    uint32_t value = ((uint32_t) data[3] << 24) | ((uint32_t) data[2] << 16) | ((uint32_t) data[1] << 8) | (uint32_t) data[0];
    return value;
}

// PUT signed values

void putSINT16(uint8_t *data, int16_t value)
{
    *data++ = (value & 0xff);
    *data = ((value >> 8) & 0xff);
}

// PUT unsigned values

void putUINT16(uint8_t *data, uint16_t value)
{
    *data++ = (value & 0xff);
    *data = ((value >> 8) & 0xff);
}

void putUINT24(uint8_t *data, uint32_t value)
{
    *data++ = (value & 0xff);
    *data++ = ((value >> 8) & 0xff);
    *data = ((value >> 16) & 0xff);
}

void putUINT32(uint8_t *data, uint32_t value)
{
    *data++ = (value & 0xff);
    *data++ = ((value >> 8) & 0xff);
    *data++ = ((value >> 16) & 0xff);
    *data = ((value >> 24) & 0xff);
}


/*
 * Enables advertising with parameters:
 *     o General discoverable mode
 *     o Undirected connectable mode
 */
static void bleAdvertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    int rc;

    /*
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info)
     *     o Advertising tx power
     *     o Device name
     */
    memset(&fields, 0, sizeof(fields));

    /*
     * Advertise two flags:
     *      o Discoverability in forthcoming advertisement (general)
     *      o BLE-only (BR/EDR unsupported)
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /*
     * Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 1;
    fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    
    fields.uuids16 = (ble_uuid16_t[]) {
        BLE_UUID16_INIT(GATT_CPS_UUID),
    };
    fields.num_uuids16 = 1;
    fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error setting advertisement data; rc=%d\n", rc);
        return;
    }

    /* Begin advertising */
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(ble_cps_addr_type, NULL, BLE_HS_FOREVER, &adv_params, bleGapEvent, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error enabling advertisement; rc=%d\n", rc);
        return;
    }
}

static void notifyTask(void *parms)
{
    struct os_mbuf *om = NULL;
    const TickType_t notifyPeriod = pdMS_TO_TICKS(1000);  // 1 second

    while (true) {
        TickType_t start, end;

        start = xTaskGetTickCount();

        if (cpsCpmNotify) {
            const uint16_t flags = CPM_INSTANT_POWER | CPM_PEDAL_POWER_BALANCE | CPM_CRANK_REVOLUTION_DATA;
            const int16_t instantPower = 225;
            const uint8_t pedalPowerBalance = 100;  // 50%
            static uint16_t cumulativeCrankRevolutions;
            static uint16_t lastCrankEventTime;
            struct CpmData cpmData;

            // 2 revolutions in 1 sec (1024 ticks) = 120 RPM
            cumulativeCrankRevolutions += 2;
            lastCrankEventTime += 1024;

            putUINT16(cpmData.flags, flags);
            putSINT16(cpmData.instPower, instantPower);
            cpmData.pedalPowerBalance = pedalPowerBalance;
            putUINT16(cpmData.cumulativeCrankRevolutions, cumulativeCrankRevolutions);
            putUINT16(cpmData.lastCrankEventTime, lastCrankEventTime);

            om = ble_hs_mbuf_from_flat(&cpmData, sizeof(cpmData));

            printf("ts: %" PRIu32 " cpsCpmNotify: { ", start);
            for (int i = 0; i < om->om_len; i++) {
                printf("0x%02x ", om->om_data[i]);
            }
            printf("}\n");

            ble_gatts_notify_custom(connHandle, cpsCpmHandle, om);
        }

        end = xTaskGetTickCount();

        vTaskDelay(notifyPeriod - (end - start));
    }
}

static int bleGapEvent(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed */
        MODLOG_DFLT(INFO, "connection %s; status=%d\n",
                    event->connect.status == 0 ? "established" : "failed",
                    event->connect.status);

        if (event->connect.status != 0) {
            /* Connection failed; resume advertising */
            bleAdvertise();
        }
        connHandle = event->connect.conn_handle;
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        MODLOG_DFLT(INFO, "disconnect; reason=%d\n", event->disconnect.reason);

        /* Connection terminated; resume advertising */
        bleAdvertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        MODLOG_DFLT(INFO, "adv complete\n");
        bleAdvertise();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "SUBSCRIBE: cur_notify=%u attr_handle=%u", event->subscribe.cur_notify, event->subscribe.attr_handle);
        bool enabled = !! event->subscribe.cur_notify;
        if (event->subscribe.attr_handle == cpsCpmHandle) {
            cpsCpmNotify = enabled;
        } else if (event->subscribe.attr_handle == cpsPwrVecHandle) {
            cpsPwrVecNotify = enabled;
        } else if (event->subscribe.attr_handle == fec2ChrHandle) {
            fec2Notify = enabled;
        }
        break;

    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.value);
        break;
    }

    return 0;
}

static void bleOnSync(void)
{
    int rc;

    rc = ble_hs_id_infer_auto(0, &ble_cps_addr_type);
    assert(rc == 0);

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(ble_cps_addr_type, addr_val, NULL);

    MODLOG_DFLT(INFO, "Device Address: ");
    print_addr(addr_val);
    MODLOG_DFLT(INFO, "\n");

    MODLOG_DFLT(INFO, "cpsCpmHandle=%u", cpsCpmHandle);
    MODLOG_DFLT(INFO, "fec2ChrHandle=%u", fec2ChrHandle);
    MODLOG_DFLT(INFO, "fec3ChrHandle=%u", fec3ChrHandle);
    MODLOG_DFLT(INFO, "\n");

    /* Begin advertising */
    bleAdvertise();
}

static void bleOnReset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void bleHostTask(void *param)
{
    ESP_LOGI(tag, "BLE Host Task Started");

    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void app_main(void)
{
    int rc;

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        MODLOG_DFLT(ERROR, "Failed to init nimble %d \n", ret);
        return;
    }

    /* Initialize the NimBLE host configuration */
    ble_hs_cfg.sync_cb = bleOnSync;
    ble_hs_cfg.reset_cb = bleOnReset;

    xTaskCreate(notifyTask, "notifyTask", NOTIFY_TASK_STACK_SIZE, NULL, NOTIFY_TASK_PRIORITY, &notifyTaskHandle);

    rc = gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name */
    rc = ble_svc_gap_device_name_set(device_name);
    assert(rc == 0);

    /* Start the task */
    nimble_port_freertos_init(bleHostTask);
}

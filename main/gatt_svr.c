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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/ans/ble_svc_ans.h"
#include "ble.h"
#include "sdkconfig.h"

static const char *manuf_name = "Garmin/Tacx";
static const char *model_num = "FLUX 2";
static const char *serial_num = "1234567890";
static const char *hard_rev = "1";
static const char *firm_rev = "0.0.0";

static const char *chrOp[] = {
        [BLE_GATT_ACCESS_OP_READ_CHR] = "READ_CHR",
        [BLE_GATT_ACCESS_OP_WRITE_CHR] = "WRITE_CHR",
        [BLE_GATT_ACCESS_OP_READ_DSC] = "READ_DSC",
        [BLE_GATT_ACCESS_OP_WRITE_DSC] = "WRITE_DSC",
};

uint16_t cpsCpmHandle;
uint16_t cpsPwrVecHandle;
uint16_t fec2ChrHandle;
uint16_t fec3ChrHandle;

static int gatt_svr_chr_access_device_info(uint16_t connHandle, uint16_t attrHandle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid;

    uuid = ble_uuid_u16(ctxt->chr->uuid);

    if (uuid == GATT_MANUFACTURER_NAME_UUID) {
        return (os_mbuf_append(ctxt->om, manuf_name, strlen(manuf_name)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    } else if (uuid == GATT_MODEL_NUMBER_UUID) {
        return (os_mbuf_append(ctxt->om, model_num, strlen(model_num)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    } else if (uuid == GATT_SERIAL_NUMBER_UUID) {
        return (os_mbuf_append(ctxt->om, serial_num, strlen(serial_num)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    } else if (uuid == GATT_HARDWARE_REVISION_UUID) {
        return (os_mbuf_append(ctxt->om, hard_rev, strlen(hard_rev)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    } else if (uuid == GATT_FIRMWARE_REVISION_UUID) {
        return (os_mbuf_append(ctxt->om, firm_rev, strlen(firm_rev)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_svr_chr_access_cycling_power(uint16_t connHandle, uint16_t attrHandle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    static uint8_t cycling_power_feature[4] = {0x09, 0x00, 0x00, 0x00}; // PEDAL_POWER_BALANCE, CRANK_REVOLUTION_DATA
    static uint8_t sensor_location[1] = {0x0d};  // Rear Hub
    char fmtBuf[BLE_UUID_STR_LEN];
    TickType_t ts = xTaskGetTickCount();

    MODLOG_DFLT(INFO, "connHandle=%u attrHandle=%u op=%s uuid=%s len=%u",
    		connHandle, attrHandle, chrOp[ctxt->op], ble_uuid_to_str(ctxt->chr->uuid, fmtBuf), ctxt->om->om_len);

    if (ctxt->chr->uuid->type == BLE_UUID_TYPE_16) {
        uint16_t uuid = ble_uuid_u16(ctxt->chr->uuid);

        if (uuid == GATT_CYCLING_POWER_FEATURE_UUID) {
            return (os_mbuf_append(ctxt->om, cycling_power_feature, sizeof(cycling_power_feature)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        } else if (uuid == GATT_SENSOR_LOCATION_UUID) {
            return (os_mbuf_append(ctxt->om, sensor_location, sizeof(sensor_location)) == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        } else if (uuid == GATT_CYCLING_POWER_CONTROL_POINT_UUID) {
            if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
                printf("ts: %" PRIu32 " cpsCp: { ", ts);
                for (int i = 0; i < ctxt->om->om_len; i++) {
                    printf("0x%02x ", ctxt->om->om_data[i]);
                }
                printf("}\n");
                return 0;
            }
        }
    }

    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

static int gatt_svr_chr_access_tacx_fec_over_ble_service(uint16_t connHandle, uint16_t attrHandle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
	char fmtBuf[BLE_UUID_STR_LEN];
	TickType_t ts = xTaskGetTickCount();

    MODLOG_DFLT(INFO, "connHandle=%u attrHandle=%u op=%s uuid=%s len=%u",
    		connHandle, attrHandle, chrOp[ctxt->op], ble_uuid_to_str(ctxt->chr->uuid, fmtBuf), ctxt->om->om_len);

    if (ctxt->chr->uuid->type == BLE_UUID_TYPE_128) {
        if (attrHandle == fec3ChrHandle) {
            if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
                printf("ts: %" PRIu32 " fec3Chr: { ", ts);
                for (int i = 0; i < ctxt->om->om_len; i++) {
                    printf("0x%02x ", ctxt->om->om_data[i]);
                }
                printf("}\n");
                return 0;
            }
        }
    }

    return 0;
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(INFO, "registered service %s with handle=%d\n",
                ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(INFO,
                "registering characteristic %s with " "def_handle=%d val_handle=%d\n",
                ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        MODLOG_DFLT(INFO, "registering descriptor %s with handle=%d\n",
                ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
        {
            // Device Information Service
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_INFO_UUID),
            .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    /* Characteristic: Manufacturer name */
                    .uuid = BLE_UUID16_DECLARE(GATT_MANUFACTURER_NAME_UUID),
                    .access_cb = gatt_svr_chr_access_device_info,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    /* Characteristic: Model number string */
                    .uuid = BLE_UUID16_DECLARE(GATT_MODEL_NUMBER_UUID),
                    .access_cb = gatt_svr_chr_access_device_info,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    /* Characteristic: Serial number string */
                    .uuid = BLE_UUID16_DECLARE(GATT_SERIAL_NUMBER_UUID),
                    .access_cb = gatt_svr_chr_access_device_info,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    /* Characteristic: Hardware Revision string */
                    .uuid = BLE_UUID16_DECLARE(GATT_HARDWARE_REVISION_UUID),
                    .access_cb = gatt_svr_chr_access_device_info,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    /* Characteristic: Firmware Revision string */
                    .uuid = BLE_UUID16_DECLARE(GATT_FIRMWARE_REVISION_UUID),
                    .access_cb = gatt_svr_chr_access_device_info,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    /* No more characteristics in this service */
                    0,
                },
            }
        },

        {
            // Cycling Power Service
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID16_DECLARE(GATT_CPS_UUID),
            .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    /* Characteristic: Cycling Power Measurement */
                    .uuid = BLE_UUID16_DECLARE(GATT_CYCLING_POWER_MEASUREMENT_UUID),
                    .access_cb = gatt_svr_chr_access_cycling_power,
                    .val_handle = &cpsCpmHandle,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                },
                {
                    /* Characteristic: Cycling Power Feature */
                    .uuid = BLE_UUID16_DECLARE(GATT_CYCLING_POWER_FEATURE_UUID),
                    .access_cb = gatt_svr_chr_access_cycling_power,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    /* Characteristic: Sensor Location */
                    .uuid = BLE_UUID16_DECLARE(GATT_SENSOR_LOCATION_UUID),
                    .access_cb = gatt_svr_chr_access_cycling_power,
                    .flags = BLE_GATT_CHR_F_READ,
                },
                {
                    /* Characteristic: Power Vector */
                    .uuid = BLE_UUID16_DECLARE(GATT_CYCLING_POWER_VECTOR_UUID),
                    .access_cb = gatt_svr_chr_access_cycling_power,
                    .val_handle = &cpsPwrVecHandle,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                },
                {
                    /* Characteristic: Cycling Power Control Point */
                    .uuid = BLE_UUID16_DECLARE(GATT_CYCLING_POWER_CONTROL_POINT_UUID),
                    .access_cb = gatt_svr_chr_access_cycling_power,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_INDICATE,
                },
                {
                    /* No more characteristics in this service */
                    0,
                },
            }
        },

        {
            // TACX FE-C Over BLE Service: 6e40fec1-b5a3-f393-e0a9-e50e24dcca9e
            .type = BLE_GATT_SVC_TYPE_PRIMARY,
            .uuid = BLE_UUID128_DECLARE(0x96, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0xc1, 0xfe, 0x40, 0x6e),
            .characteristics = (struct ble_gatt_chr_def[]) {
                {
                    /* Characteristic: ??? 6e40fec2-b5a3-f393-e0a9-e50e24dcca9e */
                    .uuid = BLE_UUID128_DECLARE(0x96, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0xc2, 0xfe, 0x40, 0x6e),
                    .access_cb = gatt_svr_chr_access_tacx_fec_over_ble_service,
                    .val_handle = &fec2ChrHandle,
                    .flags = BLE_GATT_CHR_F_NOTIFY,
                },
                {
                    /* Characteristic: ??? 6e40fec3-b5a3-f393-e0a9-e50e24dcca9e */
                    .uuid = BLE_UUID128_DECLARE(0x96, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, 0x93, 0xf3, 0xa3, 0xb5, 0xc3, 0xfe, 0x40, 0x6e),
                    .access_cb = gatt_svr_chr_access_tacx_fec_over_ble_service,
                    .val_handle = &fec3ChrHandle,
                    .flags = BLE_GATT_CHR_F_WRITE,
                },
                {
                    /* No more characteristics in this service */
                    0,
                },
            }
        },


    {
        /* No more services */
        0,
    },
};

int gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    if ((rc = ble_gatts_count_cfg(gatt_svr_svcs)) != 0) {
        return rc;
    }

    if ((rc = ble_gatts_add_svcs(gatt_svr_svcs)) != 0) {
        return rc;
    }

    return 0;
}

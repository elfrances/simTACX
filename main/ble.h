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

#pragma once

#include "nimble/ble.h"
#include "modlog/modlog.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int8_t getSINT8(const uint8_t *data);
extern int16_t getSINT16(const uint8_t *data);
extern int32_t getSINT24(const uint8_t *data);
extern int32_t getSINT32(const uint8_t *data);

extern uint8_t getUINT8(const uint8_t *data);
extern uint16_t getUINT16(const uint8_t *data);
extern uint32_t getUINT24(const uint8_t *data);
extern uint32_t getUINT32(const uint8_t *data);

extern void putSINT8(uint8_t *data, int8_t value);
extern void putSINT16(uint8_t *data, int16_t value);
extern void putSINT24(uint8_t *data, int32_t value);
extern void putSINT32(uint8_t *data, int32_t value);

extern void putUINT8(uint8_t *data, uint8_t value);
extern void putUINT16(uint8_t *data, uint16_t value);
extern void putUINT24(uint8_t *data, uint32_t value);
extern void putUINT32(uint8_t *data, uint32_t value);

// Device Info Service
#define GATT_DEVICE_INFO_UUID                   0x180A
#define GATT_MANUFACTURER_NAME_UUID                 0x2A29  // READ
#define GATT_MODEL_NUMBER_UUID                      0x2A24  // READ
#define GATT_SERIAL_NUMBER_UUID                     0x2A25  // READ
#define GATT_HARDWARE_REVISION_UUID                 0x2A27  // READ
#define GATT_FIRMWARE_REVISION_UUID                 0x2A26  // READ

// Battery Service
#define GATT_BATTERY_SERVICE_UUID               0x180F
#define GATT_BATTERY_LEVEL_UUID                     0x2A19  // READ

// Cycling Power Service
#define GATT_CPS_UUID                           0x1818
#define GATT_CYCLING_POWER_MEASUREMENT_UUID         0x2a63  // NOTIFY
#define GATT_CYCLING_POWER_VECTOR_UUID              0x2a64  // NOTIFY
#define GATT_CYCLING_POWER_FEATURE_UUID             0x2a65  // READ
#define GATT_CYCLING_POWER_CONTROL_POINT_UUID       0x2a66  // WRITE,INDICATE
#define GATT_SENSOR_LOCATION_UUID                   0x2a5d  // READ

// Cycling Power Feature
#define CPF_PEDAL_POWER_BALANCE                 0x00000001
#define CPF_CRANK_REVOLUTION_DATA               0x00000008

// Cycling Power Measurement
#define CPM_INSTANT_POWER                       0x00000001
#define CPM_PEDAL_POWER_BALANCE                 0x00000002
#define CPM_CRANK_REVOLUTION_DATA               0x00000020

struct CpmData {
    uint8_t flags[2];
    uint8_t instPower[2];
    uint8_t pedalPowerBalance;
    uint8_t cumulativeCrankRevolutions[2];
    uint8_t lastCrankEventTime[2];
} __attribute__((packed));

extern uint16_t cpsCpmHandle;
extern uint16_t cpsPwrVecHandle;
extern uint16_t fec2ChrHandle;
extern uint16_t fec3ChrHandle;

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);

int gatt_svr_init(void);

#ifdef __cplusplus
}
#endif

/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_RIL_NV_ITEMS_H
#define ANDROID_RIL_NV_ITEMS_H 1

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Must match the values in RadioNVItems.java. */
typedef enum {

    // CDMA radio information
    RIL_NV_CDMA_MEID = 1,                   // hex MEID
    RIL_NV_CDMA_PRL_VERSION = 2,            // CDMA PRL version

    // CDMA mobile account information
    RIL_NV_CDMA_MDN = 3,                    // CDMA MDN
    RIL_NV_CDMA_MIN = 4,                    // CDMA MIN (MSID)

    // Carrier device provisioning
    RIL_NV_DEVICE_MSL = 5,                  // device MSL
    RIL_NV_RTN_RECONDITIONED_STATUS = 6,    // RTN reconditioned status
    RIL_NV_RTN_ACTIVATION_DATE = 7,         // RTN activation date
    RIL_NV_RTN_LIFE_TIMER = 8,              // RTN life timer
    RIL_NV_RTN_LIFE_CALLS = 9,              // RTN life calls
    RIL_NV_RTN_LIFE_DATA_TX = 10,           // RTN life data TX
    RIL_NV_RTN_LIFE_DATA_RX = 11,           // RTN life data RX
    RIL_NV_OMADM_HFA_LEVEL = 12,            // HFA in progress

    // Mobile IP profile information
    RIL_NV_MIP_PROFILE_NAI = 13,            // NAI realm
    RIL_NV_MIP_PROFILE_HOME_ADDRESS = 14,   // MIP home address
    RIL_NV_MIP_PROFILE_AAA_AUTH = 15,       // AAA auth
    RIL_NV_MIP_PROFILE_HA_AUTH = 16,        // HA auth
    RIL_NV_MIP_PROFILE_PRI_HA_ADDR = 17,    // primary HA address
    RIL_NV_MIP_PROFILE_SEC_HA_ADDR = 18,    // secondary HA address
    RIL_NV_MIP_PROFILE_REV_TUN_PREF = 19,   // reverse TUN preference
    RIL_NV_MIP_PROFILE_HA_SPI = 20,         // HA SPI
    RIL_NV_MIP_PROFILE_AAA_SPI = 21,        // AAA SPI
    RIL_NV_MIP_PROFILE_MN_HA_SS = 22,       // HA shared secret
    RIL_NV_MIP_PROFILE_MN_AAA_SS = 23,      // AAA shared secret

    // CDMA network and band config
    RIL_NV_CDMA_BC10 = 24,                  // CDMA band class 10
    RIL_NV_CDMA_BC14 = 25,                  // CDMA band class 14
    RIL_NV_CDMA_SO68 = 26,                  // CDMA SO68
    RIL_NV_CDMA_SO73_COP0 = 27,             // CDMA SO73 COP0
    RIL_NV_CDMA_SO73_COP1TO7 = 28,          // CDMA SO73 COP1-7
    RIL_NV_CDMA_1X_ADVANCED_ENABLED = 29,   // CDMA 1X Advanced enabled
    RIL_NV_CDMA_EHRPD_ENABLED = 30,         // CDMA eHRPD enabled
    RIL_NV_CDMA_EHRPD_FORCED = 31,          // CDMA eHRPD forced

    // LTE network and band config
    RIL_NV_LTE_BAND_ENABLE_25 = 32,         // LTE band 25 enable
    RIL_NV_LTE_BAND_ENABLE_26 = 33,         // LTE band 26 enable
    RIL_NV_LTE_BAND_ENABLE_41 = 34,         // LTE band 41 enable

    RIL_NV_LTE_SCAN_PRIORITY_25 = 35,       // LTE band 25 scan priority
    RIL_NV_LTE_SCAN_PRIORITY_26 = 36,       // LTE band 26 scan priority
    RIL_NV_LTE_SCAN_PRIORITY_41 = 37,       // LTE band 41 scan priority

    RIL_NV_LTE_HIDDEN_BAND_PRIORITY_25 = 38,    // LTE hidden band 25 priority
    RIL_NV_LTE_HIDDEN_BAND_PRIORITY_26 = 39,    // LTE hidden band 26 priority
    RIL_NV_LTE_HIDDEN_BAND_PRIORITY_41 = 40,    // LTE hidden band 41 priority

} RIL_NV_Item;

#ifdef __cplusplus
}
#endif

#endif /* ANDROID_RIL_NV_ITEMS_H */

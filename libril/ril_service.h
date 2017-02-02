/*
 * Copyright (c) 2016 The Android Open Source Project
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

#ifndef RIL_SERVICE_H
#define RIL_SERVICE_H

#include <telephony/ril.h>
#include <ril_internal.h>

namespace radio {

void registerService(RIL_RadioFunctions *callbacks, android::CommandInfo *commands);
int getIccCardStatusResponse(android::Parcel &p, int slotId, int requestNumber, int responseType,
        int token, RIL_Errno e, void *response, size_t responselen);
int supplyIccPinForAppResponse(android::Parcel &p, int slotId, int requestNumber,
        int responseType, int serial, RIL_Errno e, void *response, size_t responselen);
int supplyIccPukForAppResponse(android::Parcel &p, int slotId, int requestNumber,
        int responseType, int serial, RIL_Errno e, void *response, size_t responselen);
int supplyIccPin2ForAppResponse(android::Parcel &p, int slotId, int requestNumber,
        int responseType, int serial, RIL_Errno e, void *response, size_t responselen);
int supplyIccPuk2ForAppResponse(android::Parcel &p, int slotId, int requestNumber,
        int responseType, int serial, RIL_Errno e, void *response, size_t responselen);
int changeIccPinForAppResponse(android::Parcel &p, int slotId, int requestNumber,
        int responseType, int serial, RIL_Errno e, void *response, size_t responselen);
int changeIccPin2ForAppResponse(android::Parcel &p, int slotId, int requestNumber,
        int responseType, int serial, RIL_Errno e, void *response, size_t responselen);
int supplyNetworkDepersonalizationResponse(android::Parcel &p, int slotId, int requestNumber,
        int responseType, int serial, RIL_Errno e, void *response, size_t responselen);
int getCurrentCallsResponse(android::Parcel &p, int slotId, int requestNumber,
        int responseType, int serial, RIL_Errno e, void *response, size_t responselen);
int dialResponse(android::Parcel &p, int slotId, int requestNumber,
        int responseType, int serial, RIL_Errno e, void *response, size_t responselen);
void acknowledgeRequest(int slotId, int serial);
void radioStateChanged(int slotId, int indicationType, RIL_RadioState radioState);
}   // namespace radio

#endif  // RIL_SERVICE_H
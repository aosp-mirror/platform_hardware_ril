/**
 * Copyright (C) 2010 The Android Open Source Project
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

#include <map>

#include <v8.h>
#include <telephony/ril.h>

#include "ril.pb.h"

#include "logging.h"
#include "mock_ril.h"
#include "node_buffer.h"
#include "node_object_wrap.h"
#include "protobuf_v8.h"
#include "status.h"
#include "util.h"
#include "worker.h"

#include "responses.h"

//#define RESPONSES_DEBUG
#ifdef  RESPONSES_DEBUG

#define DBG(...) LOGD(__VA_ARGS__)

#else

#define DBG(...)

#endif


/**
 * The Buffer is assumed to be empty so nothing to convert
 * @return STATUS_OK and *data = NULL *datalen = 0;
 */
RIL_Errno RspWithNoData(
        int cmd, RIL_Token token, RIL_Errno rilErrno, Buffer *buffer) {
    DBG("RspWithNoData E");

    // Complete the request
    s_rilenv->OnRequestComplete(token, rilErrno, NULL, 0);

    DBG("RspWithNoData X");
    return rilErrno;
}

/**
 * Handle response for an array of strings
 *
 * If a string value is "*magic-null*" then that value
 * will be returned as null.
 */
RIL_Errno RspStrings(
        int cmd, RIL_Token token, RIL_Errno rilErrno, Buffer *buffer) {
    DBG("RspStrings E");

    ril_proto::RspStrings *rsp = new ril_proto::RspStrings();
    rsp->ParseFromArray(buffer->data(), buffer->length());
    int result_len = rsp->strings_size() * sizeof(const char *);
    const char **result = (const char **)alloca(result_len);
    for (int i = 0; i < rsp->strings_size();  i++) {
        result[i] = rsp->strings(i).c_str();
        DBG("result[%d]='%s'", i, result[i]);
        if (strcmp("*magic-null*", result[i]) == 0) {
            result[i] = NULL;
        }
    }

    // Complete the request
    s_rilenv->OnRequestComplete(token, rilErrno, result, result_len);

    DBG("RspStrings X rilErrno=%d", rilErrno);
    return rilErrno;
}

/**
 * Handle response for a string
 */
RIL_Errno RspString(
        int cmd, RIL_Token token, RIL_Errno rilErrno, Buffer *buffer) {
    DBG("RspString E");

    ril_proto::RspStrings *rsp = new ril_proto::RspStrings();
    rsp->ParseFromArray(buffer->data(), buffer->length());
    const char *result = rsp->strings(0).c_str();

    // Complete the request
    s_rilenv->OnRequestComplete(token, rilErrno, (void *)result, strlen(result));

    DBG("RspString X rilErrno=%d", rilErrno);
    return rilErrno;
}

/**
 * Handle response for an array of integers
 */
RIL_Errno RspIntegers(
        int cmd, RIL_Token token, RIL_Errno rilErrno, Buffer *buffer) {
    DBG("RspIntegers E");

    ril_proto::RspIntegers *rsp = new ril_proto::RspIntegers();
    rsp->ParseFromArray(buffer->data(), buffer->length());
    int result_len = rsp->integers_size() * sizeof(const int32_t);
    int32_t *result = (int32_t *)alloca(result_len);
    for (int i = 0; i < rsp->integers_size();  i++) {
        result[i] = rsp->integers(i);
        DBG("result[%d]=%d", i, result[i]);
    }

    // Complete the request
    s_rilenv->OnRequestComplete(token, rilErrno, result, result_len);

    DBG("RspIntegers X rilErrno=%d", rilErrno);
    return rilErrno;
}

/**
 * Handle RIL_REQUEST_GET_SIM_STATUS response
 */
RIL_Errno RspGetSimStatus(
        int cmd, RIL_Token token, RIL_Errno rilErrno, Buffer *buffer) {
    DBG("RspGetSimStatus E");

    ril_proto::RspGetSimStatus *rsp = new ril_proto::RspGetSimStatus();
    rsp->ParseFromArray(buffer->data(), buffer->length());
    const ril_proto::RilCardStatus& r = rsp->card_status();
    RIL_CardStatus cardStatus;
    cardStatus.card_state = RIL_CardState(r.card_state());
    cardStatus.universal_pin_state = RIL_PinState(r.universal_pin_state());
    cardStatus.gsm_umts_subscription_app_index = r.gsm_umts_subscription_app_index();
    cardStatus.num_applications = r.num_applications();
    for (int i = 0; i < cardStatus.num_applications; i++) {
       cardStatus.applications[i].app_type = RIL_AppType(r.applications(i).app_type());
       cardStatus.applications[i].app_state = RIL_AppState(r.applications(i).app_state());
       cardStatus.applications[i].perso_substate =
            RIL_PersoSubstate(r.applications(i).perso_substate());
       cardStatus.applications[i].aid_ptr = const_cast<char *>(r.applications(i).aid().c_str());
       cardStatus.applications[i].app_label_ptr =
            const_cast<char *>(r.applications(i).app_label().c_str());
       cardStatus.applications[i].pin1_replaced = r.applications(i).pin1_replaced();
       cardStatus.applications[i].pin1 = RIL_PinState(r.applications(i).pin1());
       cardStatus.applications[i].pin2 = RIL_PinState(r.applications(i).pin2());
    }

    // Complete the request
    s_rilenv->OnRequestComplete(token, rilErrno,
            &cardStatus, sizeof(cardStatus));

    DBG("RspGetSimStatus X rilErrno=%d", rilErrno);
    return rilErrno;
}

/**
 * Handle RIL_REQUEST_ENTER_SIM_PIN_DATA response
 */
RIL_Errno RspEnterSimPinData(
        int cmd, RIL_Token token, RIL_Errno rilErrno, Buffer *buffer) {
    DBG("RspEnterSimPinData E");

    ril_proto::RspEnterSimPin *rsp = new ril_proto::RspEnterSimPin();
    rsp->ParseFromArray(buffer->data(), buffer->length());
    DBG("retries_remaining=%d", rsp->retries_remaining());
    int retries_remaining = rsp->retries_remaining();

    // Complete the request
    s_rilenv->OnRequestComplete(token, rilErrno,
            &retries_remaining, sizeof(retries_remaining));

    DBG("RspEnterSimPinData X rilErrno=%d", rilErrno);
    return rilErrno;
}

/**
 * Handle RIL_REQUEST_OPERATOR response
 */
RIL_Errno RspOperator(
        int cmd, RIL_Token token, RIL_Errno rilErrno, Buffer *buffer) {
    int status;

    DBG("RspOperator E");

    ril_proto::RspOperator *rsp = new ril_proto::RspOperator();
    rsp->ParseFromArray(buffer->data(), buffer->length());
    const char *result[3] = { NULL, NULL, NULL };
    if (rsp->has_long_alpha_ons()) {
        DBG("long_alpha_ons=%s", rsp->long_alpha_ons().c_str());
        result[0] = rsp->long_alpha_ons().c_str();
    }
    if (rsp->has_short_alpha_ons()) {
        DBG("short_alpha_ons=%s", rsp->short_alpha_ons().c_str());
        result[1] = rsp->short_alpha_ons().c_str();
    }
    if (rsp->has_mcc_mnc()) {
        DBG("mcc_mnc=%s", rsp->mcc_mnc().c_str());
        result[2] = rsp->mcc_mnc().c_str();
    }

    // Complete the request
    s_rilenv->OnRequestComplete(token, rilErrno, result, sizeof(result));

    DBG("RspOperator X rilErrno=%d", rilErrno);
    return rilErrno;
}

/**
 * Maps for converting request complete and unsoliciated response
 * protobufs to ril data arrays.
 */
typedef RIL_Errno (*RspConversion)(
                int cmd, RIL_Token token, RIL_Errno rilErrno, Buffer* protobuf);
typedef std::map<int, RspConversion> RspConversionMap;
RspConversionMap rilRspConversionMap;

typedef void (*UnsolRspConversion)(int cmd, Buffer* protobuf);
typedef std::map<int, UnsolRspConversion> UnsolRspConversionMap;
UnsolRspConversionMap unsolRilRspConversionMap;

/**
 * Send a ril request complete response.
 */
v8::Handle<v8::Value> SendRilRequestComplete(const v8::Arguments& args) {
    DBG("SendRilRequestComplete E");
    v8::HandleScope handle_scope;
    v8::Handle<v8::Value> retValue;

    int cmd;
    RIL_Errno rilErrno;
    RIL_Token token;
    Buffer* buffer;

    /**
     * Get the arguments. There should be at least 3, cmd,
     * ril error code and token. Optionally a Buffer containing
     * the protobuf representation of the data to return.
     */
    if (args.Length() < REQUEST_COMPLETE_REQUIRED_CMDS) {
        // Expecting a cmd, ERROR and token
        LOGE("SendRilRequestComplete X %d parameters"
             " expecting at least %d: rilErrno, cmd, and token",
                args.Length(), REQUEST_COMPLETE_REQUIRED_CMDS);
        return v8::Undefined();
    }
    v8::Handle<v8::Value> v8RilErrCode(
                    args[REQUEST_COMPLETE_RIL_ERR_CODE_INDEX]->ToObject());
    rilErrno = RIL_Errno(v8RilErrCode->NumberValue());

    v8::Handle<v8::Value> v8Cmd(
                    args[REQUEST_COMPLETE_CMD_INDEX]->ToObject());
    cmd = int(v8Cmd->NumberValue());

    v8::Handle<v8::Value> v8Token(
                    args[REQUEST_COMPLETE_TOKEN_INDEX]->ToObject());
    token = RIL_Token(int64_t(v8Token->NumberValue()));

    if (args.Length() >= (REQUEST_COMPLETE_DATA_INDEX+1)) {
        buffer = ObjectWrap::Unwrap<Buffer>(
                    args[REQUEST_COMPLETE_DATA_INDEX]->ToObject());
    } else {
        buffer = NULL;
    }

    RspConversionMap::iterator itr;
    itr = rilRspConversionMap.find(cmd);
    if (itr != rilRspConversionMap.end()) {
        itr->second(cmd, token, rilErrno, buffer);
    } else {
        if ((buffer == NULL) || (buffer->length() <= 0)) {
            // Nothing to convert
            rilErrno = RIL_E_SUCCESS;
        } else {
            // There was a buffer but we don't support the resonse yet.
            LOGE("SendRilRequestComplete: No conversion routine for cmd %d,"
                    " return RIL_E_REQUEST_NOT_SUPPORTED", cmd);
            rilErrno = RIL_E_REQUEST_NOT_SUPPORTED;
        }
        // Complete the request
        s_rilenv->OnRequestComplete(token, rilErrno, NULL, 0);
    }

    DBG("SendRilRequestComplete X rillErrno=%d", rilErrno);
    return v8::Undefined();
}

/**
 * Send an unsolicited response.
 */
v8::Handle<v8::Value> SendRilUnsolicitedResponse(const v8::Arguments& args) {
    DBG("SendRilUnsolicitedResponse E");
    v8::HandleScope handle_scope;
    v8::Handle<v8::Value> retValue;

    int status;
    void *data;
    size_t datalen;

    int cmd;
    Buffer* buffer;

    /**
     * Get the cmd number and data arguments
     */
    if (args.Length() < UNSOL_RESPONSE_REQUIRED_CMDS) {
        // Expecting a cmd
        LOGE("SendRilUnsolicitedResponse X %d parameters"
             " expecting at least a cmd",
                args.Length());
        return v8::Undefined();
    }
    v8::Handle<v8::Value> v8RilErrCode(args[UNSOL_RESPONSE_CMD_INDEX]->ToObject());
    cmd = int(v8RilErrCode->NumberValue());

    // data is optional
    if (args.Length() >= (UNSOL_RESPONSE_DATA_INDEX+1)) {
        buffer = ObjectWrap::Unwrap<Buffer>(args[UNSOL_RESPONSE_DATA_INDEX]->ToObject());
    } else {
        buffer = NULL;
    }

    UnsolRspConversionMap::iterator itr;
    itr = unsolRilRspConversionMap.find(cmd);
    if (itr != unsolRilRspConversionMap.end()) {
        itr->second(cmd, buffer);
    } else {
        if ((buffer == NULL) || (buffer->length() <= 0)) {
            // Nothing to convert
            data = NULL;
            datalen = 0;
        } else {
            // There was a buffer but we don't support the resonse yet.
            LOGE("SendRilUnsolicitedResponse: No conversion routine for cmd %d,"
                    " return RIL_E_REQUEST_NOT_SUPPORTED", cmd);
            data = NULL;
            datalen = 0;
        }
        s_rilenv->OnUnsolicitedResponse(cmd, NULL, 0);
    }

    DBG("SendRilUnsolicitedResponse X");
    return v8::Undefined();
}

int responsesInit(v8::Handle<v8::Context> context) {
    LOGD("responsesInit E");
    int status = STATUS_OK;

    rilRspConversionMap[RIL_REQUEST_GET_SIM_STATUS] = RspGetSimStatus; // 1
    rilRspConversionMap[RIL_REQUEST_ENTER_SIM_PIN] = RspEnterSimPinData; // 2
    rilRspConversionMap[RIL_REQUEST_HANGUP] = RspWithNoData; // 12
    rilRspConversionMap[RIL_REQUEST_REGISTRATION_STATE] = RspStrings; // 20
    rilRspConversionMap[RIL_REQUEST_GPRS_REGISTRATION_STATE] = RspStrings; // 21
    rilRspConversionMap[RIL_REQUEST_OPERATOR] = RspOperator; // 22
    rilRspConversionMap[RIL_REQUEST_GET_IMEI] = RspString; // 38
    rilRspConversionMap[RIL_REQUEST_GET_IMEISV] = RspString; // 39
    rilRspConversionMap[RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE] = RspIntegers; // 45
    rilRspConversionMap[RIL_REQUEST_BASEBAND_VERSION] = RspString; // 51
    rilRspConversionMap[RIL_REQUEST_SCREEN_STATE] = RspWithNoData; // 61

    LOGD("responsesInit X: status=%d", status);
    return STATUS_OK;
}

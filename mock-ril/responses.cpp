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
 * No buffer
 * @return STATUS_OK and *data = NULL *datalen = 0;
 */
int noBuffer(Buffer *buffer, void **data, size_t *datalen) {
    DBG("noBuffer E");

    if ((data == NULL) || (datalen == NULL)) {
        return STATUS_BAD_PARAMETER;
    }
    data = NULL;
    datalen = 0;

    DBG("noBuffer X");
    return STATUS_OK;
}

/**
 * Convert EnterSimPinData
 */
int cvrtBufferToRspEnterSimPinData(Buffer *buffer, void **data,
        size_t *datalen) {
    int status;

    DBG("cvrtBufferToRspEnterSimPinData E");
    if ((buffer == NULL) || (data == NULL) || (datalen == NULL)) {
        return STATUS_BAD_PARAMETER;
    }

    ril_proto::RspEnterSimPin *rsp = new ril_proto::RspEnterSimPin();
    rsp->ParseFromArray(buffer->data(), buffer->length());
    DBG("retries_remaining=%d", rsp->retries_remaining());
    int retries_remaining = rsp->retries_remaining();
    *data = (void *)&retries_remaining;
    *datalen = sizeof retries_remaining;
    status = STATUS_OK;

    DBG("cvrtBufferToRspEnterSimPinData X status=%d", status);
    return status;
}

/**
 * Maps for converting request complete and unsoliciated response
 * to a conversation routine.
 */
typedef int (*CvrtRilResponseBufferToData)(Buffer* protobuf, void **data, size_t *len);
typedef std::map<int, CvrtRilResponseBufferToData> CvrtRilResponseBufferToDataMap;

CvrtRilResponseBufferToDataMap rilResponseMap;
CvrtRilResponseBufferToDataMap rilUnsolicitedResponseMap;

/**
 * Send a ril request complete response.
 */
v8::Handle<v8::Value> SendRilRequestComplete(const v8::Arguments& args) {
    DBG("SendRilRequestComplete E");
    v8::HandleScope handle_scope;
    v8::Handle<v8::Value> retValue;

    void *data;
    size_t datalen;

    int cmd;
    RIL_Errno rilErrCode;
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
             " expecting at least %d: rilErrCode, cmd, and token",
                args.Length(), REQUEST_COMPLETE_REQUIRED_CMDS);
        return v8::Undefined();
    }
    v8::Handle<v8::Value> v8RilErrCode(
                    args[REQUEST_COMPLETE_RIL_ERR_CODE_INDEX]->ToObject());
    rilErrCode = RIL_Errno(v8RilErrCode->NumberValue());

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

    CvrtRilResponseBufferToDataMap::iterator itr;
    itr = rilResponseMap.find(cmd);
    if (itr != rilResponseMap.end()) {
        int status = itr->second(buffer, &data, &datalen);
        if (status == STATUS_OK) {
            rilErrCode = RIL_E_SUCCESS;
        } else {
            // TODO: Add mapping from status to rilErrCode
            rilErrCode = RIL_E_GENERIC_FAILURE;
        }
    } else {
        if ((buffer == NULL) || (buffer->length() <= 0)) {
            // Nothing to convert
            rilErrCode = RIL_E_SUCCESS;
            data = NULL;
            datalen = 0;
        } else {
            // There was a buffer but we don't support the resonse yet.
            LOGE("SendRilRequestComplete: No conversion routine for cmd %d,"
                    " return RIL_E_REQUEST_NOT_SUPPORTED", cmd);
            rilErrCode = RIL_E_REQUEST_NOT_SUPPORTED;
            data = NULL;
            datalen = 0;
        }
    }

    // Complete the request
    s_rilenv->OnRequestComplete(token, rilErrCode, data, datalen);

    DBG("SendRilRequestComplete X rilErrCode=%d", rilErrCode);
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

    CvrtRilResponseBufferToDataMap::iterator itr;
    itr = rilUnsolicitedResponseMap.find(cmd);
    if (itr != rilUnsolicitedResponseMap.end()) {
        status = itr->second(buffer, &data, &datalen);
    } else {
        if ((buffer == NULL) || (buffer->length() <= 0)) {
            // Nothing to convert
            status = STATUS_OK;
            data = NULL;
            datalen = 0;
        } else {
            // There was a buffer but we don't support the resonse yet.
            LOGE("SendRilUnsolicitedResponse: No conversion routine for cmd %d,"
                    " return RIL_E_REQUEST_NOT_SUPPORTED", cmd);
            status = STATUS_ERR;
            data = NULL;
            datalen = 0;
        }
    }

    if (status == STATUS_OK) {
        s_rilenv->OnUnsolicitedResponse(cmd, data, datalen);
    }

    DBG("SendRilUnsolicitedResponse X status=%d", status);
    return v8::Undefined();
}

int responsesInit(v8::Handle<v8::Context> context) {
    LOGD("responsesInit E");
    int status = STATUS_OK;

    rilResponseMap[RIL_REQUEST_ENTER_SIM_PIN] = cvrtBufferToRspEnterSimPinData;
    rilResponseMap[RIL_REQUEST_SCREEN_STATE] = noBuffer;

    LOGD("responsesInit X: status=%d", status);
    return STATUS_OK;
}

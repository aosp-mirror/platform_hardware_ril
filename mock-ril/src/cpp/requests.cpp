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
#include "ril.h"


#include "hardware/ril/mock-ril/src/proto/ril.pb.h"

#include "logging.h"
#include "js_support.h"
#include "mock_ril.h"
#include "node_buffer.h"
#include "node_object_wrap.h"
#include "node_util.h"
#include "protobuf_v8.h"
#include "status.h"
#include "util.h"
#include "worker.h"

#include "requests.h"

//#define REQUESTS_DEBUG
#ifdef  REQUESTS_DEBUG

#define DBG(...) LOGD(__VA_ARGS__)

#else

#define DBG(...)

#endif


/**
 * Request has no data so create an empty Buffer
 */
int ReqWithNoData(Buffer **pBuffer,
        const void *data, const size_t datalen, const RIL_Token t) {
    int status;
    static Buffer *emptyBuffer = Buffer::New(0L);

    DBG("ReqWithNoData E");
    *pBuffer = emptyBuffer;
    status = STATUS_OK;

    DBG("ReqWithNoData X status=%d", status);
    return status;
}

/**
 * request for RIL_REQUEST_ENTER_SIM_PIN  // 2
 */
int ReqEnterSimPin(Buffer **pBuffer,
        const void *data, const size_t datalen, const RIL_Token t) {
    int status;
    Buffer *buffer;

    DBG("ReqEnterSimPin E");
    if (datalen < sizeof(int)) {
        LOGE("ReqEnterSimPin: data to small err size < sizeof int");
        status = STATUS_BAD_DATA;
    } else {
        ril_proto::ReqEnterSimPin *req = new ril_proto::ReqEnterSimPin();
        DBG("ReqEnterSimPin: pin = %s", ((const char **)data)[0]);
        req->set_pin((((char **)data)[0]));
        buffer = Buffer::New(req->ByteSize());
        req->SerializeToArray(buffer->data(), buffer->length());
        delete req;
        *pBuffer = buffer;
        status = STATUS_OK;
    }
    DBG("ReqEnterSimPin X status=%d", status);
    return status;
}

/**
 * request for RIL_REQUEST_DIAL  // 10
 */
int ReqDial(Buffer **pBuffer,
            const void *data, const size_t datalen, const RIL_Token t) {
    int status;
    Buffer *buffer;

    DBG("ReqDial E");
    DBG("data=%p datalen=%d t=%p", data, datalen, t);

    if (datalen < sizeof(int)) {
        LOGE("ReqDial: data to small err size < sizeof int");
        status = STATUS_BAD_DATA;
    } else {
        ril_proto::ReqDial *req = new ril_proto::ReqDial();

        // cast the data to RIL_Dial
        RIL_Dial *rilDial = (RIL_Dial *)data;
        DBG("ReqDial: rilDial->address =%s, rilDial->clir=%d", rilDial->address, rilDial->clir);

        req->set_address(rilDial->address);
        req->set_clir(rilDial->clir);
        ril_proto::RilUusInfo *uusInfo = (ril_proto::RilUusInfo *)(&(req->uus_info()));

        if (rilDial->uusInfo != NULL) {
            DBG("ReqDial: print uusInfo:");
            DBG("rilDial->uusInfo->uusType = %d, "
                "rilDial->uusInfo->uusDcs =%d, "
                "rilDial->uusInfo->uusLength=%d, "
                "rilDial->uusInfo->uusData = %s",
                rilDial->uusInfo->uusType,
                rilDial->uusInfo->uusDcs,
                rilDial->uusInfo->uusLength,
                rilDial->uusInfo->uusData);

            uusInfo->set_uus_type((ril_proto::RilUusType)rilDial->uusInfo->uusType);
            uusInfo->set_uus_dcs((ril_proto::RilUusDcs)rilDial->uusInfo->uusDcs);
            uusInfo->set_uus_length(rilDial->uusInfo->uusLength);
            uusInfo->set_uus_data(rilDial->uusInfo->uusData);
        } else {
            DBG("uusInfo is NULL");
        }

        DBG("ReqDial: after set the request");
        DBG("req->ByetSize=%d", req->ByteSize());
        buffer = Buffer::New(req->ByteSize());
        DBG("buffer size=%d", buffer->length());

        req->SerializeToArray(buffer->data(), buffer->length());
        delete req;
        *pBuffer = buffer;
        status = STATUS_OK;
        DBG("ReqDial X, buffer->length()=%d", buffer->length());
    }
    DBG("ReqDial X status = %d", status);
    return status;
}

/**
 * request for RIL_REQUEST_HANGUP    // 12
 */
int ReqHangUp(Buffer **pBuffer,
        const void *data, const size_t datalen, const RIL_Token t) {
    int status;
    Buffer *buffer;

    DBG("ReqHangUp E");
    if (datalen < sizeof(int)) {
        LOGE("ReqHangUp: data to small err size < sizeof int");
        status = STATUS_BAD_DATA;
    } else {
        ril_proto::ReqHangUp *req = new ril_proto::ReqHangUp();
        DBG("ReqHangUp: connection_index=%d", ((int *)data)[0]);
        req->set_connection_index(((int *)data)[0]);
        buffer = Buffer::New(req->ByteSize());
        req->SerializeToArray(buffer->data(), buffer->length());
        delete req;
        *pBuffer = buffer;
        status = STATUS_OK;
    }
    DBG("ReqHangUp X status=%d", status);
    return status;
}

/**
 * request for RIL_REQUEST_SEPARATE_CONNECTION    // 52
 */
int ReqSeparateConnection (Buffer **pBuffer,
                           const void *data, const size_t datalen, const RIL_Token t) {
    int status;
    Buffer *buffer;
    v8::HandleScope handle_scope;

    DBG("ReqSeparateConnection E");
    if (datalen < sizeof(int)) {
        LOGE("ReqSetMute: data to small err size < sizeof int");
        status = STATUS_BAD_DATA;
    } else {
        ril_proto::ReqSeparateConnection *req = new ril_proto::ReqSeparateConnection();
        DBG("ReqSeparateConnection: index=%d", ((int *)data)[0]);
        req->set_index(((int *)data)[0]);
        DBG("ReqSeparateConnection: req->ByetSize=%d", req->ByteSize());
        buffer = Buffer::New(req->ByteSize());
        req->SerializeToArray(buffer->data(), buffer->length());
        delete req;
        *pBuffer = buffer;
        status = STATUS_OK;
    }
    DBG("ReqSeparateConnection X status=%d", status);
    return status;
}

/**
 * request for RIL_REQUEST_SET_MUTE      // 53
 */
int ReqSetMute(Buffer **pBuffer,
               const void *data, const size_t datalen, const RIL_Token t) {
    int status;
    Buffer *buffer;
    v8::HandleScope handle_scope;

    DBG("ReqSetMute E");
    if (datalen < sizeof(int)) {
        LOGE("ReqSetMute: data to small err size < sizeof int");
        status = STATUS_BAD_DATA;
    } else {
        ril_proto::ReqSetMute *req = new ril_proto::ReqSetMute();
        DBG("ReqSetMute: state=%d", ((int *)data)[0]);
        req->set_state(((int *)data)[0]);
        DBG("ReqSetMute: req->ByetSize=%d", req->ByteSize());
        buffer = Buffer::New(req->ByteSize());
        req->SerializeToArray(buffer->data(), buffer->length());
        delete req;
        *pBuffer = buffer;
        status = STATUS_OK;
    }
    DBG("ReqSetMute X status=%d", status);
    return status;
}

/**
 * request for RIL_REQUEST_SCREEN_STATE  // 61
 */
int ReqScreenState(Buffer **pBuffer,
        const void *data, const size_t datalen, const RIL_Token t) {
    int status;
    Buffer *buffer;
    v8::HandleScope handle_scope;

    DBG("ReqScreenState E data=%p datalen=%d t=%p",
         data, datalen, t);
    if (datalen < sizeof(int)) {
        LOGE("ReqScreenState: data to small err size < sizeof int");
        status = STATUS_BAD_DATA;
    } else {
        ril_proto::ReqScreenState *req = new ril_proto::ReqScreenState();
        DBG("ReqScreenState: state=%d", ((int *)data)[0]);
        req->set_state(((int *)data)[0]);
        DBG("ReqScreenState: req->ByteSize()=%d", req->ByteSize());
        buffer = Buffer::New(req->ByteSize());
        DBG("ReqScreenState: serialize");
        req->SerializeToArray(buffer->data(), buffer->length());
        delete req;
        *pBuffer = buffer;
        status = STATUS_OK;
    }
    DBG("ReqScreenState X status=%d", status);
    return status;
}

/**
 * Map from indexed by cmd and used to convert Data to Protobuf.
 */
typedef int (*ReqConversion)(Buffer** pBuffer, const void *data,
                const size_t datalen, const RIL_Token t);
typedef std::map<int, ReqConversion> ReqConversionMap;
ReqConversionMap rilReqConversionMap;

int callOnRilRequest(v8::Handle<v8::Context> context, int cmd,
                   const void *buffer, RIL_Token t) {
    DBG("callOnRilRequest E: cmd=%d", cmd);

    int status;
    v8::HandleScope handle_scope;
    v8::TryCatch try_catch;

    // Get the onRilRequest Function
    v8::Handle<v8::String> name = v8::String::New("onRilRequest");
    v8::Handle<v8::Value> onRilRequestFunctionValue = context->Global()->Get(name);
    v8::Handle<v8::Function> onRilRequestFunction =
        v8::Handle<v8::Function>::Cast(onRilRequestFunctionValue);

    // Create the cmd and token
    v8::Handle<v8::Value> v8RequestValue = v8::Number::New(cmd);
    v8::Handle<v8::Value> v8TokenValue = v8::Number::New(int64_t(t));

    // Invoke onRilRequest
    const int argc = 3;
    v8::Handle<v8::Value> argv[argc] = {
            v8RequestValue, v8TokenValue, ((Buffer *)buffer)->handle_ };
    v8::Handle<v8::Value> result =
        onRilRequestFunction->Call(context->Global(), argc, argv);
    if (try_catch.HasCaught()) {
        LOGE("callOnRilRequest error");
        ReportException(&try_catch);
        status = STATUS_ERR;
    } else {
        v8::String::Utf8Value result_string(result);
        DBG("callOnRilRequest result=%s", ToCString(result_string));
        status = STATUS_OK;
    }

    DBG("callOnRilRequest X: status=%d", status);
    return status;
}

RilRequestWorkerQueue::RilRequestWorkerQueue(v8::Handle<v8::Context> context) {
    DBG("RilRequestWorkerQueue E:");

    context_ = context;
    pthread_mutex_init(&free_list_mutex_, NULL);

    DBG("RilRequestWorkerQueue X:");
}

RilRequestWorkerQueue::~RilRequestWorkerQueue() {
    DBG("~RilRequestWorkerQueue E:");
    Request *req;
    pthread_mutex_lock(&free_list_mutex_);
    while(free_list_.size() != 0) {
        req = free_list_.front();
        delete req;
        free_list_.pop();
    }
    pthread_mutex_unlock(&free_list_mutex_);
    pthread_mutex_destroy(&free_list_mutex_);
    DBG("~RilRequestWorkerQueue X:");
}

/**
 * Add a request to the processing queue.
 * Data is serialized to a protobuf before adding to the queue.
 */
void RilRequestWorkerQueue::AddRequest (const int request,
        const void *data, const size_t datalen, const RIL_Token token) {
    DBG("RilRequestWorkerQueue:AddRequest: %d E", request);

    v8::Locker locker;
    v8::HandleScope handle_scope;
    v8::Context::Scope context_scope(context_);

    int status;

    // Convert the data to a protobuf before inserting it into the request queue (serialize data)
    Buffer *buffer = NULL;
    ReqConversionMap::iterator itr;
    itr = rilReqConversionMap.find(request);
    if (itr != rilReqConversionMap.end()) {
        status = itr->second(&buffer, data, datalen, token);
    } else {
        LOGE("RilRequestWorkerQueue:AddRequest: X unknown request %d", request);
        status = STATUS_UNSUPPORTED_REQUEST;
    }

    if (status == STATUS_OK) {
        // Add serialized request to the queue
        Request *req;
        pthread_mutex_lock(&free_list_mutex_);
        DBG("RilRequestWorkerQueue:AddRequest: return ok, buffer = %p, buffer->length()=%d",
            buffer, buffer->length());
        if (free_list_.size() == 0) {
            req = new Request(request, buffer, token);
            pthread_mutex_unlock(&free_list_mutex_);
        } else {
            req = free_list_.front();
            free_list_.pop();
            pthread_mutex_unlock(&free_list_mutex_);
            req->Set(request, buffer, token);
        }
        // add the request
        Add(req);
    } else {
        DBG("RilRequestWorkerQueue:AddRequest: return from the serialization, status is not OK");
        // An error report complete now
        RIL_Errno rilErrCode = (status == STATUS_UNSUPPORTED_REQUEST) ?
                 RIL_E_REQUEST_NOT_SUPPORTED : RIL_E_GENERIC_FAILURE;
        s_rilenv->OnRequestComplete(token, rilErrCode, NULL, 0);
    }

    DBG("RilRequestWorkerQueue::AddRequest: X"
         " request=%d data=%p datalen=%d token=%p",
            request, data, datalen, token);
}

void RilRequestWorkerQueue::Process(void *p) {

    Request *req = (Request *)p;
    DBG("RilRequestWorkerQueue::Process: E"
         " request=%d buffer=%p, bufferlen=%d t=%p",
            req->request_, req->buffer_, req->buffer_->length(), req->token_);

    v8::Locker locker;
    v8::HandleScope handle_scope;
    v8::Context::Scope context_scope(context_);
    callOnRilRequest(context_, req->request_,
                          req->buffer_, req->token_);

    pthread_mutex_lock(&free_list_mutex_);
    free_list_.push(req);
    pthread_mutex_unlock(&free_list_mutex_);
}

int requestsInit(v8::Handle<v8::Context> context, RilRequestWorkerQueue **rwq) {
    LOGD("requestsInit E");

    rilReqConversionMap[RIL_REQUEST_GET_SIM_STATUS] = ReqWithNoData; // 1
    rilReqConversionMap[RIL_REQUEST_ENTER_SIM_PIN] = ReqEnterSimPin; // 2
    rilReqConversionMap[RIL_REQUEST_GET_CURRENT_CALLS] = ReqWithNoData; // 9
    rilReqConversionMap[RIL_REQUEST_DIAL] = ReqDial;   // 10
    rilReqConversionMap[RIL_REQUEST_GET_IMSI] = ReqWithNoData; // 11
    rilReqConversionMap[RIL_REQUEST_HANGUP] = ReqHangUp; // 12
    rilReqConversionMap[RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND] = ReqWithNoData; // 13
    rilReqConversionMap[RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND] = ReqWithNoData; // 14
    rilReqConversionMap[RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE] = ReqWithNoData; // 15
    rilReqConversionMap[RIL_REQUEST_CONFERENCE] = ReqWithNoData;  // 16
    rilReqConversionMap[RIL_REQUEST_LAST_CALL_FAIL_CAUSE] = ReqWithNoData;  // 18
    rilReqConversionMap[RIL_REQUEST_SIGNAL_STRENGTH] = ReqWithNoData; // 19
    rilReqConversionMap[RIL_REQUEST_VOICE_REGISTRATION_STATE] = ReqWithNoData; // 20
    rilReqConversionMap[RIL_REQUEST_DATA_REGISTRATION_STATE] = ReqWithNoData; // 21
    rilReqConversionMap[RIL_REQUEST_OPERATOR] = ReqWithNoData; // 22
    rilReqConversionMap[RIL_REQUEST_GET_IMEI] = ReqWithNoData; // 38
    rilReqConversionMap[RIL_REQUEST_GET_IMEISV] = ReqWithNoData; // 39
    rilReqConversionMap[RIL_REQUEST_ANSWER] = ReqWithNoData; // 40
    rilReqConversionMap[RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE] = ReqWithNoData; // 45
    rilReqConversionMap[RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC] = ReqWithNoData; // 46
    rilReqConversionMap[RIL_REQUEST_BASEBAND_VERSION] = ReqWithNoData; // 51
    rilReqConversionMap[RIL_REQUEST_SEPARATE_CONNECTION] = ReqSeparateConnection; // 52
    rilReqConversionMap[RIL_REQUEST_SET_MUTE] = ReqSetMute; // 53
    rilReqConversionMap[RIL_REQUEST_SCREEN_STATE] = ReqScreenState; // 61

    *rwq = new RilRequestWorkerQueue(context);
    int status = (*rwq)->Run();

    LOGD("requestsInit X: status=%d", status);
    return status;
}

/**
 * Subroutine to test a single RIL request
 */
void testRilRequest(v8::Handle<v8::Context> context, int request, const void *data,
                    const size_t datalen, const RIL_Token t) {
    Buffer *buffer = NULL;
    ReqConversionMap::iterator itr;
    int status;

    LOGD("testRilRequest: request=%d", request);

    itr = rilReqConversionMap.find(request);
    if (itr != rilReqConversionMap.end()) {
        status = itr->second(&buffer, data, sizeof(data), (void *)0x12345677);
    } else {
        LOGE("testRequests X unknown request %d", request);
        status = STATUS_UNSUPPORTED_REQUEST;
    }
    if (status == STATUS_OK) {
        callOnRilRequest(context, request, buffer, (void *)0x12345677);
    } else {
        LOGE("testRilRequest X, serialize error");
    }
}

void testRequests(v8::Handle<v8::Context> context) {
    LOGD("testRequests E: ********");

    v8::TryCatch try_catch;

    char *buffer;
    const char *fileName= "/sdcard/data/mock_ril.js";
    int status = ReadFile(fileName, &buffer);
    if (status == 0) {
        runJs(context, &try_catch, fileName, buffer);
        Buffer *buffer = NULL;
        ReqConversionMap::iterator itr;
        int status;
        int request;

        if (!try_catch.HasCaught()) {
            {
                const int data[1] = { 1 };
                testRilRequest(context, RIL_REQUEST_SIGNAL_STRENGTH, data, sizeof(data),
                               (void *)0x12345677);
            }
            {
                const char *data[1] = { "winks-pin" };
                testRilRequest(context, RIL_REQUEST_ENTER_SIM_PIN, data, sizeof(data),
                               (void *)0x12345677);
            }
            {
                const int data[1] = { 1 };
                testRilRequest(context, RIL_REQUEST_HANGUP, data, sizeof(data),
                               (void *)0x12345677);
            }
            {
                const int data[1] = { 1 };
                testRilRequest(context, RIL_REQUEST_SCREEN_STATE, data, sizeof(data),
                               (void *)0x12345677);
            }
            {
                const int data[1] = { 1 };
                testRilRequest(context, RIL_REQUEST_GET_SIM_STATUS, data, sizeof(data),
                               (void *)0x12345677);
            }
            {
                RilRequestWorkerQueue *rwq = new RilRequestWorkerQueue(context);
                if (rwq->Run() == STATUS_OK) {
                    const int data[1] = { 1 };
                    rwq->AddRequest(RIL_REQUEST_SCREEN_STATE,
                                    data, sizeof(data), (void *)0x1234567A);
                    rwq->AddRequest(RIL_REQUEST_SIGNAL_STRENGTH,
                                    data, sizeof(data), (void *)0x1234567A);
                    // Sleep to let it be processed
                    v8::Unlocker unlocker;
                    sleep(3);
                    v8::Locker locker;
                }
                delete rwq;
            }
        }
    }

    LOGD("testRequests X: ********\n");
}

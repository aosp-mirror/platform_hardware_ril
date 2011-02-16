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

#include <v8.h>
#include "ril.h"

#include "logging.h"
#include "status.h"
#include "worker.h"
#include "util.h"

#include "hardware/ril/mock-ril/src/proto/ril.pb.h"

#include "logging.h"
#include "js_support.h"
#include "node_buffer.h"
#include "node_util.h"
#include "protobuf_v8.h"
#include "requests.h"

#include "experiments.h"

void testStlPort() {
    // Test using STLport
    std::queue<int *> q;
    int data[] = {1, 2, 3};

    int *param = data;
    LOGD("before push q.size=%d", q.size());
    q.push(param);
    LOGD("after push q.size=%d", q.size());
    void *p = q.front();
    if (p == param) {
        LOGD("q.push succeeded");
    } else {
        LOGD("q.push failed");
    }
    q.pop();
    LOGD("after pop q.size=%d", q.size());
}

v8::Handle<v8::Value> GetReqScreenState(v8::Local<v8::String> property,
                               const v8::AccessorInfo &info) {
    v8::Local<v8::Object> self = info.Holder();
    v8::Local<v8::External> wrap =
            v8::Local<v8::External>::Cast(self->GetInternalField(0));
    void *p = wrap->Value();
    int state = static_cast<int *>(p)[0];
    LOGD("GetReqScreenState state=%d", state);
    return v8::Integer::New(state);
}

bool callOnRilRequest(v8::Handle<v8::Context> context, int request,
                   void *data, size_t datalen, RIL_Token t) {
    v8::HandleScope handle_scope;
    v8::TryCatch try_catch;

    // Get the onRilRequestFunction, making sure its a function
    v8::Handle<v8::String> name = v8::String::New("onRilRequest");
    v8::Handle<v8::Value> onRilRequestFunctionValue = context->Global()->Get(name);
    if(!onRilRequestFunctionValue->IsFunction()) {
        // Wasn't a function
        LOGD("callOnRilRequest X wasn't a function");
        return false;
    }
    v8::Handle<v8::Function> onRilRequestFunction =
        v8::Handle<v8::Function>::Cast(onRilRequestFunctionValue);

    // Create the request
    v8::Handle<v8::Value> v8RequestValue = v8::Number::New(request);

    // Create the parameter for the request
    v8::Handle<v8::Object> params_obj =
            v8::ObjectTemplate::New()->NewInstance();
    switch(request) {
        case(RIL_REQUEST_SCREEN_STATE): {
            LOGD("callOnRilRequest RIL_REQUEST_SCREEN_STATE");
            if (datalen < sizeof(int)) {
                LOGD("callOnRilRequest err size < sizeof int");
            } else {
                v8::Handle<v8::ObjectTemplate> params_obj_template =
                        v8::ObjectTemplate::New();
                params_obj_template->SetInternalFieldCount(1);
                params_obj_template->SetAccessor(v8::String::New(
                            "ReqScreenState"), GetReqScreenState, NULL);
                // How to not leak this pointer!!!
                int *p = new int;
                *p = ((int *)data)[0];
                params_obj = params_obj_template->NewInstance();
                params_obj->SetInternalField(0, v8::External::New(p));
            }
            break;
        }
        default: {
            LOGD("callOnRilRequest X unknown request");
            break;
        }
    }

    // Invoke onRilRequest
    bool retValue;
    const int argc = 2;
    v8::Handle<v8::Value> argv[argc] = { v8RequestValue, params_obj };
    v8::Handle<v8::Value> result =
        onRilRequestFunction->Call(context->Global(), argc, argv);
    if (try_catch.HasCaught()) {
        LOGD("callOnRilRequest error");
        ReportException(&try_catch);
        retValue = false;
    } else {
        v8::String::Utf8Value result_string(result);
        LOGD("callOnRilRequest result=%s", ToCString(result_string));
        retValue = true;
    }
    return retValue;
}

void testOnRilRequestUsingCppRequestObjs(v8::Handle<v8::Context> context) {
    LOGD("testOnRilRequestUsingCppRequestObjs E:");
    v8::HandleScope handle_scope;

    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    runJs(context, &try_catch, "local-string",
        "function onRilRequest(reqNum, params) {\n"
        "  print(\"reqNum=\" + reqNum);\n"
        "  if (reqNum == 61) {\n"
        "      print(\"params.ReqScreenState=\" + params.ReqScreenState);\n"
        "  }\n"
        "  return \"Hello World\";\n"
        "}\n");
    if (!try_catch.HasCaught()) {
        // Call the onRilRequest function
        int data[1] = { 0 };
        callOnRilRequest(context, RIL_REQUEST_SCREEN_STATE, data,
                sizeof(data), NULL);
    }
    LOGD("testOnRilRequestUsingCppRequestObjs X:");
}

void testReqScreenStateProtobuf() {
    v8::HandleScope handle_scope;
    v8::TryCatch try_catch;

    LOGD("testReqScreenStateProtobuf E");

    LOGD("create ReqScreenState");
    ril_proto::ReqScreenState* ss = new ril_proto::ReqScreenState();
    ss->set_state(true);
    bool state = ss->state();
    LOGD("state=%d", state);
    ss->set_state(false);
    state = ss->state();
    LOGD("state=%d", state);
    int len = ss->ByteSize();
    LOGD("create buffer len=%d", len);
    char *buffer = new char[len];
    LOGD("serialize");
    bool ok = ss->SerializeToArray(buffer, len);
    if (!ok) {
        LOGD("testReqScreenStateProtobuf X: Could not serialize ss");
        return;
    }
    LOGD("ReqScreenState serialized ok");
    ril_proto::ReqScreenState *newSs = new ril_proto::ReqScreenState();
    ok = newSs->ParseFromArray(buffer, len);
    if (!ok) {
        LOGD("testReqScreenStateProtobuf X: Could not deserialize ss");
        return;
    }
    LOGD("newSs->state=%d", newSs->state());

    delete [] buffer;
    delete ss;
    delete newSs;
    LOGD("testReqScreenStateProtobuf X");
}

void testReqHangUpProtobuf() {
    v8::HandleScope handle_scope;
    v8::TryCatch try_catch;

    LOGD("testReqHangUpProtobuf E");

    LOGD("create ReqHangUp");
    ril_proto::ReqHangUp* hu = new ril_proto::ReqHangUp();
    hu->set_connection_index(3);
    bool connection_index = hu->connection_index();
    LOGD("connection_index=%d", connection_index);
    hu->set_connection_index(2);
    connection_index = hu->connection_index();
    LOGD("connection_index=%d", connection_index);
    LOGD("create buffer");
    int len = hu->ByteSize();
    char *buffer = new char[len];
    LOGD("serialize");
    bool ok = hu->SerializeToArray(buffer, len);
    if (!ok) {
        LOGD("testReqHangUpProtobuf X: Could not serialize hu");
        return;
    }
    LOGD("ReqHangUp serialized ok");
    ril_proto::ReqHangUp *newHu = new ril_proto::ReqHangUp();
    ok = newHu->ParseFromArray(buffer, len);
    if (!ok) {
        LOGD("testReqHangUpProtobuf X: Could not deserialize hu");
        return;
    }
    LOGD("newHu->connection_index=%d", newHu->connection_index());

    delete [] buffer;
    delete hu;
    delete newHu;
    LOGD("testReqHangUpProtobuf X");
}

void testProtobufV8(v8::Handle<v8::Context> context) {
    LOGD("testProtobufV8 E:");
    v8::HandleScope handle_scope;

    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    if (try_catch.HasCaught()) {
        LOGD("TryCatch.hasCaught is true after protobuf_v8::init");
        ReportException(&try_catch);
    }
    runJs(context, &try_catch, "local-string",
        "fileContents = readFileToString('mock_ril.js');\n"
        "print('fileContents:\\n' + fileContents);\n"
        "\n"
        "buffer = readFileToBuffer('ril.desc');\n"
        "var schema = new Schema(buffer);\n"
        "\n"
        "var originalReqEnterSimPin = { pin : 'hello-the-pin' };\n"
        "print('originalReqEnterSimPin: pin=' + originalReqEnterSimPin.pin);\n"
        "var ReqEnterSimPinSchema = schema['ril_proto.ReqEnterSimPin'];\n"
        "serializedOriginalReqEnterSimPin = ReqEnterSimPinSchema.serialize(originalReqEnterSimPin);\n"
        "print('serializedOriginalReqEnterSimPin.length=' + serializedOriginalReqEnterSimPin.length);\n"
        "newReqEnterSimPin = ReqEnterSimPinSchema.parse(serializedOriginalReqEnterSimPin);\n"
        "print('newReqEnterSimPin: pin=' + newReqEnterSimPin.pin);\n"
        "\n"
        "var originalReqScreenState = { state : true };\n"
        "print('originalReqScreenState: state=' + originalReqScreenState.state);\n"
        "var ReqScreenStateSchema = schema['ril_proto.ReqScreenState'];\n"
        "var serializedOriginalReqScreenState = ReqScreenStateSchema.serialize(originalReqScreenState);\n"
        "print('serializedOriginalReqScreenState.length=' + serializedOriginalReqScreenState.length);\n"
        "var newReqScreenState = ReqScreenStateSchema.parse(serializedOriginalReqScreenState);\n"
        "print('newReqScreenState: state=' + newReqScreenState.state);\n"
        "\n"
        "originalReqScreenState.state = false;\n"
        "print('originalReqScreenState: state=' + originalReqScreenState.state);\n"
        "serializedOriginalReqScreenState = ReqScreenStateSchema.serialize(originalReqScreenState);\n"
        "print('serializedOriginalReqScreenState.length=' + serializedOriginalReqScreenState.length);\n"
        "newReqScreenState = ReqScreenStateSchema.parse(serializedOriginalReqScreenState);\n"
        "print('newReqScreenState: state=' + newReqScreenState.state);\n");
    LOGD("testProtobufV8 X");
}

void experiments(v8::Handle<v8::Context> context) {
    LOGD("experiments E: ********");
    testStlPort();
    testReqScreenStateProtobuf();
    testOnRilRequestUsingCppRequestObjs(context);
    testProtobufV8(context);
    LOGD("experiments X: ********\n");
}

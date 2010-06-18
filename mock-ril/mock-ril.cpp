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

// The LOG_TAG should start with "RIL" so it shows up in the  radio log
#define LOG_TAG "RIL-MOCK"

#include <telephony/ril.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <alloca.h>
#include <getopt.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <termios.h>

#include <queue>

#include <utils/Log.h>

#include "t.pb.h"
#include <v8.h>

#define MOCK_RIL_VER_STRING "Android Mock-ril 0.1"

/**
 * Forward declarations
 */
static void onRequest (int request, void *data, size_t datalen, RIL_Token t);
static RIL_RadioState currentState();
static int onSupports (int requestCode);
static void onCancel (RIL_Token t);
static const char *getVersion();

extern const char * requestToString(int request);

/*** Static Variables ***/
static const RIL_RadioFunctions s_callbacks = {
    RIL_VERSION,
    onRequest,
    currentState,
    onSupports,
    onCancel,
    getVersion
};

static const struct RIL_Env *s_rilenv;

static RIL_RadioState sState = RADIO_STATE_UNAVAILABLE;

/**
 * Call from RIL to us to make a RIL_REQUEST
 *
 * Must be completed with a call to RIL_onRequestComplete()
 *
 * RIL_onRequestComplete() may be called from any thread, before or after
 * this function returns.
 * * Will always be called from the same thread, so returning here implies
 * that the radio is ready to process another command (whether or not
 * the previous command has completed).
 */
static void onRequest (int request, void *data, size_t datalen, RIL_Token t)
{
    LOGD("onRequest: request=%d data=%p datalen=%d token=%p",
            request, data, datalen, t);
    s_rilenv->OnRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
}

/**
 * Synchronous call from the RIL to us to return current radio state.
 * RADIO_STATE_UNAVAILABLE should be the initial state.
 */
static RIL_RadioState currentState()
{
    LOGD("currentState: sState=%d", sState);
    return sState;
}

/**
 * Call from RIL to us to find out whether a specific request code
 * is supported by this implementation.
 *
 * Return 1 for "supported" and 0 for "unsupported"
 */

static int
onSupports (int requestCode)
{
    LOGD("onSupports: nothing supported at the moment, return 0");
    return 0;
}

static void onCancel (RIL_Token t)
{
    LOGD("onCancel: ignorning");
}

static const char * getVersion(void)
{
    LOGD("getVersion: return '%s'", MOCK_RIL_VER_STRING);
    return MOCK_RIL_VER_STRING;
}


// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}

// A javascript print function
v8::Handle<v8::Value> Print(const v8::Arguments& args) {
    bool first = true;
    const int str_size = 1000;
    char* str = new char[str_size];
    int offset = 0;
    for (int i = 0; i < args.Length(); i++) {
        v8::HandleScope handle_scope;
        if (first) {
            first = false;
        } else {
            offset += snprintf(&str[offset], str_size, " ");
        }
        v8::String::Utf8Value strUtf8(args[i]);
        const char* cstr = ToCString(strUtf8);
        offset += snprintf(&str[offset], str_size, "%s", cstr);
    }
    LOGD("%s", str);
    delete [] str;
    return v8::Undefined();
}

// Report an exception
void LogErrorMessage(v8::Handle<v8::Message> message,
         const char *alternate_message) {
  v8::HandleScope handle_scope;
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    LOGD("%s", alternate_message);
  } else {
    v8::String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber();
    LOGD("file:%s line:%i", filename_string, linenum);

    // Print line of source code.
    v8::String::Utf8Value sourceline(message->GetSourceLine());
    const char* sourceline_string = ToCString(sourceline);
    LOGD("%s", sourceline_string);

    // Print location information under source line
    int start = message->GetStartColumn();
    int end = message->GetEndColumn();
    char *error_string = new char[end+1];
    memset(error_string, ' ', start);
    memset(&error_string[start], '^', end - start);
    error_string[end] = 0;
    LOGD("%s", error_string);
    delete [] error_string;
  }
}

void ErrorCallback(v8::Handle<v8::Message> message,
        v8::Handle<v8::Value> data) {
    LogErrorMessage(message, "");
}

// Report an exception
void ReportException(v8::TryCatch* try_catch) {
    v8::HandleScope handle_scope;

    v8::String::Utf8Value exception(try_catch->Exception());
    v8::Handle<v8::Message> msg = try_catch->Message();
    if (msg.IsEmpty()) {
       // Why is try_catch->Message empty?
       // is always empty on compile errors
    }
    LogErrorMessage(msg, ToCString(exception));
}

v8::Handle<v8::Value> GetScreenState(v8::Local<v8::String> property,
                               const v8::AccessorInfo &info) {
    v8::Local<v8::Object> self = info.Holder();
    v8::Local<v8::External> wrap =
            v8::Local<v8::External>::Cast(self->GetInternalField(0));
    void *p = wrap->Value();
    int state = static_cast<int *>(p)[0];
    LOGD("GetScreenState state=%d", state);
    return v8::Integer::New(state);
}

bool callOnRequest(v8::Handle<v8::Context> context, int request,
                   void *data, size_t datalen, RIL_Token t) {
    v8::HandleScope handle_scope;
    v8::TryCatch try_catch;

    // Get the onRequestFunction, making sure its a function
    v8::Handle<v8::String> name = v8::String::New("onRequest");
    v8::Handle<v8::Value> onRequestFunctionValue = context->Global()->Get(name);
    if(!onRequestFunctionValue->IsFunction()) {
        // Wasn't a function
        LOGD("callOnRequest X wasn't a function");
        return false;
    }
    v8::Handle<v8::Function> onRequestFunction =
        v8::Handle<v8::Function>::Cast(onRequestFunctionValue);

    // Create the request
    v8::Handle<v8::Value> v8RequestValue = v8::Number::New(request);

    // Create the parameter for the request
    v8::Handle<v8::Object> params_obj =
            v8::ObjectTemplate::New()->NewInstance();
    switch(request) {
        case(RIL_REQUEST_SCREEN_STATE): {
            LOGD("callOnRequest RIL_REQUEST_SCREEN_STATE");
            if (datalen < sizeof(int)) {
                LOGD("callOnRequest err size < sizeof int");
            } else {
                v8::Handle<v8::ObjectTemplate> params_obj_template =
                        v8::ObjectTemplate::New();
                params_obj_template->SetInternalFieldCount(1);
                params_obj_template->SetAccessor(v8::String::New("ScreenState"),
                                                 GetScreenState, NULL);
                // How to not leak this pointer!!!
                int *p = new int;
                *p = ((int *)data)[0];
                params_obj = params_obj_template->NewInstance();
                params_obj->SetInternalField(0, v8::External::New(p));
            }
            break;
        }
        default: {
            LOGD("callOnRequest X unknown request");
            break;
        }
    }

    // Invoke onRequest
    bool retValue;
    const int argc = 2;
    v8::Handle<v8::Value> argv[argc] = { v8RequestValue, params_obj };
    v8::Handle<v8::Value> result =
        onRequestFunction->Call(context->Global(), argc, argv);
    if (try_catch.HasCaught()) {
        ReportException(&try_catch);
        retValue = false;
    } else {
        v8::String::Utf8Value result_string(result);
        LOGD("callOnRequest result=%s", ToCString(result_string));
        retValue = true;
    }
    return retValue;
}

// There appears to be a bug which causes TryCatch.HasCaught()
// to return true after creating a String Value.
void v8Bug1() {
    v8::HandleScope handle_scope;
    v8::TryCatch try_catch;

    LOGD("v8Bug1: start, HasCaught()=%d", try_catch.HasCaught());

    v8::Handle<v8::Value> mydata(v8::String::New("hello"));
    LOGD("v8Bug1 after creating mydata, HasCaught()=%d", try_catch.HasCaught());
    if (try_catch.HasCaught()) {
        LOGD("v8Bug1: Why is HasCaught true");
    }
}

void testV8() {
    LOGD("testV8 E:");
    v8::HandleScope handle_scope;

    // Add a Message listner to Catch errors as they occur
    v8::Handle<v8::Value> mydata(v8::String::New("hello"));
    v8::V8::AddMessageListener(ErrorCallback, mydata);

    v8::TryCatch try_catch;

    // Create a template for the global object and
    // add the function template for print to it.
    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
    global->Set(v8::String::New("print"), v8::FunctionTemplate::New(Print));

    // Create context with our globals and make it the current scope
    v8::Handle<v8::Context> context = v8::Context::New(NULL, global);
    v8::Context::Scope context_scope(context);

    // Compile the source
    v8::Handle<v8::String> source = v8::String::New(
        "function onRequest(reqNum, params) {\n"
        "  print(\"reqNum=\" + reqNum);\n"
        "  if (reqNum == 61) {\n"
        "      print(\"params.ScreenState=\" + params.ScreenState);\n"
        "  }\n"
        "  return \"Hello World\";\n"
        "}\n");
    v8::Handle<v8::Script> script = v8::Script::Compile(source);
    if (script.IsEmpty()) {
        LOGD("testV8 compile source failed");
        ReportException(&try_catch);
    } else {
        // Run the resulting script
        v8::Handle<v8::Value> result = script->Run();
        if (try_catch.HasCaught()) {
            LOGD("testV8 run script failed");
            ReportException(&try_catch);
        } else {
            // Call the onRequest function
            int data[1] = { 0 };
            callOnRequest(context, RIL_REQUEST_SCREEN_STATE, data,
                    sizeof(data), NULL);
        }
    }
    LOGD("testV8 X:");
}

pthread_t s_tid_mainloop;
static void * mainLoop(void *param)
{
    // Test using STLport
    std::queue<void *> q;

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

    // Test a simple protobuf
    LOGD("create T a simple protobuf");
    T* t = new T();
    LOGD("set_id(1)");
    t->set_id(1);
    int id = t->id();
    LOGD("id=%d", id);
    delete t;

    LOGD("mainLoop E");

    for (int i=0;;i++) {
        sleep(10);
        LOGD("mainLoop: looping %d", i);
    }

    LOGD("mainLoop X");

    return NULL;
}

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc,
        char **argv) {
    int ret;
    pthread_attr_t attr;

    LOGD("RIL_Init E");

    v8Bug1();
    testV8();

    s_rilenv = env;

    ret = pthread_attr_init (&attr);
    if (ret != 0) {
        LOGE("RIL_Init X: pthread_attr_init failed err=%s", strerror(ret));
        return NULL;
    }
    ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (ret != 0) {
        LOGE("RIL_Init X: pthread_attr_setdetachstate failed err=%s",
                strerror(ret));
        return NULL;
    }
    ret = pthread_create(&s_tid_mainloop, &attr, mainLoop, NULL);
    if (ret != 0) {
        LOGE("RIL_Init X: pthread_create failed err=%s", strerror(ret));
        return NULL;
    }

    LOGD("RIL_Init X");
    return &s_callbacks;
}

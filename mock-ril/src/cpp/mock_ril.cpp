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

#include <v8.h>
#include "ril.h"

#include "hardware/ril/mock-ril/src/proto/ril.pb.h"

#include "ctrl_server.h"
#include "logging.h"
#include "experiments.h"
#include "js_support.h"
#include "node_buffer.h"
#include "node_object_wrap.h"
#include "node_util.h"
#include "protobuf_v8.h"
#include "requests.h"
#include "responses.h"
#include "status.h"
#include "util.h"
#include "worker.h"
#include "worker_v8.h"

#include "mock_ril.h"

extern "C" {
// Needed so we can call it prior to calling startMockRil
extern void RIL_register(const RIL_RadioFunctions *callbacks);
}

//#define MOCK_RIL_DEBUG
#ifdef  MOCK_RIL_DEBUG

#define DBG(...) LOGD(__VA_ARGS__)

#else

#define DBG(...)

#endif


#define MOCK_RIL_VER_STRING "Android Mock-ril 0.1"

/**
 * Forward declarations
 */
static void onRequest (int request, void *data, size_t datalen, RIL_Token t);
static RIL_RadioState currentState();
static int onSupports (int requestCode);
static void onCancel (RIL_Token t);
static const char *getVersion();

static void testOnRequestComplete(RIL_Token t, RIL_Errno e,
                       void *response, size_t responselen);
static void testRequestTimedCallback(RIL_TimedCallback callback,
                void *param, const struct timeval *relativeTime);
static void testOnUnsolicitedResponse(int unsolResponse, const void *data,
                                size_t datalen);

/**
 * The environment from rild with the completion routine
 */
const struct RIL_Env *s_rilenv;

/**
 * Expose our routines to rild
 */
static const RIL_RadioFunctions s_callbacks = {
    RIL_VERSION,
    onRequest,
    currentState,
    onSupports,
    onCancel,
    getVersion
};

/**
 * A test environment
 */
static const RIL_Env testEnv = {
    testOnRequestComplete,
    testOnUnsolicitedResponse,
    testRequestTimedCallback
};

/**
 * The request worker queue to handle requests
 */
static RilRequestWorkerQueue *s_requestWorkerQueue;

/**
 * Call from RIL to us to make a RIL_REQUEST
 *
 * Must be completed with a call to RIL_onRequestComplete()
 *
 * RIL_onRequestComplete() may be called from any thread, before or after
 * this function returns.
 *
 * Will always be called from the same thread, so returning here implies
 * that the radio is ready to process another command (whether or not
 * the previous command has c1mpleted).
 */
static void onRequest (int request, void *data, size_t datalen, RIL_Token t)
{
    DBG("onRequest: request=%d data=%p datalen=%d token=%p",
            request, data, datalen, t);
    s_requestWorkerQueue->AddRequest(request, data, datalen, t);
}

/**
 * Synchronous call from the RIL to us to return current radio state.
 * RADIO_STATE_UNAVAILABLE should be the initial state.
 */
static RIL_RadioState currentState()
{
    DBG("currentState: gRadioState=%d", gRadioState);
    return gRadioState;
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
    DBG("onSupports: nothing supported at the moment, return 0");
    return 0;
}

static void onCancel (RIL_Token t)
{
    DBG("onCancel: ignorning");
}

static const char * getVersion(void)
{
    DBG("getVersion: return '%s'", MOCK_RIL_VER_STRING);
    return MOCK_RIL_VER_STRING;
}

/**
 * "t" is parameter passed in on previous call to RIL_Notification
 * routine.
 *
 * If "e" != SUCCESS, then response can be null/is ignored
 *
 * "response" is owned by caller, and should not be modified or
 * freed by callee
 *
 * RIL_onRequestComplete will return as soon as possible
 */
void testOnRequestComplete(RIL_Token t, RIL_Errno e,
                       void *response, size_t responselen) {
    DBG("testOnRequestComplete E: token=%p rilErrCode=%d data=%p datalen=%d",
            t, e, response, responselen);
    DBG("testOnRequestComplete X:");
}

/**
 * "unsolResponse" is one of RIL_UNSOL_RESPONSE_*
 * "data" is pointer to data defined for that RIL_UNSOL_RESPONSE_*
 *
 * "data" is owned by caller, and should not be modified or freed by callee
 */
void testOnUnsolicitedResponse(int unsolResponse, const void *data,
                                size_t datalen) {
    DBG("testOnUnsolicitedResponse ignoring");
}

/**
 * Call user-specifed "callback" function on on the same thread that
 * RIL_RequestFunc is called. If "relativeTime" is specified, then it specifies
 * a relative time value at which the callback is invoked. If relativeTime is
 * NULL or points to a 0-filled structure, the callback will be invoked as
 * soon as possible
 */
void testRequestTimedCallback(RIL_TimedCallback callback,
                               void *param, const struct timeval *relativeTime) {
    DBG("testRequestTimedCallback ignoring");
}

#if 0
class UnsolicitedThread : public WorkerThread {
  private:
    v8::Handle<v8::Context> context_;

  public:
    UnsolicitedThread(v8::Handle<v8::Context> context) :
        context_(context) {
    }

    int OnUnsolicitedTick(int tick) {
        v8::HandleScope handle_scope;

        // Get handle to onUnslicitedTick.
        v8::Handle<v8::String> name = v8::String::New("onUnsolicitedTick");
        v8::Handle<v8::Value> functionValue = context_->Global()->Get(name);
        v8::Handle<v8::Function> onUnsolicitedTick =
                v8::Handle<v8::Function>::Cast(functionValue);

        // Create the argument array
        v8::Handle<v8::Value> v8TickValue = v8::Number::New(tick);
        v8::Handle<v8::Value> argv[1] = { v8TickValue };

        v8::Handle<v8::Value> resultValue =
            onUnsolicitedTick->Call(context_->Global(), 1, argv);
        int result = int(resultValue->NumberValue());
        return result;
    }

    void * Worker(void *param)
    {
        LOGD("UnsolicitedThread::Worker E param=%p", param);

        v8::Locker locker;

        for (int i = 0; isRunning(); i++) {
            // Get access and setup scope
            v8::HandleScope handle_scope;
            v8::Context::Scope context_scope(context_);

            // Do it
            int sleepTime = OnUnsolicitedTick(i);

            // Wait
            v8::Unlocker unlocker;
            sleep(sleepTime);
            v8::Locker locker;
        }

        LOGD("UnsolicitedThread::Worker X param=%p", param);

        return NULL;
    }
};
#endif

void startMockRil(v8::Handle<v8::Context> context) {
    v8::HandleScope handle_scope;
    v8::TryCatch try_catch;

    // Get handle to startMockRil and call it.
    v8::Handle<v8::String> name = v8::String::New("startMockRil");
    v8::Handle<v8::Value> functionValue = context->Global()->Get(name);
    v8::Handle<v8::Function> start =
            v8::Handle<v8::Function>::Cast(functionValue);

    v8::Handle<v8::Value> result = start->Call(context->Global(), 0, NULL);
    if (try_catch.HasCaught()) {
        LOGE("startMockRil error");
        ReportException(&try_catch);
        LOGE("FATAL ERROR: Unsable to startMockRil.");
    } else {
        v8::String::Utf8Value result_string(result);
        LOGE("startMockRil result=%s", ToCString(result_string));
    }

}


const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc,
        char **argv) {
    int ret;
    pthread_attr_t attr;

    LOGD("RIL_Init E: ----------------");

    // Initialize V8
    v8::V8::Initialize();

    // We're going to use multiple threads need to start locked
    v8::Locker locker;

    // Initialize modules
    protobuf_v8::Init();
    WorkerV8Init();

    // Make a context and setup a scope
    v8::Persistent<v8::Context> context = makeJsContext();
    v8::Context::Scope context_scope(context);
    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    // Initialize modules needing context
    ctrlServerInit(context);

    s_rilenv = &testEnv;

    // load/run mock_ril.js
    char *buffer;
    int status = ReadFile("/sdcard/data/mock_ril.js", &buffer);
    if (status == 0) {
        runJs(context, &try_catch, "mock_ril.js", buffer);
        if (try_catch.HasCaught()) {
            // TODO: Change to event this is fatal
            LOGE("FATAL ERROR: Unable to run mock_ril.js");
        }
    }

    s_rilenv = env;
    requestsInit(context, &s_requestWorkerQueue);
    responsesInit(context);

#if 0
    LOGD("RIL_Init run tests #####################");
    testJsSupport(context);
    testRequests(context);
    experiments(context);
    testWorker();
    testWorkerV8(context);
    LOGD("RIL_Init tests completed ###############");
#endif

    // Register our call backs so when we startMockRil
    // and it wants to send unsolicited messages the
    // mock ril is registered
    RIL_register(&s_callbacks);

    // Start the mock ril
    startMockRil(context);

#if 0
    UnsolicitedThread *ut = new UnsolicitedThread(context);
    ut->Run(NULL);
#endif

    LOGD("RIL_Init X: ----------------");
    return &s_callbacks;
}

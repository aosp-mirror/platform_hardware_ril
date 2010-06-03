/*
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
    LOGD("create T");
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

pthread_t s_tid_mainloop;

const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
    int ret;
    pthread_attr_t attr;

    LOGD("RIL_Init E");

    s_rilenv = env;

    ret = pthread_attr_init (&attr);
    if (ret != 0) {
        LOGE("RIL_Init X: pthread_attr_init failed err=%s", strerror(ret));
        return NULL;
    }
    ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (ret != 0) {
        LOGE("RIL_Init X: pthread_attr_setdetachstate failed err=%s", strerror(ret));
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

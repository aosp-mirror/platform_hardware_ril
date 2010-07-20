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

#ifndef MOCK_RIL_CONTROL_SERVER_H_
#define MOCK_RIL_CONTROL_SERVER_H_

#include <v8.h>

/**
 * All Control messages start with a header followed
 * by an optional protobuf. If length_protobuf is 0
 * there is no protobuf following the header.
 */
struct MsgHeader {
    int32_t cmd;
    int64_t token;
    int32_t status;
    int32_t length_protobuf;
};

extern v8::Handle<v8::Value> SendCtrlRequestComplete(const v8::Arguments& args);

extern void ctrlServerInit(v8::Handle<v8::Context> context);

#endif // MOCK_RIL_CONTROL_SERVER_H_

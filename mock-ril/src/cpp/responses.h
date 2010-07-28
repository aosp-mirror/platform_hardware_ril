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

#ifndef MOCK_RIL_RESPONSES_H_
#define MOCK_RIL_RESPONSES_H_

#include <v8.h>

/**
 * Send a ril request complete, data is optional
 *
 * args[0] = rilErrCode
 * args[1] = cmd
 * args[2] = token
 * args[3] = optional data
 */
#define REQUEST_COMPLETE_REQUIRED_CMDS      3
#define REQUEST_COMPLETE_RIL_ERR_CODE_INDEX 0
#define REQUEST_COMPLETE_CMD_INDEX          1
#define REQUEST_COMPLETE_TOKEN_INDEX        2
#define REQUEST_COMPLETE_DATA_INDEX         3
v8::Handle<v8::Value> SendRilRequestComplete(const v8::Arguments& args);

/**
 * Send a ril unsolicited response, buffer is optional
 *
 * args[0] = cmd
 * args[1] = optional data
 */
#define UNSOL_RESPONSE_REQUIRED_CMDS      1
#define UNSOL_RESPONSE_CMD_INDEX          0
#define UNSOL_RESPONSE_DATA_INDEX         1
v8::Handle<v8::Value> SendRilUnsolicitedResponse(const v8::Arguments& args);

// Initialize module
int responsesInit(v8::Handle<v8::Context> context);

#endif  // MOCK_RIL_RESPONSES_H_

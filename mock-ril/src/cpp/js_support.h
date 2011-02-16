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

#ifndef MOCK_RIL_JS_SUPPORT_H_
#define MOCK_RIL_JS_SUPPORT_H_

#include <v8.h>
#include "ril.h"

// The global value of radio state shared between cpp and js code.
extern RIL_RadioState gRadioState;

// A javascript print function
extern v8::Handle<v8::Value> Print(const v8::Arguments& args);

// Read a file into a array returning the buffer and the size
extern int ReadFile(const char *fileName, char** data, size_t *length = NULL);

// A javascript read file function arg[0] = filename
extern v8::Handle<v8::Value> ReadFileToString(const v8::Arguments& args);

// A javascript read file function arg[0] = filename
extern v8::Handle<v8::Value> ReadFileToBuffer(const v8::Arguments& args);

// make the Java
extern v8::Persistent<v8::Context> makeJsContext();

// Run a javascript
extern void runJs(v8::Handle<v8::Context> context, v8::TryCatch *try_catch,
           const char *fileName, const char *code);

// Test this module
extern void testJsSupport(v8::Handle<v8::Context> context);

#endif // MOCK_RIL_JS_SUPPORT_H_

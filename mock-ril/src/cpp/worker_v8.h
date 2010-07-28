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

#ifndef MOCK_RIL_WORKER_V8_H_
#define MOCK_RIL_WORKER_V8_H_

#include <v8.h>

// Initialize this module
extern void WorkerV8Init();

// Add Worker to the object template
extern void WorkerV8ObjectTemplateInit(v8::Handle<v8::ObjectTemplate> target);

extern void testWorkerV8(v8::Handle<v8::Context> context);

#endif //  MOCK_RIL_WORKER_V8_H_

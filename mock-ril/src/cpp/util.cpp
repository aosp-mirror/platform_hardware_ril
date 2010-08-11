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
#include <string.h>

#include "logging.h"
#include "util.h"

// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
    return *value ? *value : "<string conversion failed>";
}

// Extracts a C string from a V8 AsciiValue.
const char* ToCString(const v8::String::AsciiValue& value) {
    return *value ? *value : "<string conversion failed>";
}

// Extracts a C string from a v8::Value
const char* ToCString(v8::Handle<v8::Value> value) {
    v8::String::AsciiValue strAsciiValue(value);
    return ToCString(strAsciiValue);
}

// Report an exception
void LogErrorMessage(v8::Handle<v8::Message> message,
        const char *alternate_message) {
    v8::HandleScope handle_scope;
    if (message.IsEmpty()) {
        // V8 didn't provide any extra information about this error; just
        // print the exception.
        if (alternate_message == NULL || strlen(alternate_message) == 0) {
            LOGD("LogErrorMessage no message");
        } else {
            LOGD("LogErrorMessage no message: %s", alternate_message);
        }
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
        int lenErr = end - start;
        int size = end + 1;
        if (lenErr == 0) {
            lenErr += 1;
            size += 1;
        }
        char *error_string = new char[size];
        memset(error_string, ' ', start);
        memset(&error_string[start], '^', lenErr);
        error_string[size-1] = 0;
        LOGD("%s", error_string);
        LOGD("%s", ToCString(v8::String::Utf8Value(message->Get())));
        delete [] error_string;
    }
}

// Report an exception
void ReportException(v8::TryCatch* try_catch) {
    v8::HandleScope handle_scope;

    v8::String::Utf8Value exception(try_catch->Exception());
    v8::Handle<v8::Message> msg = try_catch->Message();
    if (msg.IsEmpty()) {
        // Why is try_catch->Message empty?
        // it is always empty on compile errors
    }
    LogErrorMessage(msg, ToCString(exception));
}

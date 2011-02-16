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

#include "ctrl_server.h"
#include "logging.h"
#include "node_buffer.h"
#include "node_object_wrap.h"
#include "node_util.h"
#include "protobuf_v8.h"
#include "responses.h"
#include "status.h"
#include "util.h"
#include "worker.h"
#include "worker_v8.h"

#include "js_support.h"

//#define JS_SUPPORT_DEBUG
#ifdef  JS_SUPPORT_DEBUG

#define DBG(...) LOGD(__VA_ARGS__)

#else

#define DBG(...)

#endif

/**
 * Current Radio state
 */
RIL_RadioState gRadioState = RADIO_STATE_UNAVAILABLE;

v8::Handle<v8::Value> RadioStateGetter(v8::Local<v8::String> property,
        const v8::AccessorInfo& info) {
    return v8::Integer::New((int)gRadioState);
}

void RadioStateSetter(v8::Local<v8::String> property,
        v8::Local<v8::Value> value, const v8::AccessorInfo& info) {
    gRadioState = RIL_RadioState(value->Int32Value());
}

// A javascript sleep for a number of milli-seconds
v8::Handle<v8::Value> MsSleep(const v8::Arguments& args) {
    if (args.Length() != 1) {
        DBG("MsSleep: expecting milli-seconds to sleep");
    } else {
        v8::Handle<v8::Value> v8MsValue(args[0]->ToObject());
        int ms = int(v8MsValue->NumberValue());
        v8::Unlocker unlocker;
        usleep(ms * 1000);
        v8::Locker locker;
    }
    return v8::Undefined();
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

int ReadFile(const char *fileName, char** data, size_t *length) {
    int status;
    char* buffer = NULL;
    size_t fileLength = 0;
    FILE *f;

    DBG("ReadFile E fileName=%s", fileName);

    f = fopen(fileName, "rb");
    if (f == NULL) {
        DBG("Could not fopen '%s'", fileName);
        status = STATUS_COULD_NOT_OPEN_FILE;
    } else {
        // Determine the length of the file
        fseek(f, 0, SEEK_END);
        fileLength = ftell(f);
        DBG("fileLength=%d", fileLength);
        rewind(f);

        // Read file into a buffer
        buffer = new char[fileLength+1];
        size_t readLength = fread(buffer, 1, fileLength, f);
        if (readLength != fileLength) {
            DBG("Couldn't read entire file");
            delete [] buffer;
            buffer = NULL;
            status = STATUS_COULD_NOT_READ_FILE;
        } else {
            DBG("File read");
            buffer[fileLength] = 0;
            status = STATUS_OK;
        }
        fclose(f);
    }

    if (length != NULL) {
        *length = fileLength;
    }
    *data = buffer;
    DBG("ReadFile X status=%d", status);
    return status;
}

char *CreateFileName(const v8::Arguments& args) {
    v8::String::Utf8Value fileNameUtf8Value(args[0]);
    const char* fileName = ToCString(fileNameUtf8Value);
    const char* directory = "/sdcard/data/";

    int fullPathLength = strlen(directory) + strlen(fileName) + 1;
    char * fullPath = new char[fullPathLength];
    strncpy(fullPath, directory, fullPathLength);
    strncat(fullPath, fileName, fullPathLength);
    return fullPath;
}

// A javascript read file function arg[0] = filename
v8::Handle<v8::Value> ReadFileToString(const v8::Arguments& args) {
    DBG("ReadFileToString E");
    v8::HandleScope handle_scope;
    v8::Handle<v8::Value> retValue;

    if (args.Length() < 1) {
        // No file name return Undefined
        DBG("ReadFile X no argumens");
        return v8::Undefined();
    } else {
        char *fileName = CreateFileName(args);

        char *buffer;
        int status = ReadFile(fileName, &buffer);
        if (status == 0) {
            retValue = v8::String::New(buffer);
        } else {
            retValue = v8::Undefined();
        }
    }
    DBG("ReadFileToString X");
    return retValue;
}

// A javascript read file function arg[0] = filename
v8::Handle<v8::Value> ReadFileToBuffer(const v8::Arguments& args) {
    DBG("ReadFileToBuffer E");
    v8::HandleScope handle_scope;
    v8::Handle<v8::Value> retValue;

    if (args.Length() < 1) {
        // No file name return Undefined
        DBG("ReadFileToBuffer X no argumens");
        return v8::Undefined();
    } else {
        char *fileName = CreateFileName(args);

        char *buffer;
        size_t length;
        int status = ReadFile(fileName, &buffer, &length);
        if (status == 0) {
            Buffer *buf = Buffer::New(length);
            memmove(buf->data(), buffer, length);
            retValue = buf->handle_;
        } else {
            retValue = v8::Undefined();
        }
    }
    DBG("ReadFileToBuffer X");
    return retValue;
}

void ErrorCallback(v8::Handle<v8::Message> message,
        v8::Handle<v8::Value> data) {
    LogErrorMessage(message, "");
}

// Read, compile and run a javascript file
v8::Handle<v8::Value> Include(const v8::Arguments& args) {
    DBG("Include E");
    v8::HandleScope handle_scope;
    v8::Handle<v8::Value> retValue;
    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    if (args.Length() < 1) {
        // No file name return Undefined
        DBG("Include X no argumens");
        return v8::Undefined();
    } else {
        char *fileName = CreateFileName(args);

        char *buffer;
        int status = ReadFile(fileName, &buffer);
        if (status == 0) {
            runJs(v8::Context::GetCurrent(), &try_catch, fileName, buffer);
        } else {
            retValue = v8::Undefined();
        }
    }
    DBG("Include X");
    return retValue;
}


/**
 * Create a JsContext, must be called within a HandleScope?
 */
v8::Persistent<v8::Context> makeJsContext() {
    v8::HandleScope handle_scope;
    v8::TryCatch try_catch;

    // Add a Message listner to Catch errors as they occur
    v8::V8::AddMessageListener(ErrorCallback);

    // Create a template for the global object and
    // add the function template for print to it.
    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
    global->SetAccessor(v8::String::New("gRadioState"),
            RadioStateGetter, RadioStateSetter);
    global->Set(v8::String::New("msSleep"), v8::FunctionTemplate::New(MsSleep));
    global->Set(v8::String::New("print"), v8::FunctionTemplate::New(Print));
    global->Set(v8::String::New("readFileToBuffer"),
            v8::FunctionTemplate::New(ReadFileToBuffer));
    global->Set(v8::String::New("readFileToString"),
            v8::FunctionTemplate::New(ReadFileToString));
    global->Set(v8::String::New("sendRilRequestComplete"),
            v8::FunctionTemplate::New(SendRilRequestComplete));
    global->Set(v8::String::New("sendRilUnsolicitedResponse"),
            v8::FunctionTemplate::New(SendRilUnsolicitedResponse));
    global->Set(v8::String::New("sendCtrlRequestComplete"),
            v8::FunctionTemplate::New(SendCtrlRequestComplete));
    global->Set(v8::String::New("include"), v8::FunctionTemplate::New(Include));
    WorkerV8ObjectTemplateInit(global);
    SchemaObjectTemplateInit(global);
    Buffer::InitializeObjectTemplate(global);

    // Create context with our globals and make it the current scope
    v8::Persistent<v8::Context> context = v8::Context::New(NULL, global);


    if (try_catch.HasCaught()) {
        DBG("makeJsContext: Exception making the context");
        ReportException(&try_catch);
        try_catch.ReThrow();
    }

    return context;
}

/**
 * Run some javascript code.
 */
void runJs(v8::Handle<v8::Context> context, v8::TryCatch *try_catch,
        const char *fileName, const char *code) {
    v8::HandleScope handle_scope;

    // Compile the source
    v8::Handle<v8::Script> script = v8::Script::Compile(
                v8::String::New(code), v8::String::New(fileName));
    if (try_catch->HasCaught()) {
        LOGE("-- Compiling the source failed");
    } else {
        // Run the resulting script
        v8::Handle<v8::Value> result = script->Run();
        if (try_catch->HasCaught()) {
            LOGE("-- Running the script failed");
        }
    }
}

void testRadioState(v8::Handle<v8::Context> context) {
    LOGD("testRadioState E:");
    v8::HandleScope handle_scope;

    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    runJs(context, &try_catch, "local-string",
        "for(i = 0; i < 10; i++) {\n"
        "  gRadioState = i;\n"
        "  print('gRadioState=' + gRadioState);\n"
        "}\n"
        "gRadioState = 1;\n"
        "print('last gRadioState=' + gRadioState);\n");
    LOGD("testRadioState X:");
}

void testMsSleep(v8::Handle<v8::Context> context) {
    LOGD("testMsSleep E:");
    v8::HandleScope handle_scope;

    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    runJs(context, &try_catch, "local-string",
        "for(i = 0; i < 10; i++) {\n"
        "  sleeptime = i * 200\n"
        "  print('msSleep ' + sleeptime);\n"
        "  msSleep(sleeptime);\n"
        "}\n");
    LOGD("testMsSleep X:");
}

void testPrint(v8::Handle<v8::Context> context) {
    LOGD("testPrint E:");
    v8::HandleScope handle_scope;

    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    runJs(context, &try_catch, "local-string", "print(\"Hello\")");
    LOGD("testPrint X:");
}

void testCompileError(v8::Handle<v8::Context> context) {
    LOGD("testCompileError E:");
    v8::HandleScope handle_scope;

    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    // +++ generate a compile time error
    runJs(context, &try_catch, "local-string", "+++");
    LOGD("testCompileError X:");
}

void testRuntimeError(v8::Handle<v8::Context> context) {
    LOGD("testRuntimeError E:");
    v8::HandleScope handle_scope;

    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    // Runtime error
    runJs(context, &try_catch, "local-string",
        "function hello() {\n"
        "  print(\"Hi there\");\n"
        "}\n"
        "helloo()");
    LOGD("testRuntimeError X:");
}

void testReadFile() {
    char *buffer;
    size_t length;
    int status;

    LOGD("testReadFile E:");

    status = ReadFile("/sdcard/data/no-file", &buffer, &length);
    LOGD("testReadFile expect status != 0, status=%d, buffer=%p, length=%d",
            status, buffer, length);

    LOGD("testReadFile X:");
}


void testReadFileToStringBuffer(v8::Handle<v8::Context> context) {
    LOGD("testReadFileToStringBuffer E:");
    v8::HandleScope handle_scope;

    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    runJs(context, &try_catch, "local-string",
        "fileContents = readFileToString(\"mock_ril.js\");\n"
        "print(\"fileContents:\\n\" + fileContents);\n"
        "buffer = readFileToBuffer(\"ril.desc\");\n"
        "print(\"buffer.length=\" + buffer.length);\n");
    LOGD("testReadFileToStringBuffer X:");
}

void testJsSupport(v8::Handle<v8::Context> context) {
    LOGD("testJsSupport E: ********");
    testRadioState(context);
    testMsSleep(context);
    testPrint(context);
    testCompileError(context);
    testRuntimeError(context);
    testReadFile();
    testReadFileToStringBuffer(context);
    LOGD("testJsSupport X: ********\n");
}

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

#include <queue>
#include <v8.h>

#include "logging.h"
#include "js_support.h"
#include "node_object_wrap.h"
#include "node_util.h"
#include "util.h"
#include "worker.h"

#include "worker_v8.h"


//#define WORKER_V8_V8_DEBUG
#ifdef  WORKER_V8_V8_DEBUG

#define DBG(...) LOGD(__VA_ARGS__)

#else

#define DBG(...)

#endif

v8::Persistent<v8::FunctionTemplate> WorkerV8Template;

class WorkerV8 : public ObjectWrap {
  private:
    friend class Handler;


    struct ArgInfo {
        v8::Persistent<v8::Object> js_this;
        v8::Persistent<v8::Value> value;
    };

    pthread_mutex_t ai_free_list_mutex_;
    std::queue<ArgInfo *>  ai_free_list_;

    ArgInfo *ObtainArgInfo() {
        ArgInfo *ai;
        pthread_mutex_lock(&ai_free_list_mutex_);
        if (ai_free_list_.size() == 0) {
            ai = new ArgInfo();
        } else {
            ai = ai_free_list_.front();
            ai_free_list_.pop();
        }
        pthread_mutex_unlock(&ai_free_list_mutex_);
        return ai;
    }

    void ReleaseArgInfo(ArgInfo *ai) {
        pthread_mutex_lock(&ai_free_list_mutex_);
        ai_free_list_.push(ai);
        pthread_mutex_unlock(&ai_free_list_mutex_);
    }

    class Handler : public WorkerQueue {
      private:
        v8::Persistent<v8::Value> functionValue_;
        WorkerV8 *worker_;

      public:
        Handler(WorkerV8 *worker, v8::Handle<v8::Value> value)
            : worker_(worker) {
            functionValue_ = v8::Persistent<v8::Value>::New(value);
        }

        void Process(void *param) {
            DBG("Handler::Process: E");

            v8::Locker locker;
            v8::HandleScope handle_scope;
            v8::TryCatch try_catch;
            try_catch.SetVerbose(true);

            ArgInfo *ai = (ArgInfo*)param;
            v8::Handle<v8::Value> args(ai->value);
            v8::Function::Cast(*functionValue_)->Call(ai->js_this, 1, &args);

            ai->js_this.Dispose();
            ai->value.Dispose();

            worker_->ReleaseArgInfo(ai);

            DBG("Handler::Process: X");
        }
    };

    Handler *handler_;

  public:
    WorkerV8(v8::Handle<v8::Object> self, v8::Handle<v8::Value> functionValue) {
        DBG("WorkerV8::WorkerV8 E:");
        pthread_mutex_init(&ai_free_list_mutex_, NULL);
        handler_ = new Handler(this, functionValue);
        Wrap(self);
        DBG("WorkerV8::WorkerV8 X: this=%p handler_=%p", this, handler_);
    }

    virtual ~WorkerV8() {
        DBG("~WorkerV8::WorkerV8 E:");
        DBG("~WorkerV8::WorkerV8 X:");
    }

    static v8::Handle<v8::Value> Run(const v8::Arguments& args) {
        WorkerV8 *workerV8 = ObjectWrap::Unwrap<WorkerV8>(args.This());
        DBG("WorkerV8::Run(args) E:");
        workerV8->handler_->Run();
        DBG("WorkerV8::Run(args) X:");
        return v8::Undefined();
    }

    static v8::Handle<v8::Value> Add(const v8::Arguments& args) {
        DBG("WorkerV8::Add(args) E:");
        WorkerV8 *workerV8 = ObjectWrap::Unwrap<WorkerV8>(args.This());

        // Validate one argument to add
        if (args.Length() != 1) {
            DBG("WorkerV8::Add(args) X: expecting one param");
            return v8::ThrowException(v8::String::New("Add has no parameter"));
        }
        ArgInfo *ai = workerV8->ObtainArgInfo();
        ai->js_this = v8::Persistent<v8::Object>::New( args.This() );
        ai->value = v8::Persistent<v8::Value>::New( args[0] );

        workerV8->handler_->Add(ai);
        DBG("WorkerV8::Add(args) X:");
        return v8::Undefined();
    }

    static v8::Handle<v8::Value> AddDelayed(const v8::Arguments& args) {
        DBG("WorkerV8::AddDelayed(args) E:");
        WorkerV8 *workerV8 = ObjectWrap::Unwrap<WorkerV8>(args.This());

        // Validate two argument to addDelayed
        if (args.Length() != 2) {
            DBG("WorkerV8::AddDelayed(args) X: expecting two params");
            return v8::ThrowException(v8::String::New("AddDelayed expects req delayTime params"));
        }
        ArgInfo *ai = workerV8->ObtainArgInfo();
        ai->js_this = v8::Persistent<v8::Object>::New( args.This() );
        ai->value = v8::Persistent<v8::Value>::New( args[0] );
        v8::Handle<v8::Value> v8DelayMs(args[1]->ToObject());
        int32_t delay_ms = v8DelayMs->Int32Value();
        workerV8->handler_->AddDelayed(ai, delay_ms);

        DBG("WorkerV8::AddDelayed(args) X:");
        return v8::Undefined();
    }

    static v8::Handle<v8::Value> NewWorkerV8(const v8::Arguments& args) {
        DBG("WorkerV8::NewWorkerV8 E: args.Length()=%d", args.Length());
        WorkerV8 *worker = new WorkerV8(args.This(), args[0]);
        DBG("WorkerV8::NewWorkerV8 X:");
        return worker->handle_;
    }
};

void WorkerV8Init() {
    DBG("WorkerV8Init E:");
    v8::HandleScope handle_scope;

    WorkerV8Template = v8::Persistent<v8::FunctionTemplate>::New(
                           v8::FunctionTemplate::New(WorkerV8::NewWorkerV8));
    WorkerV8Template->SetClassName(v8::String::New("Worker"));
    // native self (Field 0 is handle_) field count is at least 1
    WorkerV8Template->InstanceTemplate()->SetInternalFieldCount(1);

    // Set prototype methods
    SET_PROTOTYPE_METHOD(WorkerV8Template, "run", WorkerV8::Run);
    SET_PROTOTYPE_METHOD(WorkerV8Template, "add", WorkerV8::Add);
    SET_PROTOTYPE_METHOD(WorkerV8Template, "addDelayed", WorkerV8::AddDelayed);

    DBG("WorkerV8Init X:");
}

void testWorkerV8(v8::Handle<v8::Context> context) {
    LOGD("testWorkerV8 E: ********");
    v8::HandleScope handle_scope;

    v8::TryCatch try_catch;
    try_catch.SetVerbose(true);

    LOGD("testWorkerV8 runJs");
    runJs(context, &try_catch, "local-string",
        "var w1 = new Worker(function (msg) {"
        "     print('w1: ' + msg);\n"
        "});\n"
        "w1.run();\n"
        "var w2 = new Worker(function (msg) {"
        "     print('w2: ' + msg);\n"
        "});\n"
        "w2.run();\n"
        "w2.addDelayed('three', 1000);\n"
        "w2.add('one');\n"
        "w1.add('two');\n"
        "w1.addDelayed('four', 2000);\n"
    );
    LOGD("testWorkerV8 X: ********");
}

extern void WorkerV8ObjectTemplateInit(v8::Handle<v8::ObjectTemplate> target) {
    DBG("WorkerV8ObjectTemplateInit(target) E:");
    target->Set(v8::String::New("Worker"), WorkerV8Template);
    DBG("WorkerV8ObjectTemplateInit(target) X:\n");
}

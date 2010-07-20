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

#include "logging.h"

#include "worker.h"

//#define WORKER_DEBUG
#ifdef  WORKER_DEBUG

#define DBG(...) LOGD(__VA_ARGS__)

#else

#define DBG(...)

#endif

void * WorkerThread::Work(void *param) {
    WorkerThread *t = (WorkerThread *)param;
    android_atomic_acquire_store(STATE_RUNNING, &t->state_);
    void * v = t->Worker(t->workerParam_);
    android_atomic_acquire_store(STATE_STOPPED, &t->state_);
    return v;
}

bool WorkerThread::isRunning() {
    DBG("WorkerThread::isRunning E");
    bool ret_value = android_atomic_acquire_load(&state_) == STATE_RUNNING;
    DBG("WorkerThread::isRunning X ret_value=%d", ret_value);
    return ret_value;
}

WorkerThread::WorkerThread() {
    DBG("WorkerThread::WorkerThread E");
    state_ = STATE_INITIALIZED;
    pthread_mutex_init(&mutex_, NULL);
    pthread_cond_init(&cond_, NULL);
    DBG("WorkerThread::WorkerThread X");
}

WorkerThread::~WorkerThread() {
    DBG("WorkerThread::~WorkerThread E");
    Stop();
    DBG("WorkerThread::~WorkerThread X");
}

// Return true if changed from STATE_RUNNING to STATE_STOPPING
bool WorkerThread::BeginStopping() {
    DBG("WorkerThread::BeginStopping E");
    bool ret_value = (android_atomic_acquire_cas(STATE_RUNNING, STATE_STOPPING, &state_) == 0);
    DBG("WorkerThread::BeginStopping X ret_value=%d", ret_value);
    return ret_value;
}

// Wait until state is not STATE_STOPPING
void WorkerThread::WaitUntilStopped() {
    DBG("WorkerThread::WaitUntilStopped E");
    pthread_cond_signal(&cond_);
    while(android_atomic_release_load(&state_) == STATE_STOPPING) {
        usleep(200000);
    }
    DBG("WorkerThread::WaitUntilStopped X");
}

void WorkerThread::Stop() {
    DBG("WorkerThread::Stop E");
    if (BeginStopping()) {
        WaitUntilStopped();
    }
    DBG("WorkerThread::Stop X");
}

int WorkerThread::Run(void *workerParam) {
    DBG("WorkerThread::Run E workerParam=%p", workerParam);
    int status;
    int ret;

    workerParam_ = workerParam;

    ret = pthread_attr_init(&attr_);
    if (ret != 0) {
        LOGE("RIL_Init X: pthread_attr_init failed err=%s", strerror(ret));
        return STATUS_ERR;
    }
    ret = pthread_attr_setdetachstate(&attr_, PTHREAD_CREATE_DETACHED);
    if (ret != 0) {
        LOGE("RIL_Init X: pthread_attr_setdetachstate failed err=%s",
                strerror(ret));
        return STATUS_ERR;
    }
    ret = pthread_create(&tid_, &attr_,
                (void * (*)(void *))&WorkerThread::Work, this);
    if (ret != 0) {
        LOGE("RIL_Init X: pthread_create failed err=%s", strerror(ret));
        return STATUS_ERR;
    }

    // Wait until worker is running
    while (android_atomic_acquire_load(&state_) == STATE_INITIALIZED) {
        usleep(200000);
    }

    DBG("WorkerThread::Run X workerParam=%p" workerParam);
    return STATUS_OK;
}


class WorkerQueueThread : public WorkerThread {
  private:
    friend class WorkerQueue;

  public:
    WorkerQueueThread() {
    }

    virtual ~WorkerQueueThread() {
        Stop();
    }

    void * Worker(void *param) {
        DBG("WorkerQueueThread::Worker E");
        WorkerQueue *wq = (WorkerQueue *)param;

        // Do the work until we're told to stop
        while (isRunning()) {
            pthread_mutex_lock(&mutex_);
            while (isRunning() && (wq->q_.size() == 0)) {
                pthread_cond_wait(&cond_, &mutex_);
            }
            if (isRunning()) {
                void *p = wq->q_.front();
                wq->q_.pop();
                pthread_mutex_unlock(&mutex_);
                wq->Process(p);
            } else {
                pthread_mutex_unlock(&mutex_);
            }
        }
        DBG("WorkerQueueThread::Worker X");
        return NULL;
    }
};

WorkerQueue::WorkerQueue() {
    wqt_ = new WorkerQueueThread();
}

WorkerQueue::~WorkerQueue() {
    Stop();
    delete wqt_;
}

int WorkerQueue::Run() {
    return wqt_->Run(this);
}

void WorkerQueue::Stop() {
    wqt_->Stop();
}

void WorkerQueue::Add(void *p) {
    DBG("WorkerQueue::Add E:");
    pthread_mutex_lock(&wqt_->mutex_);
    q_.push(p);
    if (q_.size() == 1) {
        pthread_cond_signal(&wqt_->cond_);
    }
    pthread_mutex_unlock(&wqt_->mutex_);
    DBG("WorkerQueue::Add X:");
}



class TestWorkerQueue : public WorkerQueue {
    virtual void Process(void *p) {
        LOGD("TestWorkerQueue::Process: EX p=%p", p);
    }
};

class TesterThread : public WorkerThread {
  public:
    void * Worker(void *param)
    {
        LOGD("TesterThread::Worker E param=%p", param);
        WorkerQueue *wq = (WorkerQueue *)param;

        for (int i = 0; isRunning(); i++) {
            LOGD("TesterThread: looping %d", i);
            wq->Add((void *)i);
            wq->Add((void *)i);
            wq->Add((void *)i);
            wq->Add((void *)i);
            sleep(1);
        }

        LOGD("TesterThread::Worker X param=%p", param);

        return NULL;
    }
};

void testWorker() {
    LOGD("testWorker E: ********");

    // Test we can create a thread and delete it
    TesterThread *tester = new TesterThread();
    delete tester;

    TestWorkerQueue *wq = new TestWorkerQueue();
    if (wq->Run() == STATUS_OK) {
        LOGD("testWorker WorkerQueue %p running", wq);

        // Test we can run a thread, stop it then delete it
        tester = new TesterThread();
        tester->Run(wq);
        LOGD("testWorker tester %p running", tester);
        sleep(10);
        LOGD("testWorker tester %p stopping", tester);
        tester->Stop();
        LOGD("testWorker tester %p stopped", tester);
        wq->Stop();
        LOGD("testWorker wq %p stopped", wq);
    }
    LOGD("testWorker X: ********\n");
}

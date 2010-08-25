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
#include "status.h"
#include "worker.h"

#include <time.h>

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
    pthread_mutex_destroy(&mutex_);
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

    DBG("WorkerThread::Run X workerParam=%p", workerParam);
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
            while (isRunning() && wq->q_.size() == 0) {
                if (wq->delayed_q_.size() == 0) {
                    // Both queue's are empty so wait
                    pthread_cond_wait(&cond_, &mutex_);
                } else {
                    // delayed_q_ is not empty, move any
                    // timed out records to q_.
                    int64_t now = android::elapsedRealtime();
                    while((wq->delayed_q_.size() != 0) &&
                            ((wq->delayed_q_.top()->time - now) <= 0)) {
                        struct WorkerQueue::Record *r = wq->delayed_q_.top();
                        DBG("WorkerQueueThread::Worker move p=%p time=%lldms",
                                r->p, r->time);
                        wq->delayed_q_.pop();
                        wq->q_.push_back(r);
                    }

                    if ((wq->q_.size() == 0) && (wq->delayed_q_.size() != 0)) {
                        // We need to do a timed wait
                        struct timeval tv;
                        struct timespec ts;
                        struct WorkerQueue::Record *r = wq->delayed_q_.top();
                        int64_t delay_ms = r->time - now;
                        DBG("WorkerQueueThread::Worker wait"
                            " p=%p time=%lldms delay_ms=%lldms",
                                r->p, r->time, delay_ms);
                        gettimeofday(&tv, NULL);
                        ts.tv_sec = tv.tv_sec + (delay_ms / 1000);
                        ts.tv_nsec = (tv.tv_usec +
                                        ((delay_ms % 1000) * 1000)) * 1000;
                        pthread_cond_timedwait(&cond_, &mutex_, &ts);
                    }
                }
            }
            if (isRunning()) {
                struct WorkerQueue::Record *r = wq->q_.front();
                wq->q_.pop_front();
                void *p = r->p;
                wq->release_record(r);
                pthread_mutex_unlock(&mutex_);
                wq->Process(r->p);
            } else {
                pthread_mutex_unlock(&mutex_);
            }
        }
        DBG("WorkerQueueThread::Worker X");
        return NULL;
    }
};

WorkerQueue::WorkerQueue() {
    DBG("WorkerQueue::WorkerQueue E");
    wqt_ = new WorkerQueueThread();
    DBG("WorkerQueue::WorkerQueue X");
}

WorkerQueue::~WorkerQueue() {
    DBG("WorkerQueue::~WorkerQueue E");
    Stop();

    Record *r;
    pthread_mutex_lock(&wqt_->mutex_);
    while(free_list_.size() != 0) {
        r = free_list_.front();
        free_list_.pop_front();
        DBG("WorkerQueue::~WorkerQueue delete free_list_ r=%p", r);
        delete r;
    }
    while(delayed_q_.size() != 0) {
        r = delayed_q_.top();
        delayed_q_.pop();
        DBG("WorkerQueue::~WorkerQueue delete delayed_q_ r=%p", r);
        delete r;
    }
    pthread_mutex_unlock(&wqt_->mutex_);

    delete wqt_;
    DBG("WorkerQueue::~WorkerQueue X");
}

int WorkerQueue::Run() {
    return wqt_->Run(this);
}

void WorkerQueue::Stop() {
    wqt_->Stop();
}

/**
 * Obtain a record from free_list if it is not empty, fill in the record with provided
 * information: *p and delay_in_ms
 */
struct WorkerQueue::Record *WorkerQueue::obtain_record(void *p, int delay_in_ms) {
    struct Record *r;
    if (free_list_.size() == 0) {
        r = new Record();
        DBG("WorkerQueue::obtain_record new r=%p", r);
    } else {
        r = free_list_.front();
        DBG("WorkerQueue::obtain_record reuse r=%p", r);
        free_list_.pop_front();
    }
    r->p = p;
    if (delay_in_ms != 0) {
        r->time = android::elapsedRealtime() + delay_in_ms;
    } else {
        r->time = 0;
    }
    return r;
}

/**
 * release a record and insert into the front of the free_list
 */
void WorkerQueue::release_record(struct Record *r) {
    DBG("WorkerQueue::release_record r=%p", r);
    free_list_.push_front(r);
}

/**
 * Add a record to processing queue q_
 */
void WorkerQueue::Add(void *p) {
    DBG("WorkerQueue::Add E:");
    pthread_mutex_lock(&wqt_->mutex_);
    struct Record *r = obtain_record(p, 0);
    q_.push_back(r);
    if (q_.size() == 1) {
        pthread_cond_signal(&wqt_->cond_);
    }
    pthread_mutex_unlock(&wqt_->mutex_);
    DBG("WorkerQueue::Add X:");
}

void WorkerQueue::AddDelayed(void *p, int delay_in_ms) {
    DBG("WorkerQueue::AddDelayed E:");
    if (delay_in_ms <= 0) {
        Add(p);
    } else {
        pthread_mutex_lock(&wqt_->mutex_);
        struct Record *r = obtain_record(p, delay_in_ms);
        delayed_q_.push(r);
#ifdef WORKER_DEBUG
        int64_t now = android::elapsedRealtime();
        DBG("WorkerQueue::AddDelayed"
            " p=%p delay_in_ms=%d now=%lldms top->p=%p"
            " top->time=%lldms diff=%lldms",
                p, delay_in_ms, now, delayed_q_.top()->p,
                delayed_q_.top()->time, delayed_q_.top()->time - now);
#endif
        if ((q_.size() == 0) && (delayed_q_.top() == r)) {
            // q_ is empty and the new record is at delayed_q_.top
            // so we signal the waiting thread so it can readjust
            // the wait time.
            DBG("WorkerQueue::AddDelayed signal");
            pthread_cond_signal(&wqt_->cond_);
        }
        pthread_mutex_unlock(&wqt_->mutex_);
    }
    DBG("WorkerQueue::AddDelayed X:");
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

        // Test AddDelayed
        wq->AddDelayed((void *)1000, 1000);
        wq->Add((void *)0);
        wq->Add((void *)0);
        wq->Add((void *)0);
        wq->Add((void *)0);
        wq->AddDelayed((void *)100, 100);
        wq->AddDelayed((void *)2000, 2000);

        for (int i = 1; isRunning(); i++) {
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

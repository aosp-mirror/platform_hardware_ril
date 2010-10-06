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

#ifndef MOCK_RIL_WORKER_H_
#define MOCK_RIL_WORKER_H_

#include <queue>
#include <list>
#include <vector>
#include <pthread.h>
#include <cutils/atomic.h>
#include <utils/SystemClock.h>

/**
 * A Thread class.
 *
 * 0) Extend WorkerThread creating a Worker method which
 *    monitors isRunning(). For example:
 *
 *   void * Worker(void *param) {
 *       while (isRunning() == 0) {
 *           pthread_mutex_lock(&mutex_);
 *           while (isRunning() && !SOME-CONDITION) {
 *               pthread_cond_wait(&cond_, &mutex_);
 *           }
 *           if (isRunning()) {
 *               DO-WORK
 *           } else {
 *               pthread_mutex_unlock(&mutex_);
 *           }
 *       }
 *       return NULL;
 *   }
 *
 * 1) Create the WorkerThread.
 * 2) Execute Run passing a param which will be passed to Worker.
 * 3) Call Stop() or destroy the thread to stop processing.
 *
 */
class WorkerThread {
  protected:
    pthread_attr_t attr_;
    pthread_mutex_t mutex_;
    pthread_cond_t cond_;
    pthread_t tid_;
    void *workerParam_;

    #define STATE_INITIALIZED   1
    #define STATE_RUNNING       2
    #define STATE_STOPPING      3
    #define STATE_STOPPED       4
    int32_t state_;

    static void * Work(void *param);

    virtual bool isRunning();

  public:
    WorkerThread();

    virtual ~WorkerThread();

    // Return true if changed from STATE_RUNNING to STATE_STOPPING
    virtual bool BeginStopping();

    // Wait until state is not STATE_STOPPING
    virtual void WaitUntilStopped();

    virtual void Stop();

    virtual int Run(void *workerParam);

    /**
     * Method called to do work, see example above.
     * While running isRunning() must be monitored.
     */
    virtual void *Worker(void *) = 0;
};


/**
 * A WorkerQueue.
 *
 * 0) Extend overriding Process
 * 1) Create an instance
 * 2) Call Run.
 * 3) Call Add, passing a pointer which is added to a queue
 * 4) Process will be called with a pointer as work can be done.
 */
class WorkerQueue {
  private:
    friend class WorkerQueueThread;

    struct Record {
        int64_t time;
        void *p;
    };

    class record_compare {
      public:
        // To get ascending order return true if lhs > rhs.
        bool operator() (const struct Record* lhs, const struct Record* rhs) const {
            return lhs->time > rhs->time;
        }
    };

    std::list<struct Record *> q_;                // list of records to be processed
    std::list<struct Record *> free_list_;        // list of records that have been released
    std::priority_queue<struct Record *, std::vector<struct Record *>, record_compare> delayed_q_;
                                                  // list of records that are delayed
    class WorkerQueueThread *wqt_;

  protected:
    struct Record *obtain_record(void *p, int delay_in_ms);

    void release_record(struct Record *r);

  public:
    WorkerQueue();

    virtual ~WorkerQueue();

    int Run();

    void Stop();

    void Add(void *p);

    void AddDelayed(void *p, int delay_in_ms);

    virtual void Process(void *) = 0;
};

extern void testWorker();

#endif // MOCK_RIL_WORKER_H_

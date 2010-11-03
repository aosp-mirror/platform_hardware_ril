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

#include <alloca.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/endian.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cutils/sockets.h>

#include "logging.h"
#include "node_buffer.h"
#include "status.h"
#include "util.h"
#include "worker.h"

#include "hardware/ril/mock-ril/src/proto/msgheader.pb.h"

#include "hardware/ril/mock-ril/src/proto/ctrl.pb.h"
#include "ctrl_server.h"

//#define CONTROL_SERVER_DEBUG
#ifdef  CONTROL_SERVER_DEBUG

#define DBG(...) LOGD(__VA_ARGS__)

#else

#define DBG(...)

#endif

#define MOCK_RIL_CONTROL_SERVER_STOPPING_SOCKET 54311
#define MOCK_RIL_CONTROL_SERVER_SOCKET 54312

using communication::MsgHeader;

class CtrlServerThread;
static CtrlServerThread *g_ctrl_server;

class CtrlServerThread : public WorkerThread {
  private:
    #define SOCKET_NAME_MOCK_RIL_CST_STOPPER "mock-ril-cst-stopper"
    v8::Handle<v8::Context> context_;
    int server_accept_socket_;
    int server_to_client_socket_;
    int stop_server_fd_;
    int stop_client_fd_;
    int stopper_fd_;
    fd_set rd_fds_;
    fd_set wr_fds_;
    bool done_;

    Buffer *ObtainBuffer(int length) {
        Buffer *b = Buffer::New(length);
        return b;
    }

    int WriteAll(int s, void *data, int length) {
        int ret_value;
        uint8_t *bytes = (uint8_t *)data;
        int count = length;

        while (length > 0) {
            ret_value = send(s, bytes, length, 0);
            if (ret_value < 0) {
                return STATUS_ERR;
            }
            if (ret_value == 0) {
                return STATUS_CLIENT_CLOSED_CONNECTION;
            }
            bytes += ret_value;
            length -= ret_value;
        }

        return STATUS_OK;
    }

    int ReadAll(int s, void *data, int length) {
        int ret_value;
        uint8_t *bytes = (uint8_t *)data;
        int count = length;

        while (length != 0) {
            ret_value = recv(s, bytes, length, 0);
            if (ret_value < 0) {
                return STATUS_ERR;
            }
            if (ret_value == 0) {
                return STATUS_CLIENT_CLOSED_CONNECTION;
            }
            bytes += ret_value;
            length -= ret_value;
        }

        return STATUS_OK;
    }

    int ReadMessage(MsgHeader *mh, Buffer **pBuffer) {
        int status;
        int32_t len_msg_header;

        // Reader header length
        status = ReadAll(server_to_client_socket_, &len_msg_header, sizeof(len_msg_header));
        len_msg_header = letoh32(len_msg_header);
        DBG("rm: read len_msg_header=%d  status=%d", len_msg_header, status);
        if (status != STATUS_OK) return status;

        // Read header into an array allocated on the stack and unmarshall
        uint8_t *msg_header_raw = (uint8_t *)alloca(len_msg_header);
        status = ReadAll(server_to_client_socket_, msg_header_raw, len_msg_header);
        DBG("rm: read msg_header_raw=%p  status=%d", msg_header_raw, status);
        if (status != STATUS_OK) return status;
        mh->ParseFromArray(msg_header_raw, len_msg_header);

        // Read auxillary data
        Buffer *buffer;
        if (mh->length_data() > 0) {
            buffer = ObtainBuffer(mh->length_data());
            status = ReadAll(server_to_client_socket_, buffer->data(), buffer->length());
            DBG("rm: read protobuf status=%d", status);
            if (status != STATUS_OK) return status;
        } else {
            DBG("rm: NO protobuf");
            buffer = NULL;
        }

        *pBuffer = buffer;
        return STATUS_OK;
    }

  public:
    int WriteMessage(MsgHeader *mh, Buffer *buffer) {
        int status;
        uint32_t i;
        uint64_t l;

        // Set length of data
        if (buffer == NULL) {
            mh->set_length_data(0);
        } else {
            mh->set_length_data(buffer->length());
        }

        // Serialize header
        uint32_t len_msg_header = mh->ByteSize();
        uint8_t *msg_header_raw = (uint8_t *)alloca(len_msg_header);
        mh->SerializeToArray(msg_header_raw, len_msg_header);

        // Write length in little endian followed by the header
        i = htole32(len_msg_header);
        status = WriteAll(server_to_client_socket_, &i, 4);
        DBG("wm: write len_msg_header=%d status=%d", len_msg_header, status);
        if (status != 0) return status;
        status = WriteAll(server_to_client_socket_, msg_header_raw, len_msg_header);
        DBG("wm: write msg_header_raw=%p  status=%d", msg_header_raw, status);
        if (status != 0) return status;

        // Write data
        if (mh->length_data() > 0) {
            status = WriteAll(server_to_client_socket_, buffer->data(), buffer->length());
            DBG("wm: protobuf data=%p len=%d status=%d",
                    buffer->data(), buffer->length(), status);
            if (status != 0) return status;
        }

        return STATUS_OK;
    }

    CtrlServerThread(v8::Handle<v8::Context> context) :
            context_(context),
            server_accept_socket_(-1),
            server_to_client_socket_(-1),
            done_(false) {
    }

    virtual int Run() {
        DBG("CtrlServerThread::Run E");

        // Create a server socket.
        server_accept_socket_ = socket_inaddr_any_server(
            MOCK_RIL_CONTROL_SERVER_SOCKET, SOCK_STREAM);
        if (server_accept_socket_ < 0) {
            LOGE("CtrlServerThread::Run error creating server_accept_socket_ '%s'",
                    strerror(errno));
            return STATUS_ERR;
        }

        // Create a server socket that will be used for stopping
        stop_server_fd_ = socket_loopback_server(
                MOCK_RIL_CONTROL_SERVER_STOPPING_SOCKET, SOCK_STREAM);
        if (stop_server_fd_ < 0) {
            LOGE("CtrlServerThread::Run error creating stop_server_fd_ '%s'",
                    strerror(errno));
            return STATUS_ERR;
        }

        // Create a client socket that will be used for sending a stop
        stop_client_fd_ = socket_loopback_client(
                MOCK_RIL_CONTROL_SERVER_STOPPING_SOCKET, SOCK_STREAM);
        if (stop_client_fd_ < 0) {
            LOGE("CtrlServerThread::Run error creating stop_client_fd_ '%s'",
                    strerror(errno));
            return STATUS_ERR;
        }

        // Accept the connection of the stop_client_fd_
        stopper_fd_ = accept(stop_server_fd_, NULL, NULL);
        if (stopper_fd_ < 0) {
            LOGE("CtrlServerThread::Run error accepting stop_client_fd '%s'",
                    strerror(errno));
            return STATUS_ERR;
        }

        // Run the new thread
        int ret_value = WorkerThread::Run(NULL);
        DBG("CtrlServerThread::Run X");
        return ret_value;
    }

    virtual void Stop() {
        DBG("CtrlServerThread::Stop E");
        if (BeginStopping()) {
            done_ = true;
            int rv = send(stop_client_fd_, &done_, sizeof(done_), 0);
            if (rv <= 0) {
                LOGE("CtrlServerThread::Stop could not send stop"
                            "WE WILL PROBABLY HANG");
            }
            WaitUntilStopped();
        }
        DBG("CtrlServerThread::Stop X");
    }

    virtual bool isRunning() {
        bool rv = done_ || WorkerThread::isRunning();
        return rv;
    }

    int WaitOnSocketOrStopping(fd_set *rfds, int s) {
        DBG("WaitOnSocketOrStopping E s=%d stopper_fd_=%d", s, stopper_fd_);
        FD_ZERO(rfds);
        FD_SET(s, rfds);
        FD_SET(stopper_fd_, rfds);
        int fd_number = s > stopper_fd_ ? s + 1 : stopper_fd_ + 1;
        v8::Unlocker unlocker;
        int rv = select(fd_number, rfds, NULL, NULL, NULL);
        v8::Locker locker;
        DBG("WaitOnSocketOrStopping X rv=%d s=%d stopper_fd_=%d", rv, s, stopper_fd_);
        return rv;
    }

    int sendToCtrlServer(MsgHeader *mh, Buffer *buffer) {
        DBG("sendToCtrlServer E: cmd=%d token=%lld", mh->cmd(), mh->token());

        int status = STATUS_OK;
        v8::HandleScope handle_scope;
        v8::TryCatch try_catch;
        try_catch.SetVerbose(true);

        // Get the onRilRequest Function
        v8::Handle<v8::String> name = v8::String::New("onCtrlServerCmd");
        v8::Handle<v8::Value> onCtrlServerCmdFunctionValue =
                context_->Global()->Get(name);
        v8::Handle<v8::Function> onCtrlServerCmdFunction =
                v8::Handle<v8::Function>::Cast(onCtrlServerCmdFunctionValue);

        // Create the CmdValue and TokenValue
        v8::Handle<v8::Value> v8CmdValue = v8::Number::New(mh->cmd());
        v8::Handle<v8::Value> v8TokenValue = v8::Number::New(mh->token());

        // Invoke onRilRequest
        const int argc = 3;
        v8::Handle<v8::Value> buf;
        if (mh->length_data() == 0) {
            buf = v8::Undefined();
        } else {
            buf = buffer->handle_;
        }
        v8::Handle<v8::Value> argv[argc] = {
                v8CmdValue, v8TokenValue, buf };
        v8::Handle<v8::Value> result =
            onCtrlServerCmdFunction->Call(context_->Global(), argc, argv);
        if (try_catch.HasCaught()) {
            ReportException(&try_catch);
            status = STATUS_ERR;
        } else {
            v8::String::Utf8Value result_string(result);
            DBG("sendToCtrlServer result=%s", ToCString(result_string));
            status = STATUS_OK;
        }

        if (status != STATUS_OK) {
            LOGE("sendToCtrlServer Error: status=%d", status);
            // An error report complete now
            mh->set_length_data(0);
            mh->set_status(ril_proto::CTRL_STATUS_ERR);
            g_ctrl_server->WriteMessage(mh, NULL);
        }

        DBG("sendToCtrlServer X: status=%d", status);
        return status;
    }

    virtual void * Worker(void *param) {
        DBG("CtrlServerThread::Worker E param=%p stopper_fd_=%d",
                param, stopper_fd_);

        v8::Locker locker;
        v8::HandleScope handle_scope;
        v8::Context::Scope context_scope(context_);

        while (isRunning()) {
            int ret_value;

            // Wait on either server_accept_socket_ or stopping
            DBG("CtrlServerThread::Worker wait on server for a client");
            WaitOnSocketOrStopping(&rd_fds_, server_accept_socket_);
            if (isRunning() != true) {
                break;
            }

            if (FD_ISSET(server_accept_socket_, &rd_fds_)) {
                server_to_client_socket_ = accept(server_accept_socket_, NULL, NULL);
                DBG("CtrlServerThread::Worker accepted server_to_client_socket_=%d isRunning()=%d",
                        server_to_client_socket_, isRunning());

                int status;
                Buffer *buffer;
                MsgHeader mh;
                while ((server_to_client_socket_ > 0) && isRunning()) {
                    DBG("CtrlServerThread::Worker wait on client for message");
                    WaitOnSocketOrStopping(&rd_fds_, server_to_client_socket_);
                    if (isRunning() != true) {
                        break;
                    }

                    status = ReadMessage(&mh, &buffer);
                    if (status != STATUS_OK) break;

                    if (mh.cmd() == ril_proto::CTRL_CMD_ECHO) {
                        LOGD("CtrlServerThread::Worker echo");
                        status = WriteMessage(&mh, buffer);
                        if (status != STATUS_OK) break;
                    } else {
                        DBG("CtrlServerThread::Worker sendToCtrlServer");
                        status = sendToCtrlServer(&mh, buffer);
                        if (status != STATUS_OK) break;
                    }
                }
                close(server_to_client_socket_);
                server_to_client_socket_ = -1;
            }
        }
        close(stop_server_fd_);
        stop_server_fd_ = -1;

        close(stop_client_fd_);
        stop_client_fd_ = -1;

        close(stopper_fd_);
        stopper_fd_ = -1;

        close(server_accept_socket_);
        server_accept_socket_ = -1;

        DBG("CtrlServerThread::Worker X param=%p", param);
        return NULL;
    }
};

/**
 * Send a control request complete response.
 */
v8::Handle<v8::Value> SendCtrlRequestComplete(const v8::Arguments& args) {
    DBG("SendCtrlRequestComplete E:");
    v8::HandleScope handle_scope;
    v8::Handle<v8::Value> retValue;

    void *data;
    size_t datalen;

    Buffer* buffer;
    MsgHeader mh;

    /**
     * Get the arguments. There should be at least 3, reqNum,
     * ril error code and token. Optionally a Buffer containing
     * the protobuf representation of the data to return.
     */
    if (args.Length() < 3) {
        // Expecting a reqNum, ERROR and token
        LOGE("SendCtrlRequestComplete X %d parameters"
             " expecting at least 3: status, reqNum, and token",
                args.Length());
        return v8::Undefined();
    }
    v8::Handle<v8::Value> v8CtrlStatus(args[0]->ToObject());
    mh.set_status(ril_proto::CtrlStatus(v8CtrlStatus->NumberValue()));
    DBG("SendCtrlRequestComplete: status=%d", mh.status());

    v8::Handle<v8::Value> v8ReqNum(args[1]->ToObject());
    mh.set_cmd(int(v8ReqNum->NumberValue()));
    DBG("SendCtrlRequestComplete: cmd=%d", mh.cmd());

    v8::Handle<v8::Value> v8Token(args[2]->ToObject());
    mh.set_token(int64_t(v8Token->NumberValue()));
    DBG("SendCtrlRequestComplete: token=%lld", mh.token());

    if (args.Length() >= 4) {
        buffer = ObjectWrap::Unwrap<Buffer>(args[3]->ToObject());
        mh.set_length_data(buffer->length());
        DBG("SendCtrlRequestComplete: mh.length_data=%d",
                mh.length_data());
    } else {
        mh.set_length_data(0);
        buffer = NULL;
        DBG("SendCtrlRequestComplete: NO PROTOBUF");
    }

    DBG("SendCtrlRequestComplete: WriteMessage");
    int status = g_ctrl_server->WriteMessage(&mh, buffer);

    DBG("SendCtrlRequestComplete E:");
    return v8::Undefined();
}

void ctrlServerInit(v8::Handle<v8::Context> context) {
    int status;

    g_ctrl_server = new CtrlServerThread(context);
    status = g_ctrl_server->Run();
    if (status != STATUS_OK) {
        LOGE("mock_ril control server could not start");
    } else {
        LOGD("CtrlServer started");
    }

#if 0
    LOGD("Test CtrlServerThread stop sleeping 10 seconds...");
    v8::Unlocker unlocker;
    sleep(10);
    LOGD("Test CtrlServerThread call Stop");
    g_ctrl_server->Stop();
    v8::Locker locker;

    // Restart
    g_ctrl_server = new CtrlServerThread(context);
    status = g_ctrl_server->Run();
    if (status != STATUS_OK) {
        LOGE("mock_ril control server could not start");
    } else {
        DBG("mock_ril control server started");
    }
#endif
}

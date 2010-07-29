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

/**
 * Control Server
 */
function CtrlServer() {
    // The result returned by the request handlers
    var result = new Object();

    this.ctrlGetRadioState = function(req) {
        print('ctrlGetRadioState');

        rsp = Object();
        rsp.state = gRadioState;
        result.responseProtobuf = ctrlSchema['ril_proto.CtrlRspRadioState'].serialize(rsp);

        return result;
    }

    /**
     * Process the request
     */
    this.process = function(req) {
        try {
            print('CtrlServer E: req.cmd=' + req.cmd + ' req.token=' + req.token);

            // Assume the result will be true, successful and nothing to return
            result.sendResponse = true;
            result.ctrlStatus = 0;
            result.responseProtobuf = emptyProtobuf;

            // Default result will be success with no response protobuf
            try {
                result = this.ctrlDispatchTable[req.cmd](req);
            } catch (err) {
                print('ctrlServer: Unknown cmd=' + req.cmd);
                result.ctrlStatus = 1; //ril_proto.CTRL_STATUS_ERR;
            }

            if (result.sendResponse) {
                sendCtrlRequestComplete(result.ctrlStatus, req.cmd,
                        req.token, result.responseProtobuf);
            }

            print('CtrlServer X: req.cmd=' + req.cmd + ' req.token=' + req.token);
        } catch (err) {
            print('CtrlServer X: Exception req.cmd=' +
                    req.cmd + ' req.token=' + req.token + ' err=' + err);
        }
    }

    print('CtrlServer() ctor E');
    this.ctrlDispatchTable = new Array();
    this.ctrlDispatchTable[1] = this.ctrlGetRadioState;
    print('CtrlServer() ctor X');
}

// The control server instance and its associated Worker
var ctrlServer = new CtrlServer();
var ctrlWorker = new Worker(function (req) {
    ctrlServer.process(req);
});
ctrlWorker.run();


/**
 * Add the request to the ctrlServer Worker.
 */
function onCtrlServerCmd(cmd, token, protobuf) {
    try {
        //print('onCtrlServerCmd E cmd=' + cmd + ' token=' + token);

        req = new Object();
        req.cmd = cmd;
        req.token = token;
        req.protobuf = protobuf;
        ctrlWorker.add(req);

        //print('onCtrlServerCmd X cmd=' + cmd + ' token=' + token);
    } catch (err) {
        print('onCtrlServerCmd X Exception err=' + err);
    }
}


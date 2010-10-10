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

        var rsp = new Object();
        rsp.state = gRadioState;
        result.responseProtobuf = ctrlSchema['ril_proto.CtrlRspRadioState'].serialize(rsp);

        return result;
    }

    this.ctrlSetRadioState = function(req) {
        print('ctrlSetRadioState');

        var radioReq = new Object();

        // Parse the request protobuf to an object, the returned value is a
        // string that represents the variable name
        radioReq = ctrlSchema['ril_proto.CtrlReqRadioState'].parse(req.protobuf);

        setRadioState(radioReq.state);

        // Prepare the response, return the current radio state
        var rsp = new Object();
        rsp.state = gRadioState;
        result.responseProtobuf = ctrlSchema['ril_proto.CtrlRspRadioState'].serialize(rsp);
        print('gRadioState after setting: ' + gRadioState);
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
            result.ctrlStatus = CTRL_STATUS_OK;
            result.responseProtobuf = emptyProtobuf;

            // Default result will be success with no response protobuf
            try {
                result = (this.ctrlDispatchTable[req.cmd]).call(this, req);
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
    this.ctrlDispatchTable[CTRL_CMD_GET_RADIO_STATE] = this.ctrlGetRadioState;
    this.ctrlDispatchTable[CTRL_CMD_SET_RADIO_STATE] = this.ctrlSetRadioState;
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
        print('onCtrlServerCmd E cmd=' + cmd + ' token=' + token);

        print('onCtrlServerCmd add the request:');


        if (!isCtrlServerDispatchCommand(cmd)) {
            var ctrlServerReq = new Object();
            ctrlServerReq.cmd = cmd;
            ctrlServerReq.token = token;
            ctrlServerReq.protobuf = protobuf;
            print('onCtrlServerCmd: command to control server, add to the worker queue');
            // If it is a command to the control server, add to the control server worker queue
            ctrlWorker.add(ctrlServerReq);
        } else {
            // For other commands, we need to dispatch to the corresponding components
            try {
                print('onCtrlServerCmd: get entry from dispatchTable cmd:' + cmd );
                entry = ctrlServerDispatchTable[cmd];
                if (typeof entry == 'undefined') {
                    throw ('entry = dispatchTable[' + cmd + '] was undefined');
                } else {
                    var req = new Request(cmd, token, protobuf, ctrlSchema, entry.schemaName);
                    for(i = 0; i < entry.components.length; i++) {
                        entry.components[i].add(req);
                    }
                }
            } catch (err) {
                print('onCtrlServerCmd: Unknown cmd=' + cmd + ' err=' + err);
                sendCtrlRequestComplete(RIL_E_REQUEST_NOT_SUPPORTED, cmd, token);
            }
        }
        print('onCtrlServerCmd X cmd=' + cmd + ' token=' + token);
    } catch (err) {
        print('onCtrlServerCmd X Exception err=' + err);
    }
}

function isCtrlServerDispatchCommand(cmd) {
    return (cmd > CTRL_CMD_DISPATH_BASE)
}

/**
 * Dispatch table for request, the control server will send those requests to
 * the corresponding components.
 *
 * Each table entry is index by the CTRL_CMD_xxxx
 * and contains an array of components this request
 * is to be sent to and the name of the schema
 * that converts the incoming protobuf to the
 * appropriate request data.
 *
 * ctrlServerDispatchTable[CTRL_CMD_xxx].components = Array of components
 * ctrlServerDisptachTable[CTRL_CMD_xxx].Entry.schemaName = 'Name-of-schema';
 */
var ctrlServerDispatchTable = new Array();

ctrlServerDispatchTable[CTRL_CMD_SET_MT_CALL] = { // 1001
    'components' : [simulatedRadioWorker],
    'schemaName' : 'CtrlReqSetMTCall',
};
ctrlServerDispatchTable[CTRL_CMD_HANGUP_CONN_REMOTE] = { // 1002
    'components' : [simulatedRadioWorker],
    'schemaName' : 'CtrlHangupConnRemote',
};
ctrlServerDispatchTable[CTRL_CMD_SET_CALL_TRANSITION_FLAG] = { // 1003
    'components' : [simulatedRadioWorker],
    'schemaName' : 'CtrlSetCallTransitionFlag',
};
ctrlServerDispatchTable[CTRL_CMD_SET_CALL_ALERT] = { // 1004
    'components' : [simulatedRadioWorker],
};
ctrlServerDispatchTable[CTRL_CMD_SET_CALL_ACTIVE] = { // 1005
    'components' : [simulatedRadioWorker],
};
ctrlServerDispatchTable[CTRL_CMD_ADD_DIALING_CALL] = { // 1006
    'components' : [simulatedRadioWorker],
    'schemaName' : 'CtrlReqAddDialingCall',
};

/**
 * Optional tests
 */
if (false) {
    include("ctrl_server_tests.js");
}

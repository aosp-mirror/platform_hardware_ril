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
 * @fileoverview Mock Radio Interface Layer (RIL) used for testing
 *
 * The following routines are defined in c++:
 *
 * Print a string to android log
 *   print(string)
 *
 * Read a file to a string.
 *   String readFileToString(String fileName)
 *
 * Read a file to a Buffer.
 *   Buffer readFileToBuffer(String fileName)
 *
 * Send an response unsolicited response to the framework.
 *   sendRilUnsolicitedResponse(Number responseNum, Buffer responseProtobuf)
 *
 * Send a completion request to the framework.
 *   sendRilRequestComplete(Number rilErrCode, Number reqNum,
 *                              String token, Buffer responseProtobuf)
 *
 * Send a complete request to the controller.
 *   sendCtrlRequestComplete(Number ctrlStatus, Number reqNum,
 *                              String token, Buffer responseProtobuf)
 *
 * Include the javascript file.
 *   include(string)
 *
 * The following objects are defined in c++
 *
 * Buffer is defined in node_buffer and provides a wrapper
 * for a buffer that can be shared between c++ and js.
 *   Buffer(length)
 *     Buffer::length()
 *     Buffer::data()
 *
 * Schema is defined in protobuf_v8 and converts between
 * a buffer and an object. A protobuf descriptor, ril.desc
 * and ctrl.desc, is used to drive the conversation.
 *     Object Schema::parse(Buffer protobuf)
 *     Buffer Schema::serialize(object)
 *
 * Worker is a thread which receives messages to be handled.
 * It is passed a function which is called once for each
 * message as it arrives. Call the add method to queue up
 * requests for the worker function to process.
 *   Object Worker(function (req))
 *      Worker::add(req);
 */

/**
 * Globals
 */

include("ril_vars.js");

var NULL_RESPONSE_STRING = '*magic-null*';

// The state of the radio, needed by currentState()
var gRadioState = RADIOSTATE_UNAVAILABLE;

// The state of the screen
var gScreenState = 0;

// The base band version
var gBaseBandVersion = 'mock-ril 0.1';

// define a global variable to access the global object
var globals = this;

// Empty Protobuf, defined here so we don't have
// to recreate an empty Buffer frequently
var emptyProtobuf = new Buffer();

// Get the ril description file and create a schema
var packageNameAndSeperator = 'ril_proto.';
var rilSchema = new Schema(readFileToBuffer('ril.desc'));
var ctrlSchema = new Schema(readFileToBuffer('ctrl.desc'));

/**
 * Print properties of an object
 */
function printProperties(obj, maxDepth, depth) {
    if (typeof maxDepth == 'undefined') {
        maxDepth = 1;
    }
    if (typeof depth == 'undefined') {
        depth = 1;
    }
    if (depth == 1) {
        print('printProperties:');
    }
    for (var property in obj) {
        try {
            if ((typeof obj[property] == 'object')
                    && (depth < maxDepth)) {
                printProperties(obj[property], maxDepth, depth+1);
            } else {
                print(depth + ': ' + property + '=' + obj[property] +
                        ' type=' + typeof obj[property]);
            }
        } catch (err) {
            print('err=' + err)
        }
    }
}

// Test printProperties
if (false) {
    var myObject = { 'field1' : '1', 'field2' : '2', 'hello' : [ 'hi', 'there' ] };
    printProperties(myObject, 3);
}

/**
 * Include the components
 */

include("simulated_radio.js");
include("simulated_icc.js");
include("ctrl_server.js");

/**
 * Construct a new request which is passed to the
 * Worker handler method.
 */
function Request(reqNum, token, protobuf, schema, schemaName) {
    this.reqNum = reqNum;
    this.token = token;
    try {
        this.data = schema[packageNameAndSeperator + schemaName].parse(protobuf);
    } catch (err) {
        // not a valid protobuf in the request
        this.data = null;
    }
}

/**
 * Dispatch incoming requests from RIL to the appropriate component.
 */
function onRilRequest(reqNum, token, requestProtobuf) {
    try {
        //print('onRilRequest E: reqNum=' + reqNum + ' token=' + token);

        /**
         * Validate parameters
         */
        rilErrCode = RIL_E_SUCCESS;
        if (typeof reqNum != 'number') {
            print('onRilRequest: reqNum is not a number');
            rilErrCode = RIL_E_GENERIC_FAILURE;
        }
        if (typeof token != 'number') {
            print('onRilRequest: token is not a number');
            rilErrCode = RIL_E_GENERIC_FAILURE;
        }
        if (typeof requestProtobuf != 'object') {
            print('onRilRequest: requestProtobuf is not an object');
            rilErrCode = RIL_E_GENERIC_FAILURE;
        }
        if (rilErrCode != RIL_E_SUCCESS) {
            sendRilRequestComplete(rilErrCode, reqNum, token);
            return 'onRilRequest X: invalid parameter';
        }

        try {
            //print('onRilRequest: get entry from dispatchTable reqNum=' + reqNum);
            entry = dispatchTable[reqNum];
            if (typeof entry == 'undefined') {
                throw ('entry = dispatchTable[' + reqNum + '] was undefined');
            } else {
                req = new Request(reqNum, token, requestProtobuf, rilSchema, entry.schemaName);
                for(i = 0; i < entry.components.length; i++) {
                    entry.components[i].add(req);
                }
            }
        } catch (err) {
            print('onRilRequest: Unknown reqNum=' + reqNum + ' err=' + err);
            sendRilRequestComplete(RIL_E_REQUEST_NOT_SUPPORTED, reqNum, token);
        }
        // print('onRilRequest X: reqNum=' + reqNum + ' token=' + token);
    } catch (err) {
        print('onRilRequest X: Exception err=' + err);
        return('onRilRequest X: Exception err=' + err);
    }
    return 'onRilRequest X';
}

function onUnsolicitedTick(tick) {
    print('onUnsolicitedTick EX tick=' + tick);
    return 3;
}

/**
 * Dispatch table for requests
 *
 * Each table entry is index by the RIL_REQUEST_xxxx
 * and contains an array of components this request
 * is to be sent to and the name of the schema
 * that converts the incoming protobuf to the
 * appropriate request data.
 *
 * DispatchTable[RIL_REQUEST_xxx].components = Array of components
 * DisptachTable[RIL_REQUEST_xxx].Entry.schemaName = 'Name-of-schema';
 */
var dispatchTable = new Array();

dispatchTable[RIL_REQUEST_GET_SIM_STATUS] = { // 1
    'components' : [simulatedIccWorker],
    'schemaName' : 'ReqGetSimStatus',
};
dispatchTable[RIL_REQUEST_ENTER_SIM_PIN] = { // 2
    'components' : [simulatedIccWorker],
    'schemaName' : 'ReqEnterSimPin',
};
dispatchTable[RIL_REQUEST_GET_CURRENT_CALLS] = { // 9
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_DIAL] = { // 10
    'components' : [simulatedRadioWorker],
    'schemaName' : 'ReqDial',
};
dispatchTable[RIL_REQUEST_GET_IMSI] = { // 11
    'components' : [simulatedIccWorker],
};
dispatchTable[RIL_REQUEST_HANGUP] = { // 12
    'components' : [simulatedRadioWorker],
    'schemaName' : 'ReqHangUp',
};
dispatchTable[RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND] =  { // 13
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND] = { // 14
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE] = { // 15
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_CONFERENCE] = { // 16
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_LAST_CALL_FAIL_CAUSE] = { // 18
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_SIGNAL_STRENGTH]  = { // 19
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_VOICE_REGISTRATION_STATE] = { // 20
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_DATA_REGISTRATION_STATE] = { // 21
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_OPERATOR] = { // 22
    'components' : [simulatedIccWorker],
};
dispatchTable[RIL_REQUEST_GET_IMEI] = { // 38
    'components' : [simulatedIccWorker],
};
dispatchTable[RIL_REQUEST_GET_IMEISV] = { // 39
    'components' : [simulatedIccWorker],
};
dispatchTable[RIL_REQUEST_ANSWER] = { // 40
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE] = { // 45
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC] = { // 46
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_BASEBAND_VERSION] = { // 51
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_SEPARATE_CONNECTION] = { // 52
    'components' : [simulatedRadioWorker],
    'schemaName' : 'ReqSeparateConnection',
};
dispatchTable[RIL_REQUEST_SET_MUTE ] = { // 53
    'components' : [simulatedRadioWorker],
    'schemaName' : 'ReqSetMute',
};
dispatchTable[RIL_REQUEST_SCREEN_STATE] = { // 61
    'components' : [simulatedRadioWorker],
    'schemaName' : 'ReqScreenState',
};

/**
 * Start the mock rill after loading
 */
function startMockRil() {
    print("startMockRil E:");
    setRadioState(RADIOSTATE_SIM_READY);
    // send the signal strength after 5 seconds, wait until mock ril is started
    simulatedRadioWorker.addDelayed({
      'reqNum' : CMD_UNSOL_SIGNAL_STRENGTH}, 5000);
    print("startMockRil X:");
}

/**
 * Optional tests
 */
if (false) {
    include("mock_ril_tests.js");
}

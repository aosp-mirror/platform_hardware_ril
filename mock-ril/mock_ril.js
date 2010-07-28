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

// Initial value of radioState
radioState = RADIO_STATE_UNAVAILABLE;

// The state of the screen
var screenState = 0;

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

dispatchTable[RIL_REQUEST_GET_SIM_STATUS] = {
    'components' : [simulatedIccWorker],
    'schemaName' : 'ReqGetSimStatus',
},
dispatchTable[RIL_REQUEST_ENTER_SIM_PIN] = {
    'components' : [simulatedIccWorker],
    'schemaName' : 'ReqEnterSimPin',
},
dispatchTable[RIL_REQUEST_HANGUP] = {
    'components' : [simulatedRadioWorker],
    'schemaName' : 'ReqHangUp',
};
dispatchTable[RIL_REQUEST_SCREEN_STATE] = {
    'components' : [simulatedRadioWorker],
    'schemaName' : 'ReqScreenState',
};
dispatchTable[RIL_REQUEST_OPERATOR] = {
    'components' : [simulatedIccWorker],
};
dispatchTable[RIL_REQUEST_GPRS_REGISTRATION_STATE] = {
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_REGISTRATION_STATE] = {
    'components' : [simulatedRadioWorker],
};
dispatchTable[RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE] = {
    'components' : [simulatedRadioWorker],
};

/**
 * Construct a new request which is passed to the
 * Worker handler method.
 */
function Request(reqNum, token, protobuf, schema, schemaName) {
    req = new Object();
    req.reqNum = reqNum;
    req.token = token;
    try {
        req.data = schema[packageNameAndSeperator + schemaName].parse(protobuf);
    } catch (err) {
        // not a valid protobuf in the request
        req.data = null;
    }
    return req;
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
            print('onRilRequest: get entry from dispatchTable reqNum=' + reqNum);
            entry = dispatchTable[reqNum];
            req = new Request(reqNum, token, requestProtobuf, rilSchema, entry.schemaName);
            for(i = 0; i < entry.components.length; i++) {
                entry.components[i].add(req);
            }
        } catch (err) {
            print('onRilRequest: Unknown reqNum=' + reqNum + ' err=' + err);
            sendRilRequestComplete(RIL_E_REQUEST_NOT_SUPPORTED, reqNum, token);
        }
        // print('onRilRequest X: reqNum=' + reqNum + ' token=' + token);
    } catch (err) {
        print('onRilRequest X: Exception err=' + err);
    }
    return 'onRilRequest X';
}

function onUnsolicitedTick(tick) {
    print('onUnsolicitedTick EX tick=' + tick);
    return 3;
}

function setRadioState(newRadioState) {
    radioState = newRadioState;
    sendRilUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED);
}

/**
 * Start the mock rill after loading
 */
function startMockRil() {
    print("startMockRil E:");
    setRadioState(RADIO_STATE_SIM_READY);
    print("startMockRil X:");
}


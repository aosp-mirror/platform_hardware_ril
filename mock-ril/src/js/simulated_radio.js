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
 * Simulated Radio
 */
function SimulatedRadio() {
    var registrationState = '1';
    var lac = '0';
    var cid = '0';
    var radioTechnology = '3';
    var baseStationId = NULL_RESPONSE_STRING;
    var baseStationLatitude = NULL_RESPONSE_STRING;
    var baseStationLongitude = NULL_RESPONSE_STRING;
    var concurrentServices = NULL_RESPONSE_STRING;
    var systemId  = NULL_RESPONSE_STRING;
    var networkId = NULL_RESPONSE_STRING;
    var roamingIndicator = NULL_RESPONSE_STRING;
    var prlActive = NULL_RESPONSE_STRING;
    var defaultRoamingIndicator = NULL_RESPONSE_STRING;
    var registrationDeniedReason = NULL_RESPONSE_STRING;
    var primaryScrambingCode = NULL_RESPONSE_STRING;

    var networkSelectionMode = 0;

    // The result returned by the request handlers
    var result = new Object();

    this.rilRequestHangUp = function(req) {
        print('rilRequestHangUp data.connection_index=' + req.data.connectionIndex);
        return result;
    }

    this.rilRequestScreenState = function (req) {
        print('rilRequestScreenState: req.data.state=' + req.data.state);
        gScreenState = req.data.state;
        return result;
    }

    this.rilRequestGprsRegistrationState = function(req) {
        print('rilRequestGprsRegistrationState E');

        var rsp = Object();
        rsp.strings = Array();
        rsp.strings[0] = registrationState;
        rsp.strings[1] = lac;
        rsp.strings[2] = cid;
        rsp.strings[3] = radioTechnology;

        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspStrings'].serialize(rsp);
        return result;
    }

    this.rilRequestRegistrationState = function(req) {
        print('rilRequestRegistrationState');

        var rsp = Object();
        rsp.strings = Array();
        rsp.strings[0] = registrationState;
        rsp.strings[1] = lac;
        rsp.strings[2] = cid;
        rsp.strings[3] = radioTechnology;
        rsp.strings[4] = baseStationId;
        rsp.strings[5] = baseStationLatitude;
        rsp.strings[6] = baseStationLongitude;
        rsp.strings[7] = concurrentServices;
        rsp.strings[8] = systemId;
        rsp.strings[9] = networkId;
        rsp.strings[10] = roamingIndicator;
        rsp.strings[11] = prlActive;
        rsp.strings[12] = defaultRoamingIndicator;
        rsp.strings[13] = registrationDeniedReason;
        rsp.strings[14] = primaryScrambingCode;

        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspStrings'].serialize(rsp);
        return result;
    }

    this.rilRequestQueryNeworkSelectionMode = function(req) {
        print('rilRequestQueryNeworkSelectionMode');

        var rsp = Object();
        rsp.integers = Array();
        rsp.integers[0] = networkSelectionMode;

        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspIntegers'].serialize(rsp);
        return result;
    }

    /**
     * Process the request
     */
    this.process = function(req) {
        try {
            print('SimulatedRadio E: req.reqNum=' + req.reqNum + ' req.token=' + req.token);

            // Assume the result will be true, successful and nothing to return
            result.sendResponse = true;
            result.rilErrCode = RIL_E_SUCCESS;
            result.responseProtobuf = emptyProtobuf;

            try {
                result = this.radioDispatchTable[req.reqNum](req);
            } catch (err) {
                print('SimulatedRadio: Unknown reqNum=' + req.reqNum);
                result.rilErrCode = RIL_E_REQUEST_NOT_SUPPORTED;
            }

            if (result.sendResponse) {
                sendRilRequestComplete(result.rilErrCode, req.reqNum,
                        req.token, result.responseProtobuf);
            }

            print('SimulatedRadio X: req.reqNum=' + req.reqNum + ' req.token=' + req.token);
        } catch (err) {
            print('SimulatedRadio X: Exception req.reqNum=' +
                    req.reqNum + ' req.token=' + req.token + ' err=' + err);
        }
    }

    print('SimulatedRadio() ctor E');
    this.radioDispatchTable = new Array();
    this.radioDispatchTable[RIL_REQUEST_HANGUP] = // 12
                this.rilRequestHangUp;
    this.radioDispatchTable[RIL_REQUEST_REGISTRATION_STATE] = // 20
                this.rilRequestRegistrationState;
    this.radioDispatchTable[RIL_REQUEST_GPRS_REGISTRATION_STATE] = // 21
                this.rilRequestGprsRegistrationState;
    this.radioDispatchTable[RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE] = // 45
                this.rilRequestQueryNeworkSelectionMode;
    this.radioDispatchTable[RIL_REQUEST_SCREEN_STATE] = // 61
                this.rilRequestScreenState;
    print('SimulatedRadio() ctor X');
}

// The simulated radio instance and its associated Worker
var simulatedRadio = new SimulatedRadio()
var simulatedRadioWorker = new Worker(function (req) {
    simulatedRadio.process(req);
});
simulatedRadioWorker.run();


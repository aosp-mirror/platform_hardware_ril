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
function Radio() {
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

    var NETWORK_SELECTION_MODE_AUTOMATIC = 0;
    var NETWORK_SELECTION_MODE_MANUAL = 1;
    var networkSelectionMode = NETWORK_SELECTION_MODE_AUTOMATIC;

    // The result returned by the request handlers
    var result = new Object();

    this.rilRequestHangUp = function(req) { // 12
        print('Radio: rilRequestHangUp data.connection_index=' + req.data.connectionIndex);
        return result;
    }

    this.rilRequestRegistrationState = function(req) { // 20
        print('Radio: rilRequestRegistrationState');

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

    this.rilRequestGprsRegistrationState = function(req) { // 21
        print('Radio: rilRequestGprsRegistrationState');

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

    this.rilRequestQueryNeworkSelectionMode = function(req) { // 45
        print('Radio: rilRequestQueryNeworkSelectionMode');

        var rsp = Object();
        rsp.integers = Array();
        rsp.integers[0] = networkSelectionMode;

        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspIntegers'].serialize(rsp);
        return result;
    }

    this.rilRequestSetNeworkSelectionAutomatic = function(req) { // 46
        print('Radio: rilRequestSetNeworkSelectionAutomatic');

        networkSelectionMode = NETWORK_SELECTION_MODE_AUTOMATIC;

        return result;
    }

    this.rilRequestBaseBandVersion = function (req) { // 51
        print('Radio: rilRequestBaseBandVersion');
        var rsp = Object();
        rsp.strings = Array();
        rsp.strings[0] = gBaseBandVersion;

        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspStrings'].serialize(rsp);
        return result;
    }

    this.rilRequestScreenState = function (req) { // 61
        print('Radio: rilRequestScreenState: req.data.state=' + req.data.state);
        screenState = req.data.state;
        return result;
    }
    /**
     * Process the request
     */
    this.process = function(req) {
        try {
            //print('Radio E: req.reqNum=' + req.reqNum + ' req.token=' + req.token);

            // Assume the result will be true, successful and nothing to return
            result.sendResponse = true;
            result.rilErrCode = RIL_E_SUCCESS;
            result.responseProtobuf = emptyProtobuf;

            try {
                result = this.radioDispatchTable[req.reqNum](req);
            } catch (err) {
                print('Radio: Unknown reqNum=' + req.reqNum);
                result.rilErrCode = RIL_E_REQUEST_NOT_SUPPORTED;
            }

            if (result.sendResponse) {
                sendRilRequestComplete(result.rilErrCode, req.reqNum,
                        req.token, result.responseProtobuf);
            }

            //print('Radio X: req.reqNum=' + req.reqNum + ' req.token=' + req.token);
        } catch (err) {
            print('Radio: Exception req.reqNum=' +
                    req.reqNum + ' req.token=' + req.token + ' err=' + err);
        }
    }

    print('Radio: constructor E');
    this.radioDispatchTable = new Array();
    this.radioDispatchTable[RIL_REQUEST_HANGUP] = // 12
                this.rilRequestHangUp;
    this.radioDispatchTable[RIL_REQUEST_REGISTRATION_STATE] = // 20
                this.rilRequestRegistrationState;
    this.radioDispatchTable[RIL_REQUEST_GPRS_REGISTRATION_STATE] = // 21
                this.rilRequestGprsRegistrationState;
    this.radioDispatchTable[RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE] = // 45
                this.rilRequestQueryNeworkSelectionMode;
    this.radioDispatchTable[RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC] = // 46
        this.rilRequestSetNeworkSelectionAutomatic;
    this.radioDispatchTable[RIL_REQUEST_BASEBAND_VERSION] = // 51
                this.rilRequestBaseBandVersion;
    this.radioDispatchTable[RIL_REQUEST_SCREEN_STATE] = // 61
                this.rilRequestScreenState;
    print('Radio: constructor X');
}

// The simulated radio instance and its associated Worker
var simulatedRadio = new Radio()
var simulatedRadioWorker = new Worker(function (req) {
    simulatedRadio.process(req);
});
simulatedRadioWorker.run();


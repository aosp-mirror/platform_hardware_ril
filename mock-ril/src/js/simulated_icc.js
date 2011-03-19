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
 * Simulated Icc
 */
function Icc() {

    var MCC = '310';
    var MNC = '260';
    var MSN = '123456789';
    var IMEI = '123456789012345';
    var IMEISV = '00';

    function RilAppStatus(type, state, persoState, aidPtr, appLabelPtr, pin1R, curPin1, curPin2) {
        this.appType = type;
        this.appState = state;
        this.persoSubstate = persoState;
        this.aid = aidPtr;
        this.appLabel = appLabelPtr;
        this.pin1Replaced = pin1R;
        this.pin1 = curPin1;
        this.pint2 = curPin2;
    }

    function RilCardStatus() {
        this.cardState = CARDSTATE_PRESENT;
        this.universalPinState = PINSTATE_UNKNOWN;
        this.gsmUmtsSubscriptionAppIndex = 0;
        this.cdmaSubscriptionAppIndex = CARD_MAX_APPS;
        this.imsSubscriptionAppIndex = CARD_MAX_APPS;
        this.numApplications = 1;
        this.applications = new Array(CARD_MAX_APPS);

        // Initialize application status
        for (i = 0; i < CARD_MAX_APPS; i++) {
            var app = new RilAppStatus(APPTYPE_UNKNOWN, APPSTATE_UNKNOWN, PERSOSUBSTATE_UNKNOWN,
                                       null, null, 0, PINSTATE_UNKNOWN, PINSTATE_UNKNOWN);
            this.applications[i] = app;
        }

        // set gsm application status.
        var gsmApp = new RilAppStatus(APPTYPE_SIM, APPSTATE_READY, PERSOSUBSTATE_READY, null, null,
                                     0, PINSTATE_UNKNOWN, PINSTATE_UNKNOWN);
        this.applications[this.gsmUmtsSubscriptionAppIndex] = gsmApp;
    }

    var cardStatus = new RilCardStatus();

    // The result returned by the request handlers
    var result = new Object();

    this.rilRequestGetSimStatus = function(req) { // 1
        print('Icc: rilRequestGetSimStatus');

        var rsp = new Object();
        rsp.cardStatus = cardStatus;

        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspGetSimStatus'].serialize(rsp);
        return result;
    }

    this.rilRequestEnterSimPin = function(req) { // 2
        print('Icc: rilRequestEnterSimPin req.data.pin=' + req.data.pin);

        var rsp = new Object();
        rsp.retriesRemaining = 3;
        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspEnterSimPin'].serialize(rsp);
        return result;
    }

    this.rilRequestGetImsi = function(req) { // 11
        print('Icc: rilRequestGetImsi');

        var rsp = new Object();
        rsp.strings = new Array();
        rsp.strings[0] = MCC + MNC + MSN;
        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspStrings'].serialize(rsp);
        return result;
    }

    this.rilRequestOperator = function(req) { // 22
        print('Icc: rilRequestOperator');

        var rsp = new Object();
        rsp.longAlphaOns = 'Mock-Ril long Alpha Ons';
        rsp.shortAlphaOns = 'Mock-Ril';
        rsp.mccMnc = MCC + MNC;
        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspOperator'].serialize(rsp);
        return result;
    }

    this.rilRequestGetImei = function(req) { // 38
        print('Icc: rilRequestGetImei');

        var rsp = new Object();
        rsp.strings = new Array();
        rsp.strings[0] = IMEI;
        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspStrings'].serialize(rsp);
        return result;
    }

    this.rilRequestGetImeisv = function(req) { // 39
        print('Icc: rilRequestGetImeisv');

        var rsp = new Object();
        rsp.strings = new Array();
        rsp.strings[0] = IMEISV;
        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspStrings'].serialize(rsp);
        return result;
    }

    /**
     * Process the request
     */
    this.process = function(req) {
        try {
            // print('Icc E: req.reqNum=' + req.reqNum + ' req.token=' + req.token);

            // Assume the result will be true, successful and nothing to return
            result.sendResponse = true;
            result.rilErrCode = RIL_E_SUCCESS;
            result.responseProtobuf = emptyProtobuf;

            try {
                result = (this.simDispatchTable[req.reqNum]).call(this, req);
            } catch (err) {
                print('Icc: Unknown reqNum=' + req.reqNum);
                result.rilErrCode = RIL_E_REQUEST_NOT_SUPPORTED;
            }

            if (result.sendResponse) {
                sendRilRequestComplete(result.rilErrCode, req.reqNum,
                        req.token, result.responseProtobuf);
            }

            // print('Icc X: req.reqNum=' + req.reqNum + ' req.token=' + req.token);
        } catch (err) {
            print('Icc X: Exception req.reqNum=' +
                    req.reqNum + ' req.token=' + req.token + ' err=' + err);
        }
    }

    print('Icc: constructor E');
    this.simDispatchTable = new Array();
    this.simDispatchTable[RIL_REQUEST_GET_SIM_STATUS] = this.rilRequestGetSimStatus; // 1
    this.simDispatchTable[RIL_REQUEST_ENTER_SIM_PIN] = this.rilRequestEnterSimPin; // 2
    this.simDispatchTable[RIL_REQUEST_GET_IMSI] = this.rilRequestGetImsi; // 11
    this.simDispatchTable[RIL_REQUEST_OPERATOR] = this.rilRequestOperator; // 22
    this.simDispatchTable[RIL_REQUEST_GET_IMEI] = this.rilRequestGetImei; // 38
    this.simDispatchTable[RIL_REQUEST_GET_IMEISV] = this.rilRequestGetImeisv; // 39
    print('Icc: constructor X');
}

// The simulated sim instance and its associated Worker
var simulatedIcc = new Icc();
var simulatedIccWorker = new Worker(function (req) {
    simulatedIcc.process(req);
});
simulatedIccWorker.run();

/**
 * Optional tests
 */
if (false) {
    include("simulated_icc_tests.js");
}

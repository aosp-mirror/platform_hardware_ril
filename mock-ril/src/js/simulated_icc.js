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
function SimulatedIcc() {

     function RilCardStatus() {
        this.cardState = 0;
        this.universalPinState = 0;
        this.gsmUmtsSubscriptionAppIndex = 0;
        this.cdmaSubscriptionAppIndex = 0;
        this.numApplications = 0;
    }

    var cardStatus = new RilCardStatus();

    // The result returned by the request handlers
    var result = new Object();

    this.rilRequestGetSimStatus = function(req) {
        print('rilRequestGetSimStatus');

        var rsp = new Object();
        rsp.cardStatus = cardStatus;
        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspGetSimStatus'].serialize(rsp);
        return result;
    }

    this.rilRequestEnterSimPin = function(req) {
        print('rilRequestEnterSimPin req.data.pin=' + req.data.pin);

        var rsp = Object();
        rsp.retriesRemaining = 3;
        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspEnterSimPin'].serialize(rsp);
        return result;
    }

    this.rilRequestOperator = function(req) {
        print('rilRequestOperator');

        var rsp = Object();
        rsp.longAlphaOns = 'Mock-Ril long Alpha Ons';
        rsp.shortAlphaOns = 'Mock-Ril';
        rsp.mccMnc = '310260';
        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspOperator'].serialize(rsp);
        return result;
    }

    /**
     * Process the request
     */
    this.process = function(req) {
        try {
            print('SimulatedIcc E: req.reqNum=' + req.reqNum + ' req.token=' + req.token);

            // Assume the result will be true, successful and nothing to return
            result.sendResponse = true;
            result.rilErrCode = RIL_E_SUCCESS;
            result.responseProtobuf = emptyProtobuf;

            try {
                result = this.simDispatchTable[req.reqNum](req);
            } catch (err) {
                print('SimulatedIcc: Unknown reqNum=' + reqNum);
                result.rilErrCode = RIL_E_REQUEST_NOT_SUPPORTED;
            }

            if (result.sendResponse) {
                sendRilRequestComplete(result.rilErrCode, req.reqNum,
                        req.token, result.responseProtobuf);
            }

            print('SimulatedIcc X: req.reqNum=' + req.reqNum + ' req.token=' + req.token);
        } catch (err) {
            print('SimulatedIcc X: Exception req.reqNum=' +
                    req.reqNum + ' req.token=' + req.token + ' err=' + err);
        }
    }

    print('SimulatedIcc() ctor E');
    this.simDispatchTable = new Array();
    this.simDispatchTable[RIL_REQUEST_GET_SIM_STATUS] = this.rilRequestGetSimStatus; // 1
    this.simDispatchTable[RIL_REQUEST_ENTER_SIM_PIN] = this.rilRequestEnterSimPin; // 2
    this.simDispatchTable[RIL_REQUEST_OPERATOR] = this.rilRequestOperator; // 22
    print('SimulatedIcc() ctor X');
}

// The simulated sim instance and its associated Worker
var simulatedIcc = new SimulatedIcc()
var simulatedIccWorker = new Worker(function (req) {
    simulatedIcc.process(req);
});
simulatedIccWorker.run();


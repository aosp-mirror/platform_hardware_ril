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
 * The Radio object contains a set of methods and objects to handle ril request
 * which is passed from simulatedRadioWorker queue. The global object initialize
 * an instance of Radio object by calling "new Radio". For each ril request,
 * rilDispatchTable gets searched and the corresponding method is called.
 * Extra requests are also defined to process unsolicited rerequests.
 *
 * The rilDispatchTable is an array indexed by RIL_REQUEST_* or REQUEST_UNSOL_*,
 * in which each request corresponds to a functions defined in the Radio object.
 * We need to pay attention when using "this" within those functions. When they are
 * called in "this.process" using
 *               result = this.radioDispatchTable[req.reqNum])(req);
 * this scope of "this" within those functions are the radioDispatchTable, not the
 * object that "this.process" belongs to. Using "this." to access other
 * functions in the object may cause trouble.
 * To avoid that, the object is passed in when those functions are called as
 * shown in the following:
 *                 result = (this.radioDispatchTable[req.reqNum]).call(this, req);
 */

/**
 * Set radio state
 */
function setRadioState(newState) {
    newRadioState = newState;
    if (typeof newState == 'string') {
        newRadioState = globals[newState];
        if (typeof newRadioState == 'undefined') {
            throw('setRadioState: Unknow string: ' + newState);
        }
    }
    if ((newRadioState < RADIOSTATE_OFF) || (newRadioState > RADIOSTATE_NV_READY)) {
        throw('setRadioState: newRadioState: ' + newRadioState + ' is invalid');
    }
    gRadioState = newRadioState;
    sendRilUnsolicitedResponse(RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED);
}

/**
 * Create a call.
 *
 * @return a RilCall
 */
function RilCall(state, phoneNumber, callerName) {
    this.state = state;
    this.index = 0;
    this.toa = 0;
    this.isMpty = false;
    this.isMt = false;
    this.als = 0;
    this.isVoice = true;
    this.isVoicePrivacy = false;
    this.number = phoneNumber;
    this.numberPresentation = 0;
    this.name = callerName;
}

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

    var muteState = 0;   // disable mute

    // Number of active calls in calls
    var numberActiveCalls = 0;

    // Maximum number of active calls
    var maxNumberActiveCalls = 7;

    // Array of "active" calls
    var calls = Array();

    // The result returned by the request handlers
    var result = new Object();

    function GWSignalStrength() {
        this.signalStrength = 10;  // 10 * 2 + (-113) = -93dBm, make it three bars
        this.bitErrorRate = 0;
    }

    function CDMASignalStrength() {
        this.dbm = -1;
        this.ecio = -1;
    }

    function EVDOSignalStrength() {
        this.dbm = -1;
        this.ecio = -1;
        this.signalNoiseRatio = 0;
    }

    var gwSignalStrength = new GWSignalStrength;
    var cdmaSignalStrength = new CDMASignalStrength();
    var evdoSignalStrength = new EVDOSignalStrength();

    /**
     * The the array of calls, this is a sparse
     * array and some elements maybe 'undefined'.
     *
     * @return Array of RilCall's
     */
    this.getCalls =  function() {
        return calls;
    }

    /**
     * @return the RilCall at calls[index] or null if undefined.
     */
    this.getCall = function(index) {
        var c = null;
        try {
            c = calls[index];
            if (typeof c == 'undefined') {
                c = null;
            }
        } catch (err) {
            c = null;
        }
        return c;
    }

    /** Add an active call
     *
     * @return a RilCall or null if too many active calls.
     */
    this.addCall = function(state, phoneNumber, callerName) {
        print('Radio: addCall');
        var c = null;
        if (numberActiveCalls < maxNumberActiveCalls) {
            numberActiveCalls += 1;
            c = new RilCall(state, phoneNumber, callerName);
            // call index starts with 1, the call index matches its index in the array
            index = calls.length + 1;
            c.index = index;
            calls[index] = c;
        }
        return c;
    }

    /**
     * Remove the call, does nothing if the call is undefined.
     *
     * @param index into calls to remove.
     * @return the call removed or null if did not exist
     */
    this.removeCall =  function(index) {
        var c = null;
        if ((numberActiveCalls > 0)
                 && (index < calls.length)
                 && (typeof calls[index] != 'undefined')) {
            c = calls[index];
            delete calls[index];
            numberActiveCalls -= 1;
            if (numberActiveCalls == 0) {
                calls = new Array();
            }
        } else {
            c = null;
        }
        return c;
    }

    /**
     * Print the call
     *
     * @param c is the RilCall to print
     */
    this.printCall = function(c) {
        if ((c == null) || (typeof c == 'undefined')) {
            print('c: undefined');
        } else {
            print('c[' + c.index + ']: index=' + c.index + ' state=' + c.state +
                  ' number=' + c.number + ' name=' + c.name);
        }
    }

    /**
     * Print all the calls.
     *
     * @param callArray is an Array of RilCall's
     */
    this.printCalls = function(callArray) {
        if (typeof callArray == 'undefined') {
            callArray = calls;
        }
        print('callArray.length=' + callArray.length);
        for (var i = 0; i < callArray.length; i++) {
            this.printCall(callArray[i]);
        }
    }

    /**
     * Print signal strength
     */
    this.printSignalStrength = function() {
        print('rssi: '         + gwSignalStrength.signalStrength);
        print('bitErrorRate: ' + gwSignalStrength.bitErrorRate);
        print('cdmaDbm: '  +  cdmaSignalStrength.dbm);
        print('cdmaEcio: ' + cdmaSignalStrength.ecio);
        print('evdoRssi: ' + evdoSignalStrength.dbm);
        print('evdoEcio: ' + evdoSignalStrength.ecio);
        print('evdoSnr: '  + evdoSignalStrength.signalNoiseRatio);
    }

    /**
     * set signal strength
     *
     * @param rssi and bitErrorRate are signal strength parameters for GSM
     *        cdmaDbm, cdmaEcio, evdoRssi, evdoEcio, evdoSnr are parameters for CDMA & EVDO
     */
    this.setSignalStrength = function(rssi, bitErrorRate, cdmaDbm, cdmaEcio, evdoRssi,
                                      evdoEcio, evdoSnr) {
        print('setSignalStrength E');

        if (rssi != 99) {
            if ((rssi < 0) || (rssi > 31)) {
                throw ('not a valid signal strength');
            }
        }
        // update signal strength
        gwSignalStrength.signalStrength = rssi;
        gwSignalStrength.bitErrorRate = bitErrorRate;
        cdmaSignalStrength.dbm = cdmaDbm;
        cdmaSignalStrength.ecio = cdmaEcio;
        evdoSignalStrength.dbm = evdoRssi;
        evdoSignalStrength.ecio = evdoEcio;
        evdoSignalStrength.signalNoiseRatio = evdoSnr;

        // pack the signal strength into RspSignalStrength and send a unsolicited response
        var rsp = new Object();

        rsp.gwSignalstrength = gwSignalStrength;
        rsp.cdmSignalstrength = cdmaSignalStrength;
        rsp.evdoSignalstrength = evdoSignalStrength;

        var response = rilSchema[packageNameAndSeperator +
                             'RspSignalStrength'].serialize(rsp);

        sendRilUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, response);

        // send the unsolicited signal strength every 1 minute.
        simulatedRadioWorker.addDelayed(
            {'reqNum' : CMD_UNSOL_SIGNAL_STRENGTH}, 60000);
        print('setSignalStrength X');
    }

    /**
     * Handle RIL_REQUEST_GET_CURRENT_CALL
     *
     * @param req is the Request
     */
    this.rilRequestGetCurrentCalls = function(req) { // 9
        print('Radio: rilRequestGetCurrentCalls E');
        var rsp = new Object();

        // pack calls into rsp.calls
        rsp.calls = new Array();
        var i;
        var j;
        for (i = 0, j = 0; i < calls.length; i++) {
            if (typeof calls[i] != 'undefined') {
                rsp.calls[j++] = calls[i];
            }
        }
        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                            'RspGetCurrentCalls'].serialize(rsp);
        return result;
    }

    /**
     * Handle RIL_REQUEST_DIAL
     *
     * @param req is the Request
     */
    this.rilRequestDial = function(req) { // 10
        print('Radio: rilRequestDial E');
        var newCall = new Object();
        newCall = this.addCall(CALLSTATE_DIALING, req.data.address, '');
        if (newCall == null) {
            result.rilErrCode = RIL_E_GENERIC_FAILURE;
            return result;
        }
        this.printCalls(calls);

        print('after add the call');
        // Set call state to dialing
        simulatedRadioWorker.add(
                        {'reqNum' : CMD_CALL_STATE_CHANGE,
                         'callType' : OUTGOING,
                         'callIndex' : newCall.index,
                         'nextState' : CALLSTATE_DIALING});
        // Update call state to alerting after 1 second
       simulatedRadioWorker.addDelayed(
                        {'reqNum' : CMD_CALL_STATE_CHANGE,
                         'callType' : OUTGOING,
                         'callIndex' : newCall.index,
                         'nextState' : CALLSTATE_ALERTING}, 1000);
       // Update call state to active after 2 seconds
        simulatedRadioWorker.addDelayed(
                        {'reqNum' : CMD_CALL_STATE_CHANGE,
                         'callType' : OUTGOING,
                         'callIndex': newCall.index,
                         'nextState' : CALLSTATE_ACTIVE}, 2000);
        return result;
    }

    /**
     * Handle RIL_REQUEST_HANG_UP
     *
     * @param req is the Request
     */
    this.rilRequestHangUp = function(req) { // 12
        print('Radio: rilRequestHangUp data.connection_index=' + req.data.connectionIndex);
        if (this.removeCall(req.data.connectionIndex) == null) {
            result.rilErrCode = RIL_E_GENERIC_FAILURE;
            print('no connection to hangup');
        }
        return result;
    }

    /**
     * Handle RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND
     *        Hang up waiting or held
     *
     * @param req is the Request
     */
    this.rilRequestHangupWaitingOrBackground = function(req) { // 13
        print('Radio: rilRequestHangupWaitingOrBackground');
        if (numberActiveCalls <= 0) {
          result.rilErrCode = RIL_E_GENERIC_FAILURE;
        } else {
            for (var i = 0; i < calls.length; i++) {
                if (typeof calls[i] != 'undefined') {
                    switch (calls[i].state) {
                        case CALLSTATE_ACTIVE:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                        case CALLSTATE_HOLDING:
                            this.removeCall(i);
                            break;
                        case CALLSTATE_DIALING:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                        case CALLSTATE_ALERTING:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                        case CALLSTATE_INCOMING:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                        case CALLSTATE_WAITING:
                            this.removeCall(i);
                            break;
                        default:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                    }
                    this.printCalls(calls);
                    if(result.rilErrCode == RIL_E_GENERIC_FAILURE) {
                        return result;
                    }
                } // end of processing call[i]
            } // end of for
        }
        // Send out RIL_UNSOL_CALL_STATE_CHANGED after the request is returned
        simulatedRadioWorker.add(
          {'reqNum' : CMD_UNSOL_CALL_STATE_CHANGED});
        return result;
    }

    /**
     * Handle RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND
     *   release all active calls (if any exist) and resume held or waiting calls.
     * @param req is the Request
     */
    this.rilRequestHangUpForegroundResumeBackground = function(req) { //14
        print('Radio: rilRequestHangUpForegroundResumeBackground');
        if (numberActiveCalls <= 0) {
            result.rilErrCode = RIL_E_GENERIC_FAILURE;
        } else {
            for (var i = 0; i < calls.length; i++) {
                if (typeof calls[i] != 'undefined') {
                    switch (calls[i].state) {
                        case CALLSTATE_ACTIVE:
                            this.removeCall(i);
                            break;
                        case CALLSTATE_HOLDING:
                            calls[i].state = CALLSTATE_ACTIVE;
                            break;
                        case CALLSTATE_DIALING:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                        case CALLSTATE_ALERTING:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                        case CALLSTATE_INCOMING:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                        case CALLSTATE_WAITING:
                            calls[i].state = CALLSTATE_ACTIVE;
                            break;
                        default:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                    }
                    this.printCalls(calls);
                    if(result.rilErrCode == RIL_E_GENERIC_FAILURE) {
                        return result;
                    }
                } // end of processing call[i]
            }
        }
        // Send out RIL_UNSOL_CALL_STATE_CHANGED after the request is returned
        simulatedRadioWorker.add(
          {'reqNum' : CMD_UNSOL_CALL_STATE_CHANGED});
        return result;
    }

    /**
     * Handle RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE
     *
     *     BEFORE                               AFTER
     * Call 1   Call 2                 Call 1       Call 2
     * ACTIVE   HOLDING                HOLDING     ACTIVE
     * ACTIVE   WAITING                HOLDING     ACTIVE
     * HOLDING  WAITING                HOLDING     ACTIVE
     * ACTIVE   IDLE                   HOLDING     IDLE
     * IDLE     IDLE                   IDLE        IDLE
     *
     * @param req is the Request
     */
    this.rilRequestSwitchWaitingOrHoldingAndActive = function(req) {  // 15
        print('Radio: rilRequestSwitchWaitingOrHoldingAndActive');
        if (numberActiveCalls <= 0) {
            result.rilErrCode = RIL_E_GENERIC_FAILURE;
        } else {
            for (var i = 0; i < calls.length; i++) {
                if (typeof calls[i] != 'undefined') {
                    switch (calls[i].state) {
                        case CALLSTATE_ACTIVE:
                            calls[i].state = CALLSTATE_HOLDING;
                            break;
                        case CALLSTATE_HOLDING:
                            calls[i].state = CALLSTATE_ACTIVE;
                            break;
                        case CALLSTATE_DIALING:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                        case CALLSTATE_ALERTING:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                        case CALLSTATE_INCOMING:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                        case CALLSTATE_WAITING:
                            calls[i].state = CALLSTATE_ACTIVE;
                            break;
                        default:
                            result.rilErrCode = RIL_E_GENERIC_FAILURE;
                            break;
                    }
                    this.printCalls(calls);
                    if(result.rilErrCode == RIL_E_GENERIC_FAILURE) {
                        return result;
                    }
                } // end of processing call[i]
            } // end of for
        }
        // Send out RIL_UNSOL_CALL_STATE_CHANGED after the request is returned
        simulatedRadioWorker.add(
          {'reqNum' : CMD_UNSOL_CALL_STATE_CHANGED});
        return result;
    }

    /**
     * Handle RIL_REQUEST_SIGNAL_STRENGTH
     *
     * @param req is the Request
     */
    this.rilRequestSignalStrength = function(req) { // 19
        print('Radio: rilRequestSignalStrength E');
        var rsp = new Object();

        // pack the signal strength into RspSignalStrength
        rsp.gwSignalstrength = gwSignalStrength;
        rsp.cdmaSignalstrength = cdmaSignalStrength;
        rsp.evdoSignalstrength = evdoSignalStrength;

        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                            'RspSignalStrength'].serialize(rsp);
        return result;
    }

    /**
     * Handle RIL_REQUEST_REGISTRATION_STATE
     *
     * @param req is the Request
     */
    this.rilRequestRegistrationState = function(req) { // 20
        print('Radio: rilRequestRegistrationState');

        var rsp = new Object();
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

    /**
     * Handle RIL_REQUEST_GPRS_REGISTRATION_STATE
     *
     * @param req is the Request
     */
    this.rilRequestGprsRegistrationState = function(req) { // 21
        print('Radio: rilRequestGprsRegistrationState');

        var rsp = new Object();
        rsp.strings = Array();
        rsp.strings[0] = registrationState;
        rsp.strings[1] = lac;
        rsp.strings[2] = cid;
        rsp.strings[3] = radioTechnology;

        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspStrings'].serialize(rsp);
        return result;
    }

    /**
     * Handle RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE
     *
     * @param req is the Request
     */
    this.rilRequestQueryNeworkSelectionMode = function(req) { // 45
        print('Radio: rilRequestQueryNeworkSelectionMode');

        var rsp = new Object();
        rsp.integers = Array();
        rsp.integers[0] = networkSelectionMode;

        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspIntegers'].serialize(rsp);
        return result;
    }

    /**
     * Handle RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC
     *
     * @param req is the Request
     */
    this.rilRequestSetNeworkSelectionAutomatic = function(req) { // 46
        print('Radio: rilRequestSetNeworkSelectionAutomatic');

        networkSelectionMode = NETWORK_SELECTION_MODE_AUTOMATIC;

        return result;
    }

    /**
     * Handle RIL_REQUEST_BASE_BAND_VERSION
     *
     * @param req is the Request
     */
    this.rilRequestBaseBandVersion = function(req) { // 51
        print('Radio: rilRequestBaseBandVersion');
        var rsp = new Object();
        rsp.strings = Array();
        rsp.strings[0] = gBaseBandVersion;

        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                 'RspStrings'].serialize(rsp);
        return result;
    }

    /**
     * Handle RIL_REQUEST_SET_MUTE
     */
    this.rilRequestSetMute = function(req) { // 53
        print('Radio: rilRequestSetMute: req.data.state=' + req.data.state);
        muteState = req.data.state;
        if (gRadioState == RADIOSTATE_UNAVAILABLE) {
            result.rilErrCode = RIL_E_RADIO_NOT_AVAILABLE;
        }
        return result;
    }

    /**
     * Handle RIL_REQUEST_SCREEN_STATE
     *
     * @param req is the Request
     */
    this.rilRequestScreenState = function(req) { // 61
        print('Radio: rilRequestScreenState: req.data.state=' + req.data.state);
        screenState = req.data.state;
        return result;
    }

    /**
     * Delay test
     */
     this.cmdDelayTest = function(req) { // 2000
         print('cmdDelayTest: req.hello=' + req.hello);
         result.sendResponse = false;
         return result;
     }

     /**
      * Delay for RIL_UNSOL_SIGNAL_STRENGTH
      * TODO: Simulate signal strength changes:
      * Method 1: provide an array for signal strength, and send the unsolicited
      *           reponse periodically (the period can also be simulated)
      * Method 2: Simulate signal strength randomly (within a certain range) and
      *           send the response periodically.
      */
     this.cmdUnsolSignalStrength = function(req) { // 2001
         print('cmdUnsolSignalStrength: req.reqNum=' + req.reqNum);
         var rsp = new Object();

         // pack the signal strength into RspSignalStrength
         rsp.gwSignalstrength = gwSignalStrength;
         rsp.cdmaSignalstrength = cdmaSignalStrength;
         rsp.evdoSignalstrength = evdoSignalStrength;

         response = rilSchema[packageNameAndSeperator +
                              'RspSignalStrength'].serialize(rsp);

         // upldate signal strength
         sendRilUnsolicitedResponse(RIL_UNSOL_SIGNAL_STRENGTH, response);

         // Send the unsolicited signal strength every 1 minute.
         simulatedRadioWorker.addDelayed(
             {'reqNum' : CMD_UNSOL_SIGNAL_STRENGTH}, 60000);

         // this is not a request, no response is needed
         result.sendResponse = false;
         return result;
     }

     /**
      * Send RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED
      */
     this.cmdUnsolCallStateChanged = function(req) { // 2002
         print('cmdUnsolCallStateChanged: req.reqNum=' + req.reqNum);
         sendRilUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);
         result.sendResponse = false;
         return result;
     }

     /**
      * Simulate call state change
      */
     this.cmdCallStateChange = function(req) { // 2003
         print('cmdCallStateChange: req.reqNum=' + req.reqNum);
         print('cmdCallStateChange: req.callType=' + req.callType);
         print('cmdCallStateChange: req.callIndex=' + req.callIndex);
         print('cmdCallStateChange: req.nextState=' + req.nextState);

         // if it is an outgoing call, update the call state of the call
         // Send out call state changed flag
         var curCall = this.getCall(req.callIndex);
         this.printCall(curCall);

         if (curCall != null) {
             curCall.state = req.nextState;
         } else {
             this.removeCall(req.callIndex);
         }

         // TODO: if it is an incoming call, update the call state of the call
         // Send out call state change flag
         // Send out RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED
         simulatedRadioWorker.add(
           {'reqNum' : CMD_UNSOL_CALL_STATE_CHANGED});
         result.sendResponse = false;
         return result;
     }
    /**
     * Process the request by dispatching to the request handlers
     */
    this.process = function(req) {
        try {
            //print('Radio E: req.reqNum=' + req.reqNum + ' req.token=' + req.token);

            // Assume the result will be true, successful and nothing to return
            result.sendResponse = true;
            result.rilErrCode = RIL_E_SUCCESS;
            result.responseProtobuf = emptyProtobuf;

            try {
                // Pass "this" object to each ril request call such that
                // they have the same scope
                result = (this.radioDispatchTable[req.reqNum]).call(this, req);
            } catch (err) {
                print('Radio:process err = ' + err);
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

    /**
     * Construct the simulated radio
     */
    print('Radio: constructor E');
    this.radioDispatchTable = new Array();
    this.radioDispatchTable[RIL_REQUEST_GET_CURRENT_CALLS] = // 9
                this.rilRequestGetCurrentCalls;
    this.radioDispatchTable[RIL_REQUEST_DIAL] = // 10
                this.rilRequestDial;
    this.radioDispatchTable[RIL_REQUEST_HANGUP] =  // 12
                this.rilRequestHangUp;
    this.radioDispatchTable[RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND] = // 13
                this.rilRequestHangupWaitingOrBackground;
    this.radioDispatchTable[RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND] =  // 14
                this.rilRequestHangUpForegroundResumeBackground;
    this.radioDispatchTable[RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE] =  // 15
                this.rilRequestSwitchWaitingOrHoldingAndActive;
    this.radioDispatchTable[RIL_REQUEST_SIGNAL_STRENGTH] = // 19
                this.rilRequestSignalStrength;
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
    this.radioDispatchTable[RIL_REQUEST_SET_MUTE] = // 53
                this.rilRequestSetMute;
    this.radioDispatchTable[RIL_REQUEST_SCREEN_STATE] = // 61
                this.rilRequestScreenState;

    this.radioDispatchTable[CMD_DELAY_TEST] = // 2000
                this.cmdDelayTest;
    this.radioDispatchTable[CMD_UNSOL_SIGNAL_STRENGTH] =  // 2001
                this.cmdUnsolSignalStrength;
    this.radioDispatchTable[CMD_UNSOL_CALL_STATE_CHANGED] =  // 2002
                this.cmdUnsolCallStateChanged;
    this.radioDispatchTable[CMD_CALL_STATE_CHANGE] = //2003
                this.cmdCallStateChange;

    print('Radio: constructor X');
}

// The simulated radio instance and its associated Worker
var simulatedRadio = new Radio();
var simulatedRadioWorker = new Worker(function (req) {
    simulatedRadio.process(req);
});
simulatedRadioWorker.run();

/**
 * Optional tests
 */
if (false) {
    include("simulated_radio_tests.js");
}

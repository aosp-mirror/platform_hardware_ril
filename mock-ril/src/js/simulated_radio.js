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
    var maxConnectionsPerCall = 5; // only 5 connections allowed per call

    // Flag to denote whether an incoming/waiting call is answered
    var incomingCallIsProcessed = false;

    // Call transition flag
    var callTransitionFlag = false;  // default to auto-transition

    var lastCallFailCause = 0;

    // Array of "active" calls
    var calls = Array(maxNumberActiveCalls + 1);

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

    function LTESignalStrength() {
        this.signalStrength = -1;
        this.rsrp = 0;
        this.rsrq = 0;
        this.rssnr = 0;
        this.cqi = 0;
    }

    var gwSignalStrength = new GWSignalStrength;
    var cdmaSignalStrength = new CDMASignalStrength();
    var evdoSignalStrength = new EVDOSignalStrength();
    var lteSignalStrength = new LTESignalStrength();

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

    /**
     * @return the first call that is in the given state
     */
    this.getCallIdByState = function(callState) {
        if ((callState < CALLSTATE_ACTIVE) || (callState > CALLSTATE_WAITING)) {
            return null;
        }
        for (var i = 0; i < calls.length; i++) {
            if (typeof calls[i] != 'undefined') {
                if (calls[i].state == callState) {
                    return i;
                }
            }
        }
        return null;
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
            // call index should fall in the closure of [1, 7]
            // Search for an "undefined" element in the array and insert the call
            for (var i = 1; i < (maxNumberActiveCalls + 1); i++) {
                print('Radio: addCall, i=' + i);
                if (typeof calls[i] == 'undefined') {
                    print('Radio: addCall, calls[' + i + '] is undefined');
                    c.index = i;
                    calls[i] = c;
                    break;
                }
            }
            this.printCalls(calls);
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
        if ((c != null) && (typeof c != 'undefined')) {
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
            if ((callArray[i] == null) || (typeof callArray[i] == 'undefined')) {
                print('c[' + i + ']: undefined');
            } else {
                this.printCall(callArray[i]);
            }
        }
    }

    /**
     * Count number of calls in the given state
     *
     * @param callState is the give state
     */
    this.countCallsInState = function(callState) {
        var count = 0;
        if ((callState < CALLSTATE_ACTIVE) || (callState > CALLSTATE_WAITING)) {
            // not a valid call state
            return null;
        }
        for (var i = 0; i < calls.length; i++) {
            if (typeof calls[i] != 'undefined') {
                if (calls[i].state == callState) {
                    count++;
                }
            }
        }
        return count;
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
        print('lteRssi: '  + lteSignalStrength.signalStrength);
        print('lteRsrp: '  + lteSignalStrength.rsrp);
        print('lteRsrq: '  + lteSignalStrength.rsrq);
        print('lteRssnr: ' + lteSignalStrength.rssnr);
        print('lteCqi: '   + lteSignalStrength.cqi);
    }

    /**
     * set signal strength
     *
     * @param rssi and bitErrorRate are signal strength parameters for GSM
     *        cdmaDbm, cdmaEcio, evdoRssi, evdoEcio, evdoSnr are parameters for CDMA & EVDO
     */
    this.setSignalStrength = function(rssi, bitErrorRate, cdmaDbm, cdmaEcio, evdoRssi,
                                      evdoEcio, evdoSnr, lteSigStrength, lteRsrp,
                                      lteRsrq, lteRssnr, lteCqi) {
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
        lteSignalStrength.signalStrength = lteSigStrength;
        lteSignalStrength.rsrp = lteRsrp;
        lteSignalStrength.rsrq = lteRsrq;
        lteSignalStrength.rssnr = lteRssnr;
        lteSignalStrength.cqi = lteCqi;

        // pack the signal strength into RspSignalStrength and send a unsolicited response
        var rsp = new Object();

        rsp.gwSignalstrength = gwSignalStrength;
        rsp.cdmSignalstrength = cdmaSignalStrength;
        rsp.evdoSignalstrength = evdoSignalStrength;
        rsp.lteSignalstrength = lteSignalStrength;

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
        if (!callTransitionFlag) {
            // for auto transition
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
        }
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
                        case CALLSTATE_HOLDING:
                        case CALLSTATE_WAITING:
                        case CALLSTATE_INCOMING:
                            this.removeCall(i);
                            incomingCallIsProcessed = true;
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
        print('Radio: lastReq = ' + lastReq);
        print('Radio: req.reqNum = ' + req.reqNum);
        if (lastReq == req.reqNum) {
            print('Radio: called twice');
            return result;
        }

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
     * Handle RIL_REQUEST_CONFERENCE
     * Conference holding and active
     *
     * @param req is the Request
     */
    this.rilRequestConference = function(req) {  // 16
        print('Radio: rilRequestConference E');
        if ((numberActiveCalls <= 0) || (numberActiveCalls > maxConnectionsPerCall)) {
            // The maximum number of connections within a call is 5
            result.rilErrCode = RIL_E_GENERIC_FAILURE;
            return result;
        } else {
            var numCallsInBadState = 0;
            for (var i = 0; i < calls.length; i++) {
                if (typeof calls[i] != 'undefined') {
                    if ((calls[i].state != CALLSTATE_ACTIVE) &&
                            (calls[i].state != CALLSTATE_HOLDING)) {
                        numCallsInBadState++;
                    }
                }
            }

            // if there are calls not in ACITVE or HOLDING state, return error
            if (numCallsInBadState > 0) {
                result.rilErrCode = RIL_E_GENERIC_FAILURE;
                return result;
            } else { // conference ACTIVE and HOLDING calls
                for (var i = 0; i < calls.length; i++) {
                    if (typeof calls[i] != 'undefined') {
                        switch (calls[i].state) {
                            case CALLSTATE_ACTIVE:
                                break;
                            case CALLSTATE_HOLDING:
                                calls[i].state = CALLSTATE_ACTIVE;
                                break;
                            default:
                                result.rilErrCode = RIL_E_GENERIC_FAILURE;
                                break;
                        }
                    }
                    this.printCalls(calls);
                    if(result.rilErrCode == RIL_E_GENERIC_FAILURE) {
                        return result;
                    }
                }
            }
        }

        // Only if conferencing is successful,
        // Send out RIL_UNSOL_CALL_STATE_CHANGED after the request is returned
        simulatedRadioWorker.add(
          {'reqNum' : CMD_UNSOL_CALL_STATE_CHANGED});
        return result;
    }

    /**
     * Handle RIL_REQUEST_LAST_CALL_FAIL_CAUSE
     *
     * @param req is the request
     */
    this.rilRequestLastCallFailCause = function(req) {
        print('Radio: rilRequestLastCallFailCause E');

        var rsp = new Object();
        rsp.integers = new Array();
        rsp.integers[0] = lastCallFailCause;

        result.responseProtobuf = rilSchema[packageNameAndSeperator +
                                            'RspIntegers'].serialize(rsp);
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
     * Handle RIL_REQUEST_VOICE_REGISTRATION_STATE
     *
     * @param req is the Request
     */
    this.rilRequestVoiceRegistrationState = function(req) { // 20
        print('Radio: rilRequestVoiceRegistrationState');

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
     * Handle RIL_REQUEST_DATA_REGISTRATION_STATE
     *
     * @param req is the Request
     */
    this.rilRequestDataRegistrationState = function(req) { // 21
        print('Radio: rilRequestDataRegistrationState');

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
     * Handle RIL_REQUEST_ANSWER
     *
     * @param req is the Request
     */
    this.rilRequestAnswer = function(req) { // 40
        print('Radio: rilRequestAnswer');

        if (numberActiveCalls != 1) {
            result.rilErrCode = RIL_E_GENERIC_FAILURE;
            return result;
        } else {
            for (var i = 0; i < calls.length; i++) {
                if (typeof calls[i] != 'undefined') {
                    if (calls[i].state == CALLSTATE_INCOMING) {
                        calls[i].state = CALLSTATE_ACTIVE;
                        break;
                    } else {
                        result.rilErrCode = RIL_E_GENERIC_FAILURE;
                        this.removeCall(i);
                        return result;
                    }
                } // end of processing call[i]
            } // end of for
        }
        incomingCallIsProcessed = true;
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
     * Handle RIL_REQUEST_SEPRATE_CONNECTION
     * Separate a party from a multiparty call placing the multiparty call
     * (less the specified party) on hold and leaving the specified party
     * as the only other member of the current (active) call
     *
     * See TS 22.084 1.3.8.2 (iii)
     *
     * @param req is the Request
     */
    this.rilReqestSeparateConnection = function(req) {  // 52
        print('Radio: rilReqestSeparateConnection');
        var index = req.data.index;

        if (numberActiveCalls <= 0) {
            result.rilErrCode = RIL_E_GENERIC_FAILURE;
            return result;
        } else {
            // get the connection to separate
            var separateConn = this.getCall(req.data.index);
            if (separateConn == null) {
                result.rilErrCode = RIL_E_GENERIC_FAILURE;
                return result;
            } else {
                if (separateConn.state != CALLSTATE_ACTIVE) {
                    result.rilErrCode = RIL_E_GENERIC_FAILURE;
                    return result;
                }
                // Put all other connections in hold.
                for (var i = 0; i < calls.length; i++) {
                    if (index != i) {
                        // put all the active call to hold
                        if (typeof calls[i] != 'undefined') {
                            switch (calls[i].state) {
                                case CALLSTATE_ACTIVE:
                                    calls[i].state = CALLSTATE_HOLDING;
                                    break;
                                default:
                                    result.rilErrCode = RIL_E_GENERIC_FAILURE;
                                    break;
                            }
                            this.printCalls(calls);
                            if(result.rilErrCode == RIL_E_GENERIC_FAILURE) {
                                return result;
                            }
                        }  // end of processing call[i]
                    }
                }  // end of for
            }
        }

        // Send out RIL_UNSOL_CALL_STATE_CHANGED after the request is returned
        simulatedRadioWorker.add(
          {'reqNum' : CMD_UNSOL_CALL_STATE_CHANGED});
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
      * send UNSOL_CALL_STATE_CHANGED and UNSOL_CALL_RING
      */
     this.cmdUnsolCallRing = function(req) { // 2004
         print('cmdUnsolCallRing: req.reqNum=' + req.reqNum);
         if(!incomingCallIsProcessed) {
             sendRilUnsolicitedResponse(RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED);
             sendRilUnsolicitedResponse(RIL_UNSOL_CALL_RING);

             // Send the next alert in 3 seconds. [refer to ril.h definition]
             simulatedRadioWorker.addDelayed(
                 {'reqNum' : CMD_UNSOL_CALL_RING}, 3000);
         }
         result.sendResponse = false;
         return result;
     }

     /**
      * Create an incoming call for the giving number
      * return CTRL_STATUS_ERR if there is already a call in any of the states of
      * dialing, alerting, incoming, waiting [TS 22 030 6.5] , else
      * return CTRL_STATUS_OK and update the call state
      */
     this.ctrlServerCmdStartInComingCall = function(req) {  // 1001
         print('ctrlServerCmdStartInComingCall: req.reqNum:' + req.reqNum);
         print('ctrlServerCmdStartInComingCall: req.data.phonenumber:' + req.data.phoneNumber);

         var phoneNumber = req.data.phoneNumber;
         var state;

         if (numberActiveCalls <= 0) {
             // If there is no connection in use, the call state is INCOMING
             state = CALLSTATE_INCOMING;
         } else {
             // If there is call in any of the states of dialing, alerting, incoming
             // waiting, this MT call can not be set
             for (var i  = 0; i < calls.length; i++) {
                 if (typeof calls[i] != 'undefined') {
                     if ( (calls[i].state == CALLSTATE_DIALING) ||
                          (calls[i].state == CALLSTATE_ALERTING) ||
                          (calls[i].state == CALLSTATE_INCOMING) ||
                          (calls[i].state == CALLSTATE_WAITING))
                     {
                         result.rilErrCode = CTRL_STATUS_ERR;
                         return result;
                     }
                 }
             }
             // If the incoming call is a second call, the state is WAITING
             state = CALLSTATE_WAITING;
         }

         // Add call to the call array
         this.addCall(state, phoneNumber, '');

         // set the incomingCallIsProcessed flag to be false
         incomingCallIsProcessed = false;

         simulatedRadioWorker.add(
           {'reqNum' : CMD_UNSOL_CALL_RING});

         result.rilErrCode = CTRL_STATUS_OK;
         return result;
    }

    /**
     * Handle control command CTRL_CMD_HANGUP_CONN_REMOTE
     *   hangup the connection for the given failure cause
     *
     *@param req is the control request
     */
    this.ctrlServerCmdHangupConnRemote = function(req) {    // 1002
        print('ctrlServerCmdHangupConnRemote: req.data.connectionId=' + req.data.connectionId +
              ' req.data.callFailCause' + req.data.callFailCause);

        var connection = req.data.connectionId;
        var failureCause = req.data.callFailCause;

        this.printCalls(calls);
        var hangupCall = this.removeCall(connection);
        if (hangupCall == null) {
            print('ctrlServerCmdHangupConnRemote: connection id is required.');
            result.rilErrCode = CTRL_STATUS_ERR;
            return result;
        } else {
            // for incoming call, stop sending call ring
            if ((hangupCall.state == CALLSTATE_INCOMING) ||
                (hangupCall.state == CALLSTATE_WAITING)) {
                incomingCallIsProcessed = true;
            }
        }
        this.printCalls(calls);
        lastCallFailCause = failureCause;

        // send out call state changed response
        simulatedRadioWorker.add(
            {'reqNum' : CMD_UNSOL_CALL_STATE_CHANGED});

        result.rilErrCode = CTRL_STATUS_OK;
        return result;
    }

    /**
     * Set call transition flag
     */
    this.ctrlServerCmdSetCallTransitionFlag = function(req) {  // 1003
        print('ctrlServerCmdSetCallTransitionFlag: flag=' + req.data.flag);

        callTransitionFlag = req.data.flag;
        result.rilErrCode = CTRL_STATUS_OK;
        return result;
    }

    /**
     * Set the dialing call to alert
     */
    this.ctrlServerCmdSetCallAlert = function(req) {    // 1004
        print('ctrlServerCmdSetCallAlert: E');

        if (callTransitionFlag == false) {
            print('ctrlServerCmdSetCallAlert: need to set the flag first');
            result.rilErrCode = CTRL_STATUS_ERR;
            return result;
        }
        if (numberActiveCalls <= 0) {
            print('ctrlServerCmdSetCallAlert: no active calls');
            result.rilErrCode = CTRL_STATUS_ERR;
            return result;
        }
        var dialingCalls = this.countCallsInState(CALLSTATE_DIALING);
        var index = this.getCallIdByState(CALLSTATE_DIALING);
        if ((dialingCalls == 1) && (index != null)) {
            calls[index].state = CALLSTATE_ALERTING;
        } else {
            // if there 0 or more than one call in dialing state, return error
            print('ctrlServerCmdSetCallAlert: no valid calls in dialing state');
            result.rilErrCode = CTRL_STATUS_ERR;
            return result;
        }
        // send unsolicited call state change response
        simulatedRadioWorker.add(
            {'reqNum' : CMD_UNSOL_CALL_STATE_CHANGED});

        result.rilErrCode = CTRL_STATUS_OK;
        return result;
    }

    /**
     * Set the alserting call to active
     */
    this.ctrlServerCmdSetCallActive = function(req) {   // 1005
        print('ctrlServerCmdSetCallActive: E');

        if (callTransitionFlag == false) {
            print('ctrlServerCmdSetCallActive: need to set the flag firt');
            result.rilErrCode = CTRL_STATUS_ERR;
            return result;
        }
        if (numberActiveCalls <= 0) {
            print('ctrlServerCmdSetCallActive: no active calls');
            result.rilErrCode = CTRL_STATUS_ERR;
            return result;
        }
        var alertingCalls = this.countCallsInState(CALLSTATE_ALERTING);
        var index = this.getCallIdByState(CALLSTATE_ALERTING);
        if ((alertingCalls == 1) && (index != null)) {
            calls[index].state = CALLSTATE_ACTIVE;
        } else {
            print('ctrlServerCmdSetCallActive: no valid calls in alert state');
            result.rilErrCode = CTRL_STATUS_ERR;
            return result;
        }
        // send out unsolicited call state change response
        simulatedRadioWorker.add(
            {'reqNum' : CMD_UNSOL_CALL_STATE_CHANGED});

        result.rilErrCode = CTRL_STATUS_OK;
        return result;
    }

    /**
     * Add a dialing call
     */
    this.ctrlServerCmdAddDialingCall = function(req) {   // 1006
        print('ctrlServerCmdAddDialingCall: E');

        var phoneNumber = req.data.phoneNumber;
        var dialingCalls = this.countCallsInState(CALLSTATE_DIALING);
        if (dialingCalls == 0) {
            this.addCall(CALLSTATE_DIALING, phoneNumber, '');
            result.rilErrCode = CTRL_STATUS_OK;
        } else {
            print('ctrlServerCmdAddDialingCall: add dialing call failed');
            result.rilErrCode = CTRL_STATUS_ERR;
        }
        return result;
    }

    /**
     * Process the request by dispatching to the request handlers
     */
    this.process = function(req) {
        try {
            print('Radio E: req.reqNum=' + req.reqNum + ' req.token=' + req.token);

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

            if (req.reqNum < 200) {
                lastReq = req.reqNum;
            }
            if (result.sendResponse) {
                if (isCtrlServerDispatchCommand(req.reqNum)) {
                    //print('Command ' + req.reqNum + ' is a control server command');
                    sendCtrlRequestComplete(result.rilErrCode, req.reqNum,
                      req.token, result.responseProtobuf);
                } else {
                    //print('Request ' + req.reqNum + ' is a ril request');
                    sendRilRequestComplete(result.rilErrCode, req.reqNum,
                      req.token, result.responseProtobuf);
                }
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
    this.radioDispatchTable[RIL_REQUEST_CONFERENCE] = // 16
                this.rilRequestConference;
    this.radioDispatchTable[RIL_REQUEST_LAST_CALL_FAIL_CAUSE] = // 18
                this.rilRequestLastCallFailCause;
    this.radioDispatchTable[RIL_REQUEST_SIGNAL_STRENGTH] = // 19
                this.rilRequestSignalStrength;
    this.radioDispatchTable[RIL_REQUEST_VOICE_REGISTRATION_STATE] = // 20
                this.rilRequestVoiceRegistrationState;
    this.radioDispatchTable[RIL_REQUEST_DATA_REGISTRATION_STATE] = // 21
                this.rilRequestDataRegistrationState;
    this.radioDispatchTable[RIL_REQUEST_ANSWER] = // 40
                this.rilRequestAnswer;
    this.radioDispatchTable[RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE] = // 45
                this.rilRequestQueryNeworkSelectionMode;
    this.radioDispatchTable[RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC] = // 46
                this.rilRequestSetNeworkSelectionAutomatic;
    this.radioDispatchTable[RIL_REQUEST_BASEBAND_VERSION] = // 51
                this.rilRequestBaseBandVersion;
    this.radioDispatchTable[RIL_REQUEST_SEPARATE_CONNECTION] = // 52
                this.rilReqestSeparateConnection;
    this.radioDispatchTable[RIL_REQUEST_SET_MUTE] = // 53
                this.rilRequestSetMute;
    this.radioDispatchTable[RIL_REQUEST_SCREEN_STATE] = // 61
                this.rilRequestScreenState;

    this.radioDispatchTable[CTRL_CMD_SET_MT_CALL] = //1001
                this.ctrlServerCmdStartInComingCall;
    this.radioDispatchTable[CTRL_CMD_HANGUP_CONN_REMOTE] = //1002
                this.ctrlServerCmdHangupConnRemote;
    this.radioDispatchTable[CTRL_CMD_SET_CALL_TRANSITION_FLAG] = //1003
                this.ctrlServerCmdSetCallTransitionFlag;
    this.radioDispatchTable[CTRL_CMD_SET_CALL_ALERT] =  // 1004
                this.ctrlServerCmdSetCallAlert;
    this.radioDispatchTable[CTRL_CMD_SET_CALL_ACTIVE] =  // 1005
                this.ctrlServerCmdSetCallActive;
    this.radioDispatchTable[CTRL_CMD_ADD_DIALING_CALL] =  // 1006
                this.ctrlServerCmdAddDialingCall;

    this.radioDispatchTable[CMD_DELAY_TEST] = // 2000
                this.cmdDelayTest;
    this.radioDispatchTable[CMD_UNSOL_SIGNAL_STRENGTH] =  // 2001
                this.cmdUnsolSignalStrength;
    this.radioDispatchTable[CMD_UNSOL_CALL_STATE_CHANGED] =  // 2002
                this.cmdUnsolCallStateChanged;
    this.radioDispatchTable[CMD_CALL_STATE_CHANGE] = //2003
                this.cmdCallStateChange;
    this.radioDispatchTable[CMD_UNSOL_CALL_RING] = //2004
                this.cmdUnsolCallRing;

    print('Radio: constructor X');
}

// The simulated radio instance and its associated Worker
var simulatedRadio = new Radio();
var simulatedRadioWorker = new Worker(function (req) {
    simulatedRadio.process(req);
});
simulatedRadioWorker.run();

// TODO: this is a workaround for bug http://b/issue?id=3001613
// When adding a new all, two RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE
// will be sent from the framework.
var lastReq = 0;

/**
 * Optional tests
 */
if (false) {
    include("simulated_radio_tests.js");
}

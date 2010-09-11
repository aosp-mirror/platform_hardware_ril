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
 * A test to test set signal strength
 */
if (false) {
    function testSetSignalStrength() {
        print('testSetSignalStrength E:');
        simulatedRadio.printSignalStrength();
        try {
            simulatedRadio.setSignalStrength(0, -1, -1, -1, -1, -1, -1);
        } catch (err) {
            print('test failed');
        }
        simulatedRadio.printSignalStrength();
        try {
            simulatedRadio.setSignalStrength(60, 30, 29 , 28, 27, 26, 25);
        } catch (err) {
            print('test success: ' + err);
        }
        simulatedRadio.printSignalStrength();
    }
    testSetSignalStrength();
}

/**
 * TODO: A test for RIL_REQUEST_GET_CURRENT_CALLS,
 *       remove when satisfied all is well.
 */
if (false) {
    var calls = simulatedRadio.getCalls();

    function testCalls() {
        print('testCalls E:');
        var c0 = simulatedRadio.addCall(CALLSTATE_ACTIVE, '16502859848', 'w');
        simulatedRadio.printCalls();
        var c1 = simulatedRadio.addCall(CALLSTATE_ACTIVE, '16502583456', 'm');
        simulatedRadio.printCalls();
        var c2 = simulatedRadio.addCall(CALLSTATE_ACTIVE, '16502345678', 'x');
        simulatedRadio.printCalls();
        var c3 = simulatedRadio.addCall(CALLSTATE_ACTIVE, '16502349876', 'y');
        simulatedRadio.printCalls();

        simulatedRadio.removeCall(c0.index);
        simulatedRadio.printCalls();
        simulatedRadio.removeCall(c1.index);
        simulatedRadio.printCalls();
        simulatedRadio.removeCall(c2.index);
        simulatedRadio.printCalls();

        result = simulatedRadio.rilRequestGetCurrentCalls();
        newCalls = rilSchema[packageNameAndSeperator +
                            'RspGetCurrentCalls'].parse(result.responseProtobuf);
        simulatedRadio.printCalls(newCalls.calls);

        // Set to false to test RIL_REQUEST_GET_CURRENT_CALLS as there will
        // be on call still active on the first RIL_REQUEST_GET_CURRENT_CALLS
        // request.
        if (false) {
            simulatedRadio.removeCall(c3.index);
            simulatedRadio.printCalls();
        }
        print('testCalls X:');
    }

    testCalls();
}

/**
 * A test for RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND
 */
if (false) {
    var calls = simulatedRadio.getCalls();

    function testHangUpForegroundResumeBackground() {
        print('testHangUpForegroundResumeBackground E:');
        var testOutput = false;
        for (var state = CALLSTATE_ACTIVE; state <= CALLSTATE_WAITING; state++) {
            var c0 = simulatedRadio.addCall(state, '16502849230', 'smith');
            var req = new Object();
            req.reqNum = RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND;
            var testResult = simulatedRadio.rilRequestHangUpForegroundResumeBackground(req);
            if (state == CALLSTATE_ACTIVE) {
                var testCalls = simulatedRadio.getCalls();
                if (testCalls.length == 0) {
                    testOutput = true;
                } else {
                    testOutput = false;
                }
            } else if (state == CALLSTATE_WAITING) {
                if (c0.state == CALLSTATE_ACTIVE) {
                    testOutput = true;
                } else {
                    testOutput = false;
                }
            } else if (state == CALLSTATE_HOLDING) {
                if (c0.state == CALLSTATE_ACTIVE) {
                    testOutput = true;
                } else {
                    testOutput = false;
                }
            } else {
                if (testResult.rilErrCode == RIL_E_GENERIC_FAILURE) {
                    testOutput = true;
                } else {
                    testOutput = false;
                }
            }
            if (testOutput == true) {
                print('testHangUpForegroundResumeBackground, call ' + state + ' PASS \n');
            } else {
                print('testHangUpForegroundResumeBackground, call ' + state + ' FAIL \n');
            }
            simulatedRadio.removeCall(c0.index);
            simulatedRadio.printCalls();
        }
    }

    testHangUpForegroundResumeBackground();
}

/**
 * Test serialization of bad numeric enum
 */
if (false) {
    c = new RilCall(1000, '11234567890', 'me');
    rsp = new Object();
    rsp.calls = [ c ];
    try {
        rilSchema[packageNameAndSeperator + 'RspGetCurrentCalls'].serialize(rsp);
        print('test-enum a bad numeric enum value, FAILURE exception expected');
    } catch (err) {
        print('test-enum a bad numeric enum value, SUCCESS exception expected: ' + err);
    }
}

/**
 * Test serialization of bad string enum
 */
if (false) {
    // The state parameter 'NOT_CALLSTATE_ACTIVE' can get corrupted in ToProto?
    c = new RilCall('NOT_CALLSTATE_ACTIVE', '11234567890', 'me');
    rsp = new Object();
    rsp.calls = [ c ];
    try {
        rilSchema[packageNameAndSeperator + 'RspGetCurrentCalls'].serialize(rsp);
        print('test-enum a bad string enum value, FAILURE exception expected');
    } catch (err) {
        print('test-enum a bad string enum value, SUCCESS exception expected: ' + err);
    }
}

/**
 * Test addDelayed
 */
if (false) {
    print("test addDelayed E");
    simulatedRadioWorker.add( {
        'reqNum' : REQUEST_DELAY_TEST,
        'hello' : 'hi no delay' });
    simulatedRadioWorker.addDelayed( {
        'reqNum' : REQUEST_DELAY_TEST,
        'hello' : 'hi not-a-number is 0 delay' }, "not-a-number");
    simulatedRadioWorker.addDelayed( {
        'reqNum' : REQUEST_DELAY_TEST,
        'hello' : 'hi negative delay is 0 delay' }, -1000);
    simulatedRadioWorker.addDelayed( {
        'reqNum' : REQUEST_DELAY_TEST,
        'hello' : 'hi delayed 2 seconds' }, 2000);
    print("test addDelayed X");
}

/**
 * A test for setRadioState, verify it can handle valid string variable,
 * undefined varilabe, and invalid radio state correctly.
 */
if (false) {
    function testSetRadioState() {
        print('testSetRadioState E:');
        // defined string variable
        newState = 'RADIOSTATE_UNAVAILABLE';
        try {
            setRadioState(newState);
        } catch (err) {
            print('test failed');
        }
        print('Expecting gRadioState to be ' + RADIOSTATE_UNAVAILABLE +
              ', gRadioState is: ' + gRadioState);

        // undefined string variable, expecting exception
        try {
            setRadioState('RADIOSTATE_UNDEFINED');
        } catch (err) {
            if (err.indexOf('Unknow string') >= 0) {
                print('test success');
                print('err: ' + err);
            } else {
                print('test failed');
            }
        }

        // valid radio state
        try {
            setRadioState(RADIOSTATE_NV_READY);
        } catch (err) {
            print('test failed');
        }
        print('Expecting gRadioState to be ' + RADIOSTATE_NV_READY +
              ', gRadioState is: ' + gRadioState);

        // invalid radio state
        try {
            setRadioState(-1);
        } catch (err) {
            if (err.indexOf('invalid') >= 0) {
                print('test success');
                print('err: ' + err);
            } else {
                print('test failed, err: ' + err);
            }
        }
        print('gRadioState should not be set: ' + gRadioState);

        // set radio state to be SIM_READY
        setRadioState(RADIOSTATE_SIM_READY);
        print('Expecting gRadioState to be ' + RADIOSTATE_SIM_READY +
              ', gRadioState is: ' + gRadioState);
        print('testSetRadioState X:');
    }

    testSetRadioState();
}

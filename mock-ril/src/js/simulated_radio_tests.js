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
 * TODO: A test for RIL_REQUEST_GET_CURRENT_CALLS,
 *       remove when satisfied all is well.
 */
if (false) {
    var calls = simulatedRadio.getCalls();

    function testCalls() {
        print('testCalls E:');
        c0 = simulatedRadio.addCall('CALLSTATE_ACTIVE', '18312342134', 'wink');
        simulatedRadio.printCalls();
        c1 = simulatedRadio.addCall('CALLSTATE_ACTIVE', '18312342135', 'me');
        simulatedRadio.printCalls();
        simulatedRadio.removeCall(c0.index);
        simulatedRadio.printCalls();

        result = simulatedRadio.rilRequestGetCurrentCalls();
        newCalls = rilSchema[packageNameAndSeperator +
                        'RspGetCurrentCalls'].parse(result.responseProtobuf);
        simulatedRadio.printCalls(newCalls.calls);

        // Set to false to test RIL_REQUEST_GET_CURRENT_CALLS as there will
        // be on call still ative on the first RIL_REQUEST_GET_CURRENT_CALLS
        // request.
        if (false) {
            simulatedRadio.removeCall(c1.index);
            simulatedRadio.printCalls();
        }
        print('testCalls X:');
    }

    testCalls();
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
        'reqNum' : simulatedRadio.REQUEST_DELAY_TEST,
        'hello' : 'hi no delay' });
    simulatedRadioWorker.addDelayed( {
        'reqNum' : simulatedRadio.REQUEST_DELAY_TEST,
        'hello' : 'hi not-a-number is 0 delay' }, "not-a-number");
    simulatedRadioWorker.addDelayed( {
        'reqNum' : simulatedRadio.REQUEST_DELAY_TEST,
        'hello' : 'hi negative delay is 0 delay' }, -1000);
    simulatedRadioWorker.addDelayed( {
        'reqNum' : simulatedRadio.REQUEST_DELAY_TEST,
        'hello' : 'hi delayed 2 seconds' }, 2000);
    print("test addDelayed X");
}

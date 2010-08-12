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


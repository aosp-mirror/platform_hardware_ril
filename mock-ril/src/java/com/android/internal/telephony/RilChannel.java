/*
 * Copyright (C) 2010, The Android Open Source Project
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

package com.android.internal.telephony;

import android.util.Log;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.nio.channels.SocketChannel;
import java.nio.ByteBuffer;

public class RilChannel {
    private static final boolean DBG = false;

    private static final String MOCKRIL_ADDR_STR = "127.0.0.1";
    private static final int MOCKRIL_PORT = 54312;
    private SocketChannel mChannel = null;

    /**
     * Constructor
     */
    private RilChannel() {
        if (DBG) log("ctor EX");
    }

    /**
     * Open the channel
     *
     * @throws IOException
     */
    private void open() throws IOException {
        InetSocketAddress mockRilAddr = new InetSocketAddress(MOCKRIL_ADDR_STR, MOCKRIL_PORT);
        mChannel= SocketChannel.open(mockRilAddr);
    }

    /**
     * Make the channel
     *
     * @return the channel
     * @throws IOException
     */
    public static RilChannel makeRilChannel() throws IOException {
        if (DBG) log("makeMockRilChannel E");
        RilChannel sm = new RilChannel();
        sm.open();
        if (DBG) log("makeMockRilChannel X");
        return sm;
    }

    /**
     * Close an open channel
     */
    public void close() {
        try {
            if (mChannel != null) {
                mChannel.close();
                if (DBG) log("DefaultState.enter closed socket");
            }
        } catch (IOException e) {
            log("Could not close conection to mock-ril");
            e.printStackTrace();
        }
    }

    /**
     * @return the channel
     */
    public SocketChannel getChannel() {
        return mChannel;
    }

    /**
     * write the bb contents to sc
     *
     * @param bb is the ByteBuffer to write
     * @return number of bytes written
     * @throws IOException
     */
    public final int sendAll(ByteBuffer bb) throws IOException {
        int count = 0;
        while (bb.remaining() != 0) {
            count += mChannel.write(bb);
        }
        return count;
    }

    /**
     * read from sc until bb is filled then rewind bb
     *
     * @param bb is the ByteBuffer to fill
     * @return number of bytes read
     * @throws IOException
     */
    public final int recvAll(ByteBuffer bb) throws IOException {
        int count = 0;
        while (bb.remaining() != 0) {
            count += mChannel.read(bb);
        }
        return count;
    }

    /**
     * Rewind bb then write the contents to sc
     *
     * @param bb is the ByteBuffer to write
     * @return number of bytes written
     * @throws IOException
     */
    public final int rewindSendAll(ByteBuffer bb) throws IOException {
        bb.rewind();
        return sendAll(bb);
    }

    /**
     * read from sc until bb is filled then rewind bb
     *
     * @param bb is the ByteBuffer to fill
     * @return number of bytes read
     * @throws IOException
     */
    public final int recvAllRewind(ByteBuffer bb) throws IOException {
        int count = recvAll(bb);
        bb.rewind();
        return count;
    }

    /**
     * Write to log.
     *
     * @param s
     */
    static void log(String s) {
        Log.v("MockRilChannel", s);
    }
}

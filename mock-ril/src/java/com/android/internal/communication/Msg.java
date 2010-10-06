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

package com.android.internal.communication;

import android.util.Log;

import com.android.internal.communication.MsgHeader;
import com.android.internal.telephony.RilChannel;
import com.google.protobuf.micro.InvalidProtocolBufferMicroException;
import com.google.protobuf.micro.MessageMicro;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * A message
 */
public class Msg {
    private MsgHeader  mHeader;
    private ByteBuffer mData;

    /**
     * Send a message header
     *
     * @param mh is message header to write
     * @throws IOException
     */
    private static void sendHeader(RilChannel rc, MsgHeader mh) throws IOException {
        ByteBuffer lenBuffer = ByteBuffer.allocateDirect(4);
        lenBuffer.order(ByteOrder.LITTLE_ENDIAN);
        lenBuffer.putInt(mh.getSerializedSize());

        ByteBuffer mhBuffer = ByteBuffer.allocateDirect(mh.getCachedSize());
        mhBuffer.put(mh.toByteArray());

        rc.rewindSendAll(lenBuffer);
        rc.rewindSendAll(mhBuffer);
    }

    /**
     * Read a message header
     *
     * @returns message header
     * @throws IOException
     */
    private static MsgHeader recvHeader(RilChannel rc) throws IOException {
        ByteBuffer lenBuffer = ByteBuffer.allocate(4);
        lenBuffer.order(ByteOrder.LITTLE_ENDIAN);
        int lenRead = rc.recvAllRewind(lenBuffer);
        int lenHeader = lenBuffer.getInt();

        ByteBuffer mhBuffer = ByteBuffer.allocate(lenHeader);
        lenRead = rc.recvAllRewind(mhBuffer);
        MsgHeader mh = MsgHeader.parseFrom(mhBuffer.array());
        return mh;
    }

    /**
     * Msg Constructor
     */
    private Msg() {
    }

    /**
     * Get a message
     */
    public static Msg obtain() {
        // TODO: Get from a free list
        return new Msg();
    }

    /**
     * Release a message
     */
    public void release() {
        // TODO: place back on free list
    }

    /**
     * Send a message header followed by the data if present
     *
     * The length data field will be filled in as appropriate
     * @param mh header
     * @param data if not null and length > 0 sent after header
     * @throws IOException
     */
    public static final void send(RilChannel rc, MsgHeader mh, ByteBuffer data)
            throws IOException {
        int lenData;

        if (data == null) {
            lenData = 0;
        } else {
            data.rewind();
            lenData = data.remaining();
        }
        mh.setLengthData(lenData);
        sendHeader(rc, mh);
        if (lenData > 0) {
            rc.sendAll(data);
        }
    }

    /**
     * Send a message with cmd, token, status followed by the data.
     *
     * The length data field will be filled in as appropriate
     * @param cmd for the header
     * @param token for the header
     * @param status for the header
     * @param pb is the protobuf to send
     * @throws IOException
     */
    public static final void send(RilChannel rc, int cmd, long token, int status, MessageMicro pb)
            throws IOException {
        MsgHeader mh = new MsgHeader();
        mh.setCmd(cmd);
        mh.setToken(token);
        mh.setStatus(status);

        ByteBuffer data;
        if (pb != null) {
            data = ByteBuffer.wrap(pb.toByteArray());
        } else {
            data = null;
        }
        send(rc, mh, data);
    }

    /**
     * Send a message with cmd, token, status followed by the data.
     *
     * The length data field will be filled in as appropriate
     * @param cmd for the header
     * @param token for the header
     * @param pb is the protobuf to send
     * @throws IOException
     */
    public static final void send(RilChannel rc, int cmd, long token, MessageMicro pb)
            throws IOException {
        send(rc, cmd, token, 0, pb);
    }

    /**
     * Send a message with cmd followed by the data.
     *
     * The length data field will be filled in as appropriate
     * @param cmd for the header
     * @param pb is the protobuf to send
     * @throws IOException
     */
    public static final void send(RilChannel rc, int cmd, MessageMicro pb) throws IOException {
        send(rc, cmd, 0, 0, pb);
    }

    /**
     * Send a message with cmd, token and status but no data
     *
     * The length data field will be filled in as appropriate
     * @param cmd for the header
     * @param token for the header
     * @param status for the header
     * @throws IOException
     */
    public static final void send(RilChannel rc, int cmd, long token, int status)
            throws IOException {
        send(rc, cmd, token, status, null);
    }

    /**
     * Send a message with cmd and token but no data
     *
     * The length data field will be filled in as appropriate
     * @param cmd for the header
     * @param token for the header
     * @throws IOException
     */
    public static final void send(RilChannel rc, int cmd, long token) throws IOException {
        send(rc, cmd, token, 0, null);
    }

    /**
     * Send a message with cmd but no data
     *
     * The length data field will be filled in as appropriate
     * @param cmd for the header
     * @throws IOException
     */
    public static final void send(RilChannel rc, int cmd) throws IOException {
        send(rc, cmd, 0, 0, null);
    }

    /**
     * Read a message
     *
     * @return Msg
     * @throws IOException
     */
    public static final Msg recv(RilChannel rc) throws IOException {
        Msg msg = Msg.obtain();
        msg.read(rc);
        return msg;
    }

    /**
     * Read a message header and data.
     *
     * @throws IOException
     */
    public void read(RilChannel rc) throws IOException {
        mHeader = recvHeader(rc);
        if (mHeader.getLengthData() > 0) {
            ByteBuffer bb = ByteBuffer.allocate(mHeader.getLengthData());
            rc.recvAllRewind(bb);
            mData = bb;
        }
    }

    /**
     * Print the message header.
     *
     * @param tag for the header
     */
    public void printHeader(String tag) {
        Log.d(tag, " cmd=" + mHeader.getCmd() + " token=" + mHeader.getToken() + " status="
                        + mHeader.getStatus() + " lengthData=" + mHeader.getLengthData());
    }

    /**
     * Set data (for testing purposes only).
     */
    public void setData(ByteBuffer data) {
        mData = data;
    }

    /**
     * Set header (for testing purposes only).
     */
    public void setHeader(MsgHeader header) {
        mHeader = header;
    }

    /**
     * @return cmd
     */
    public int getCmd() {
        return mHeader.getCmd();
    }

    /**
     * @return token
     */
    public long getToken() {
        return mHeader.getToken();
    }

    /**
     * @return status
     */
    public int getStatus() {
        return mHeader.getStatus();
    }

    /**
     * @return data ByteBuffer
     */
    public ByteBuffer getData() {
        return mData;
    }

    /**
     * @return data at index
     */
    public byte getData(int index) {
        return mData.get(index);
    }

    /**
     * Return data as a Class<T>.
     *
     * @param <T> a class that extends MessageMicro.
     * @param c the T.class to create from the data.
     * @param data is the MessageMicro protobuf to be converted.
     * @return null if an error occurs.
     */
    @SuppressWarnings("unchecked")
    public static final <T extends MessageMicro> T getAs(Class<T> c, byte[] data) {
        Object o = null;
        if ((data != null) && (data.length > 0)) {
            try {
                o = c.newInstance().mergeFrom(data);
            } catch (InvalidProtocolBufferMicroException e) {
                e.printStackTrace();
            } catch (InstantiationException e) {
                e.printStackTrace();
            } catch (IllegalAccessException e) {
                e.printStackTrace();
            }
        }
        return (T)o;
    }

    /**
     * Return data as a Class<T>.
     *
     * @param <T> a class that extends MessageMicro.
     * @param c the T.class to create from data.
     * @return null if an error occurs
     */
    @SuppressWarnings("unchecked")
    public <T extends MessageMicro> T getDataAs(Class<T> c) {
        Object o;

        if ((mData != null) && (mData.remaining() > 0)) {
            o = getAs(c, mData.array());
        } else {
            o = null;
        }
        return (T)o;
    }
}

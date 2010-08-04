#!/usr/bin/python
#
# Copyright (C) 2010 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License")
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""Test server

A detailed description of tms.
"""

__author__ = 'wink@google.com (Wink Saville)'

import socket
import struct
import sys
import time

import ctrl_pb2
import ril_pb2
import msgheader_pb2

def recvall(s, count):
  """Receive all of the data otherwise return none.

  Args:
    s: socket
    count: number of bytes

  Returns:
    data received
    None if no data is received
  """
  all_data = []
  while (count > 0):
    data = s.recv(count)
    if (len(data) == 0):
      return None
    count -= len(data)
    all_data.append(data)
  result = ''.join(all_data);
  return result

def sendall(s, data):
  """Send all of the data

  Args:
    s: socket
    count: number of bytes

  Returns:
    Nothing
  """
  s.sendall(data)

class MsgHeader:
  """A fixed length message header; cmd, token, status and length of protobuf."""
  def __init__(self):
    self.cmd = 0
    self.token = 0
    self.status = 0
    self.length_protobuf = 0

  def sendHeader(self, s):
    """Send the header to the socket

    Args:
      s: socket

    Returns
      nothing
    """
    mh = msgheader_pb2.MsgHeader()
    mh.cmd = self.cmd
    mh.token = self.token
    mh.status = self.status
    mh.length_data = self.length_protobuf
    mhser = mh.SerializeToString()
    len_msg_header_raw = struct.pack('<i', len(mhser))
    sendall(s, len_msg_header_raw)
    sendall(s, mhser)

  def recvHeader(self, s):
    """Receive the header from the socket"""
    len_msg_header_raw = recvall(s, 4)
    len_msg_hdr = struct.unpack('<i', len_msg_header_raw)
    mh = msgheader_pb2.MsgHeader()
    mh_raw = recvall(s, len_msg_hdr[0])
    mh.ParseFromString(mh_raw)
    self.cmd = mh.cmd
    self.token = mh.token
    self.status = mh.status
    self.length_protobuf = mh.length_data;

class Msg:
  """A message consists of a fixed length MsgHeader followed by a protobuf.

  This class sends and receives messages, when sending the status
  will be zero and when receiving the protobuf field will be an
  empty string if there was no protobuf.
  """
  def __init__(self):
    """Initialize the protobuf to None and header to an empty"""
    self.protobuf = None
    self.header = MsgHeader()

    # Keep a local copy of header for convenience
    self.cmd = 0
    self.token = 0
    self.status = 0

  def sendMsg(self, s, cmd, token, protobuf=''):
    """Send a message to a socket

    Args:
      s: socket
      cmd: command to send
      token: token to send, will be returned unmodified
      protobuf: optional protobuf to send

    Returns
      nothing
    """
    self.cmd = cmd
    self.token = token
    self.status = 0
    self.protobuf = protobuf

    self.header.cmd = self.cmd
    self.header.token = self.token
    self.header.status = self.status
    if (len(protobuf) > 0):
      self.header.length_protobuf = len(protobuf)
    else:
      self.header.length_protobuf = 0
      self.protobuf = ''
    self.header.sendHeader(s)
    if (self.header.length_protobuf > 0):
      sendall(s, self.protobuf)

  def recvMsg(self, s):
    """Receive a message from a socket

    Args:
      s: socket

    Returns:
      nothing
    """
    self.header.recvHeader(s)
    self.cmd = self.header.cmd
    self.token = self.header.token
    self.status = self.header.status
    if (self.header.length_protobuf > 0):
      self.protobuf = recvall(s, self.header.length_protobuf)
    else:
      self.protobuf = ''

def main(argv):
  """Create a socket and connect.

  Before using you'll need to forward the port
  used by mock_ril, 54312 to a port on the PC.
  The following worked for me:

    adb forward tcp:11111 tcp:54312.

  Then you can execute this test using:

    tms.py 127.0.0.1 11111
  """
  s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
  host = sys.argv[1]        # server address
  print "host=%s" % host
  port = int(sys.argv[2])   # server port
  print "port=%d" % port
  s.connect((host, port))

  # Create an object which is serializable to a protobuf
  rrs = ctrl_pb2.CtrlRspRadioState()
  rrs.state = ril_pb2.RADIOSTATE_UNAVAILABLE
  print "rrs.state=%d" % (rrs.state)

  # Serialize
  rrs_ser = rrs.SerializeToString()
  print "len(rrs_ser)=%d" % (len(rrs_ser))

  # Deserialize
  rrs_new = ctrl_pb2.CtrlRspRadioState()
  rrs_new.ParseFromString(rrs_ser)
  print "rrs_new.state=%d" % (rrs_new.state)


  # Do an echo test
  req = Msg()
  req.sendMsg(s, 0, 1234567890123, rrs_ser)
  resp = Msg()
  resp.recvMsg(s)
  response = ctrl_pb2.CtrlRspRadioState()
  response.ParseFromString(resp.protobuf)

  print "cmd=%d" % (resp.cmd)
  print "token=%d" % (resp.token)
  print "status=%d" % (resp.status)
  print "len(protobuf)=%d" % (len(resp.protobuf))
  print "response.state=%d" % (response.state)
  if ((resp.cmd == 0) & (resp.token == 1234567890123) &
        (resp.status == 0) & (response.state == 1)):
    print "SUCCESS: echo ok"
  else:
    print "ERROR: expecting cmd=0 token=1234567890123 status=0 state=1"

  # Test CTRL_GET_RADIO_STATE
  req.sendMsg(s, ctrl_pb2.CTRL_CMD_GET_RADIO_STATE, 4)
  resp = Msg()
  resp.recvMsg(s)

  print "cmd=%d" % (resp.cmd)
  print "token=%d" % (resp.token)
  print "status=%d" % (resp.status)
  print "length_protobuf=%d" % (len(resp.protobuf))

  if (resp.cmd == ctrl_pb2.CTRL_CMD_GET_RADIO_STATE):
      response = ctrl_pb2.CtrlRspRadioState()
      response.ParseFromString(resp.protobuf)
      print "SUCCESS: response.state=%d" % (response.state)
  else:
      print "ERROR: expecting resp.cmd == ctrl_pb2.CTRL_CMD_GET_RADIO_STATE"

  # Close socket
  print "closing socket"
  s.close()


if __name__ == '__main__':
  main(sys.argv)

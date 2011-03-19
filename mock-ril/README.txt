Mock-ril:

Install:

The protoc is now part of the Android build but its
called "aprotoc" so it doesn't conflict with versions
already installed. If you wish to install it permanetly
see external/protobuf/INSTALL.txt and
external/protobuf/python/README.txt.  If you get
"from google.protobuf import xxxx" statements that
google.protobuf is not found, you didn't install the
python support for protobuf. Also on Mac OSX I got an
error running the protoc tests but installing was fine.

Running/testing:

See "Testing a new ril:" below for general instructions but
for the mock-ril I've added some targets to the Makefile to
ease testing. Also Makefile needs to know the device being
used as this determines the directory where files are found
and stored. ANDROID_DEVICE is an environment variable and
maybe either exported:
   $ export ANDROID_DEVICE=stingray

or it can be passed on the command line:
   $ make clean ANDROID_DEVICE=stingray

If it's not set "passion" is the default.

Execute the "first" target first to setup appropriate
environment:
  $ cd hardware/ril/mock-ril
  $ make first

If changes made to ".proto" files run make with the default
"all" target:
  $ make

If changes are made to "c++" file create a new library and
run the "test" target:
  $ mm
  $ make test

If changes to only the execute "js" target:
  $ make js

To run the test control server:
  $ make tcs

Implementation:

The mock-ril is a library where the ril is implemented primarily
in javascript, mock-ril.js. In addition it can be controlled by
sending messages from another computer to port 54312 (TODO make
programmable) to the ctrlServer, a Worker in In mock-ril.js.

See mock_ril.js for additional documentation.

files:
  ctrl.proto                    Protobuf messages for the control server
  ctrl.*                        Protobuf generated files.
  ctrl_pb2.py                   Python files generated from ctrl.proto
  ctrl_server.*                 Cpp interface routines between ctrlServer
                                in javascript and the controller.
  experiments.*                 Early experiments
  js_support.*                  Java script support methods. Exposes various
                                routines to javascript, such as print, readFile
                                and include.
  logging.h                     LOG_TAG and include utils/log.h
  mock_ril.[cpp|h]              Main module inteface code.
  mock_ril.js                   The mock ril
  node_buffer.*                 A Buffer for communicating between c++ and js.
                                This was ported from nodejs.org.
  node_object.*                 An object wrapper to make it easier to expose
                                c++ code to js. Ported from nodejs.org.
  node_util.*                   Some utilities ported from nodejs.org.
  protobuf_v8.*                 Protobuf code for javascript ported from
                                http://code.google.com/p/protobuf-for-node/.
  requests.*                    Interface code for handling framework requests.
  responses*                    Interface code for handling framework responses.
  ril.proto                     The protobuf version of ril.h
  ril_vars.js                   Some additional variables defined for enums in
                                ril.h.
  ril_pb2.py                    Python files generated from ril.proto.
  status.h                      STATUS constants.
  tcs.py                        Test the ctrlServer.
  util.*                        Utility routines
  worker.*                      Define WorkerThread and WorkerQueue.
  worker_v8.*                   Expose WorkerQueue to js.


TODO: more documentation.


Testing a new ril:

The Makefile is used to generate files and make testing easier.
and there are has several targets:

all         runs protoc and generates files, ril.desc ril.pb.*

clean       target removes generated files.

first       changes to root, remounts r/w and copies some files.

test        copies the latest libmock_ril.so and kills rild
            to run the new mockril

General instructions for testing ril's:

1) On the device login in as root and remount file system so it's read/write:
     $ adb root
     restarting adbd as root

     $ adb remount
     remount succeeded

2) Set rild.libpath to the name of the ril:
     adb shell setprop rild.libpath /system/lib/libmock_ril.so

  Using setprop makes the change temporary and the old ril will be
  used after rebooting. (Another option is to set rild.libpath in
  /data/local.prop, but don't forget to reboot for it to take effect).

3) Compile and copy the ril to /system/lib/:
   adb push out/target/product/passion/system/lib/libmock_ril.so /system/lib/

4) To restart the ril, kill the currently running ril and the new one
   will automatically be restarted. You can use the ps command to find
   /system/bin/rild PID, 3212 below and kill it:
     $ adb shell ps | grep rild
     radio     3212  1     3224   628   ffffffff afd0e4fc S /system/bin/rild

     $ adb shell kill 3212

   or

     $ adb shell setprop ctl.restart ril-daemon

5) Make modifications, go to step 3.

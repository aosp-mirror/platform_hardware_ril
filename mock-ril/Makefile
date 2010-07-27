# This has various targets to assist in developing
# and testing the mock ril. The default "all" target
# invokes protoc to generate the appropriate protobuf
# code. The other targest are used for testing.

# Assume this Makefile is run in place then top
# point to the top of the android source tree
top=../../../

# A Makefile to run protoc and simplify testing.
.PHONY : all
all : msgheader.pb.cpp msgheader.pb.h msgheader.desc msgheader_pb2.py com/android/internal/communication/MsgHeader.java \
       ril.pb.cpp ril.pb.h ril.desc ril_pb2.py com/android/internal/telephony/ril_proto/RilCmds.java \
       ctrl.pb.cpp ctrl.pb.h ctrl.desc ctrl_pb2.py com/android/internal/telephony/ril_proto/RilCtrlCmds.java
option java_package='com.android.internal.ril_proto';

# TODO: Document/cleanup these testing targets as we learn whats needed.

# Make the unit tests
.PHONY : ut
ut :
	source $(top)build/envsetup.sh ; mmm $(top)frameworks/base/telephony/tests/telephonytests
	adb install -r $(top)out/target/product/passion/data/app/FrameworksTelephonyTests.apk

# Run protoc to create the c++ files for msgheader
msgheader.pb.cpp pmsgheader.pb.h : msgheader.proto
	protoc --cpp_out=. $<
	mv msgheader.pb.cc msgheader.pb.cpp

# Run protoc to create the python files for msgheader
msgheader.desc : msgheader.proto
	protoc --cpp_out=. $<
	protoc --descriptor_set_out=$@ --include_imports $<

# Run protoc to create the pyhton files for msgheader
msgheader_pb2.py : msgheader.proto
	protoc --python_out=. $<

# Run protoc to create the java files for ril
com/android/internal/communication/MsgHeader.java : msgheader.proto
	protoc --javamicro_out=. $<

# Run protoc to create the c++ files for ril
ril.pb.cpp ril.pb.h : ril.proto
	protoc --cpp_out=. $<
	mv ril.pb.cc ril.pb.cpp

# Run protoc to create the ril descriptor file for ril
ril.desc : ril.proto
	protoc --descriptor_set_out=$@ --include_imports $<

# Run protoc to create the pyhton files for ril
ril_pb2.py : ril.proto
	protoc --python_out=. $<

# Run protoc to create the java files for ril
com/android/internal/telephony/ril_proto/RilCmds.java : ril.proto
	protoc --javamicro_out=. $<

# Run protoc to create the c++ files for control
ctrl.pb.cpp ctrl.pb.h : ctrl.proto
	protoc --cpp_out=. $<
	mv ctrl.pb.cc ctrl.pb.cpp

# Run protoc to create the pyhton files for control
ctrl_pb2.py : ctrl.proto
	protoc --python_out=. $<

# Run protoc to create the ctrl descriptor file for control
ctrl.desc : ctrl.proto
	protoc --descriptor_set_out=$@ --include_imports $<

# Run protoc to create the java files for ril
com/android/internal/telephony/ril_proto/RilCtrlCmds.java : ctrl.proto
	protoc --javamicro_out=. $<

# After starting phone do this first to get lastest ril.desc/proto and setup rild
.PHONY : first
first : root_remount copy_all forward

# copy js and descriptors, restart rild and run mockril_tests
.PHONY : tmr
tmr : copy_js_desc restart_rild mockril_tests

# Copy all files, restart rild and run mockril_tests
.PHONY : test
test : copy_all restart_rild install_mockril_tests mockril_tests

# Restart rild
.PHONY : restart_rild
restart_rild :
	adb shell ps | awk '/rild/ { print $$2 }' | xargs adb shell kill

# Update only the js/copy and restart rild
.PHONY : tjs
tjs : copy_js_desc restart_rild

# Run test control server python script
.PHONY : tcs
tcs :
	./tcs.py 127.0.0.1 11111

# Run the mock ril tests (use adb shell pm list instrumentation to see the Runner)
.PHONY : mockril_tests
mockril_tests :
	adb  shell am instrument -e class 'com.android.internal.telephony.mockril.MockRilTest' -w com.android.frameworks.telephonytests/.TelephonyMockRilTestRunner

# forward the control server tcp port (54312) to a port on the PC side (11111)
.PHONY : forward
forward :
	adb forward tcp:11111 tcp:54312

# change to root and remount device
.PHONY : root_remount
root_remount :
	adb root ; sleep 3 ; adb remount ; adb shell setprop rild.libpath /data/lib/libmock_ril.so

# Copy all files
.PHONY : copy_all
copy_all : copy_js_desc copy_mockril

# Copy js and the protobuf descriptor files
.PHONY : copy_js_desc
copy_js_desc :
	adb push mock_ril.js /sdcard/data/
	adb push ril_vars.js /sdcard/data/
	adb push ril.desc /sdcard/data/
	adb push ctrl.desc /sdcard/data/
	adb forward tcp:11111 tcp:54312

# Copy the mock ril library
.PHONY : copy_mockril
copy_mockril :
	adb push $(top)out/target/product/passion/system/lib/libmock_ril.so /data/lib/

.PHONY : install_mockril_tests
install_mockril_tests :
	adb install -r $(top)out/target/product/passion/data/app/FrameworksTelephonyTests.apk

# Remove generated files
.PHONY : clean
clean :
	rm -f ril.pb.cpp ril.pb.h ril.desc ril_pb2.py
	rm -f com/android/internal/telephony/ril_proto/RilCmds.java
	rm -f ctrl.pb.cpp ctrl.pb.h ctrl.desc ctrl_pb2.py
	rm -f com/android/internal/telephony/ril_proto/RilCtrlCmds.java
	rm -f msgheader.pb.cpp msgheader.pb.h msgheader.desc msgheader_pb2.py
	rm -f com/android/internal/communication/MsgHeader.java

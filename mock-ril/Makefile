# This has various targets to assist in developing
# and testing the mock ril. The default "all" target
# invokes protoc to generate the appropriate protobuf
# code. The other targest are used for testing.

# Assume this Makefile is run in place then top
# point to the top of the android source tree
top=../../../

PROTOC=$(wildcard $(top)out/host/*/bin/aprotoc)
#if ((($(words $(PROTO)) != 1)))
ifneq ($(words $(PROTOC)),1)
$(error expecting 1 protoc we have $(words $(PROTOC)), PROTOC='$(PROTOC)')
endif

ifeq ("$(ANDROID_DEVICE)","")
$(warning Default to ANDROID_DEVICE=passion")
ANDROID_DEVICE=passion
endif

# Directories of source files
device=$(ANDROID_DEVICE)
src_js=src/js
src_proto=src/proto
src_generated=$(top)out/target/product/$(device)/obj/SHARED_LIBRARIES/libmock_ril_intermediates/proto/hardware/ril/mock-ril/src/proto

# Directories of generated source files
gen_src_py=$(src_generated)/python
gen_src_desc=$(src_generated)/desc

# Generated files
generated=$(gen_src_desc)/msgheader.desc $(gen_src_py)/msgheader_pb2.py \
      $(gen_src_desc)/ril.desc $(gen_src_py)/ril_pb2.py \
      $(gen_src_desc)/ctrl.desc $(gen_src_py)/ctrl_pb2.py

# A Makefile to run protoc and simplify testing.
.PHONY : all
all : $(generated)

# TODO: Document/cleanup these testing targets as we learn whats needed.

# Make the unit tests
.PHONY : ut
ut :
	source $(top)build/envsetup.sh ; mmm $(top)frameworks/base/telephony/tests/telephonytests
	adb install -r $(top)out/target/product/$(device)/data/app/FrameworksTelephonyTests.apk

t : $(gen_src_desc)/msgheader.desc $(gen_src_py)/msgheader_pb2.py

# Run protoc to create the descriptor files for msgheader
$(gen_src_desc)/msgheader.desc : $(src_proto)/msgheader.proto
	mkdir -p $(gen_src_desc)
	$(PROTOC) --descriptor_set_out=$@ --proto_path=$(src_proto) --include_imports $<

# Run protoc to create the python files for msgheader
$(gen_src_py)/msgheader_pb2.py : $(src_proto)/msgheader.proto
	mkdir -p $(gen_src_py)
	$(PROTOC) --python_out=$(gen_src_py) --proto_path=$(src_proto) $<

# Run protoc to create the ril descriptor file for ril
$(gen_src_desc)/ril.desc : $(src_proto)/ril.proto
	mkdir -p $(gen_src_desc)
	$(PROTOC) --descriptor_set_out=$@ --proto_path=$(src_proto) --include_imports $<

# Run protoc to create the python files for ril
$(gen_src_py)/ril_pb2.py : $(src_proto)/ril.proto
	mkdir -p $(gen_src_py)
	$(PROTOC) --python_out=$(gen_src_py) --proto_path=$(src_proto) $<

# Run protoc to create the python files for control
$(gen_src_py)/ctrl_pb2.py : $(src_proto)/ctrl.proto
	mkdir -p $(gen_src_py)
	$(PROTOC) --python_out=$(gen_src_py) --proto_path=$(top) --proto_path=$(src_proto) $<

# Run protoc to create the ctrl descriptor file for control
$(gen_src_desc)/ctrl.desc : $(src_proto)/ctrl.proto
	mkdir -p $(gen_src_desc)
	$(PROTOC) --descriptor_set_out=$@ --proto_path=$(top) --proto_path=$(src_proto) --include_imports $<

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
	adb shell setprop ctl.restart ril-daemon

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
	adb push $(src_js)/mock_ril.js /sdcard/data/
	adb push $(src_js)/mock_ril_tests.js /sdcard/data/
	adb push $(src_js)/simulated_radio.js /sdcard/data/
	adb push $(src_js)/simulated_radio_tests.js /sdcard/data/
	adb push $(src_js)/simulated_icc.js /sdcard/data/
	adb push $(src_js)/simulated_icc_tests.js /sdcard/data/
	adb push $(src_js)/ctrl_server.js /sdcard/data/
	adb push $(src_js)/ctrl_server_tests.js /sdcard/data/
	adb push $(src_js)/ril_vars.js /sdcard/data/
	adb push $(gen_src_desc)/ril.desc /sdcard/data/
	adb push $(gen_src_desc)/ctrl.desc /sdcard/data/
	adb forward tcp:11111 tcp:54312


# Copy the mock ril library
.PHONY : copy_mockril
copy_mockril :
	adb push $(top)out/target/product/$(device)/system/lib/libmock_ril.so /data/lib/

.PHONY : install_mockril_tests
install_mockril_tests :
	adb install -r $(top)out/target/product/$(device)/data/app/FrameworksTelephonyTests.apk

# Remove generated files
.PHONY : clean
clean :
	rm -f $(generated)

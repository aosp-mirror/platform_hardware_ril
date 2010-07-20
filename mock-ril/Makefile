# A Makefile to run protoc and also allow testing.
.PHONY : all
all : ril.pb.cpp ril.pb.h ril.desc ril_pb2.py ctrl.pb.cpp ctrl.pb.h ctrl.desc ctrl_pb2.py

# Run protoc to create the c++ files for ril
ril.pb.cpp ril.pb.h : ril.proto
	protoc --cpp_out=. $<
	mv ril.pb.cc ril.pb.cpp

# Run protoc to create the pyhton files for ril
ril_pb2.py : ril.proto
	protoc --python_out=. $<

# Run protoc to create the ril descriptor file for ril
ril.desc : ril.proto
	protoc --descriptor_set_out=$@ --include_imports $<

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

# After starting phone do this first to get lastest ril.desc/proto and setup rild
.PHONY : first
first :
	adb root ; sleep 3 ; adb remount ; adb shell setprop rild.libpath /data/lib/libmock_ril.so
	adb push mock_ril.js /sdcard/data/
	adb push ril_vars.js /sdcard/data/
	adb push ril.desc /sdcard/data/
	adb push ctrl.desc /sdcard/data/
	adb forward tcp:11111 tcp:54312

.PHONY : js
js :
	adb push mock_ril.js /sdcard/data/
	adb push ril_vars.js /sdcard/data/
	adb push ril.desc /sdcard/data/
	adb push ctrl.desc /sdcard/data/
	adb shell ps | awk '/rild/ { print $$2 }' | xargs adb shell kill

.PHONY : tcs
tcs :
	./tcs.py 127.0.0.1 11111

# Push a new libmock_ril.so and kill rild to run a test
.PHONY : test
test :
	adb push mock_ril.js /sdcard/data/
	adb push ril_vars.js /sdcard/data/
	adb push ril.desc /sdcard/data/
	adb push ctrl.desc /sdcard/data/
	adb push ../../../out/target/product/passion/system/lib/libmock_ril.so /data/lib/
	adb shell ps | awk '/rild/ { print $$2 }' | xargs adb shell kill

# Push a new libmock_ril.so and kill rild to run a test
.PHONY : mockriltest
mockriltest :
	adb push mock_ril.js /sdcard/data/
	adb push ril_vars.js /sdcard/data/
	adb push ril.desc /sdcard/data/
	adb push ctrl.desc /sdcard/data/
	adb shell ps | awk '/rild/ { print $$2 }' | xargs adb shell kill

# Remove generated files
.PHONY : clean
clean :
	rm -rf ril.pb.cpp ril.pb.h ril.desc ril_pb2.py
	rm -rf ctrl.pb.cpp ctrl.pb.h ctrl.desc ctrl_pb2.py

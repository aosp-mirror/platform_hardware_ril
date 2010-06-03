Testing a new ril:

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

5) Make modifications, go to step 3.

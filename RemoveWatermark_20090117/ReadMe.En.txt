Remove all Watermark on desktop.
Such as "Evaluation Copy", "For testing purpose only", "Test Mode".

Version:  0.4,  01/17/2008
Support:  Windows Vista /Server 2008 /Windows 7,  64bit(x64) / 32bit(x86)
          All Service Pack & all language of Windows.

Author:   deepxw
E-mail:   deepxw#126.com
Blog:     http://deepxw.lingd.net
          http://deepxw.blogspot.com   (English)

It is a universal patch. Without language limited, Supports all language of windows!
Without limited of Service Pack.

This tool provides two ways to remove the watermark.

* The default method, modify user32.dll.mui. This method is safe.
In 64-bit Vista / Windows 7, It needs Re-Build MUI cache, this will take a few minutes, please wait.

* Method 2: modifies user32.dll. 100% remove all watermark. (Run program with argument "-enforce")

But don't use Method 2 in Windows 7 6956 or later version.
I likes a unknown bug in these version of Windows.
Any modification with user32.dll in Windows 7 6956 will cause application fail to run in compatibility mode.
Windows 7 server / Vista is no problem.


Notes:
1) Can operate in normal mode. Do not need to enter safe mode.

2) Choose the corresponding patch based on you Windows:
   For 32bit(x86):   RemoveWatermarkX86.exe
   For 64bit(x64):   RemoveWatermarkX64.exe

3) Require administrator rights. Right-click the exe file, select Run as Administrator.

4) Enter "Y" to confirm patch.
   "user32.dll.mui" will be backup as \windows\system32\en-US\user32.dll.mui.backup.
   The string "en-US" depends on system language.

5) After patch, restart computer to take effect.

6) You can run program with argument "-silent" to patch in silent mode.


History:

2008.11.04 V0.2
  Test in Windows Vista SP2 16497 / Windows 7 M3 6801, OK.

2008.12.10 V0.3
   + Add support argument "-silent" to patch in silent mode.
   Test in Windows 7 6956, OK.

2009.01.17 V0.4
   + Support modifies user32.dll.mui to remove watermark string.
   * The default method, from modify user32.dll, change to modify user32.dll.mui.

   Test in Windows 7 7000, OK.

Copy the Cypress CyAPI C++ library into THIS folder.

CyAPI comes with the Cypress (Infineon) EZ-USB FX3 SDK or the older "SuiteUSB"
package. It is Cypress copyrighted and therefore not included here.
Typical location (FX3 SDK 1.3.x):

    C:\Program Files (x86)\Cypress\EZ-USB FX3 SDK\1.3\library\cpp\

Copy so that this folder looks like:

    cyapi\inc\CyAPI.h            (+ every other .h in library\cpp\inc :
    cyapi\inc\cyioctl.h             cyioctl.h, CyUSB30_def.h, UsbdStatus.h,
    cyapi\inc\CyUSB30_def.h         VersionNo.h ...)
    cyapi\inc\UsbdStatus.h
    cyapi\inc\VersionNo.h
    cyapi\lib\x64\CyAPI.lib      (from library\cpp\lib\x64)
    cyapi\lib\x86\CyAPI.lib      (from library\cpp\lib\x86)

The Visual Studio project CyBridgeTest\CyBridgeTest.vcxproj uses
..\cyapi\inc and ..\cyapi\lib\<x64|x86>.

Driver: the same SDK contains the signed Cypress driver
    ...\EZ-USB FX3 SDK\1.3\driver\bin\<OS>\<x64|x86>\cyusb3.inf / cyusb3.sys
Install it for the device 04B4:1004 (Device Manager -> Update driver ->
Browse -> select the folder). If your cyusb3.inf does not list
VID_04B4&PID_1004, add a line for it in the [Device.NTamd64] section
(this breaks the driver signature, so test-signing mode is then needed).

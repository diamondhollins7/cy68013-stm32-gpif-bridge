Copy the Cypress EZ-USB FX2LP firmware framework into THIS folder.

The files come with the Cypress (Infineon) CY3684 EZ-USB FX2LP Development Kit
software. They are Cypress copyrighted and are therefore not included here.
Typical install location:

    C:\Cypress\USB\CY3684_EZ-USB_FX2LP_DVK\1.1\Target\

Files needed (6) and where they usually are inside Target\ :

    fw.c          Target\Fw\LP\fw.c            framework main loop / SETUP handling
    fx2.h         Target\Inc\fx2.h             framework definitions
    fx2regs.h     Target\Inc\fx2regs.h         FX2LP register definitions
    syncdly.h     Target\Inc\syncdly.h         SYNCDELAY macro
    EZUSB.LIB     Target\Lib\LP\EZUSB.LIB      framework library (EZUSB_Delay, ...)
    USBJmpTb.OBJ  Target\Lib\LP\USBJmpTb.OBJ   USB / GPIF interrupt jump table

(If your kit version uses different sub-folders, search Target\ for the names.)

The Keil project FX2_GPIF8_Bridge.uvproj references them as .\cypress_fw\<name>.
Do NOT copy dscr.a51 or a periph/bulkloop .c file - this project provides its
own (src\dscr.a51, src\bridge.c).

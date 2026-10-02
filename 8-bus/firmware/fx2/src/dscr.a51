;;-----------------------------------------------------------------------------
;; dscr.a51 - USB descriptors for the FX2LP 8-bit GPIF bridge
;;            (Cypress FX2LP framework format, assembled with Keil A51)
;;
;;   VID/PID 04B4:1004 - Cypress sample ID listed in cyusb3.inf, so the
;;   Cypress driver (CyUSB3.sys) binds without editing the INF.
;;   For a product use your own VID/PID (and add it to the INF).
;;
;; Note: A51 'dw' stores the HIGH byte first, USB is little-endian, so
;;       dw 0B404H produces the bytes B4 04 = 0x04B4.
;; All descriptors have an even length and the segment is PAGE aligned,
;; which keeps every descriptor word aligned as SUDPTR requires.
;;-----------------------------------------------------------------------------

DSCR_DEVICE      equ   1
DSCR_CONFIG      equ   2
DSCR_STRING      equ   3
DSCR_INTRFC      equ   4
DSCR_ENDPNT      equ   5
DSCR_DEVQUAL     equ   6

DSCR_DEVICE_LEN  equ   18
DSCR_CONFIG_LEN  equ   9
DSCR_INTRFC_LEN  equ   9
DSCR_ENDPNT_LEN  equ   7
DSCR_DEVQUAL_LEN equ   10

ET_CONTROL       equ   0
ET_ISO           equ   1
ET_BULK          equ   2
ET_INT           equ   3

public      DeviceDscr, DeviceQualDscr, HighSpeedConfigDscr, FullSpeedConfigDscr, StringDscr, UserDscr

DSCR        SEGMENT   CODE PAGE

;;-----------------------------------------------------------------------------
;; Global Variables
;;-----------------------------------------------------------------------------
            rseg DSCR

DeviceDscr:
      db   DSCR_DEVICE_LEN         ;; bLength
      db   DSCR_DEVICE             ;; bDescriptorType
      dw   0002H                   ;; bcdUSB 2.00
      db   00H                     ;; bDeviceClass (per interface)
      db   00H                     ;; bDeviceSubClass
      db   00H                     ;; bDeviceProtocol
      db   64                      ;; bMaxPacketSize0
      dw   0B404H                  ;; idVendor  0x04B4
      dw   0410H                   ;; idProduct 0x1004
      dw   0002H                   ;; bcdDevice 2.00
      db   1                       ;; iManufacturer
      db   2                       ;; iProduct
      db   3                       ;; iSerialNumber
      db   1                       ;; bNumConfigurations

DeviceQualDscr:
      db   DSCR_DEVQUAL_LEN
      db   DSCR_DEVQUAL
      dw   0002H                   ;; bcdUSB 2.00
      db   00H, 00H, 00H
      db   64                      ;; bMaxPacketSize0
      db   1                       ;; bNumConfigurations
      db   0                       ;; bReserved

;; ---------------------------------------------------------------------
HighSpeedConfigDscr:
      db   DSCR_CONFIG_LEN         ;; bLength
      db   DSCR_CONFIG             ;; bDescriptorType (patched to 7 for other-speed)
      db   (HighSpeedConfigDscrEnd-HighSpeedConfigDscr) mod 256   ;; wTotalLength
      db   (HighSpeedConfigDscrEnd-HighSpeedConfigDscr)  /  256
      db   1                       ;; bNumInterfaces
      db   1                       ;; bConfigurationValue
      db   0                       ;; iConfiguration
      db   80H                     ;; bmAttributes: bus powered
      db   7DH                     ;; bMaxPower: 125 x 2 mA = 250 mA

;; Interface 0, alternate 0: vendor specific, 2 endpoints
      db   DSCR_INTRFC_LEN
      db   DSCR_INTRFC
      db   0                       ;; bInterfaceNumber
      db   0                       ;; bAlternateSetting
      db   2                       ;; bNumEndpoints
      db   0FFH                    ;; bInterfaceClass: vendor
      db   00H
      db   00H
      db   0                       ;; iInterface

;; EP2 OUT bulk (PC -> STM32)
      db   DSCR_ENDPNT_LEN
      db   DSCR_ENDPNT
      db   02H                     ;; bEndpointAddress
      db   ET_BULK
      db   00H, 02H                ;; wMaxPacketSize
      db   00H                     ;; bInterval

;; EP6 IN bulk (STM32 -> PC)
      db   DSCR_ENDPNT_LEN
      db   DSCR_ENDPNT
      db   86H
      db   ET_BULK
      db   00H, 02H
      db   00H
HighSpeedConfigDscrEnd:

;; ---------------------------------------------------------------------
FullSpeedConfigDscr:
      db   DSCR_CONFIG_LEN         ;; bLength
      db   DSCR_CONFIG             ;; bDescriptorType (patched to 7 for other-speed)
      db   (FullSpeedConfigDscrEnd-FullSpeedConfigDscr) mod 256   ;; wTotalLength
      db   (FullSpeedConfigDscrEnd-FullSpeedConfigDscr)  /  256
      db   1                       ;; bNumInterfaces
      db   1                       ;; bConfigurationValue
      db   0                       ;; iConfiguration
      db   80H                     ;; bmAttributes: bus powered
      db   7DH                     ;; bMaxPower: 125 x 2 mA = 250 mA

;; Interface 0, alternate 0: vendor specific, 2 endpoints
      db   DSCR_INTRFC_LEN
      db   DSCR_INTRFC
      db   0                       ;; bInterfaceNumber
      db   0                       ;; bAlternateSetting
      db   2                       ;; bNumEndpoints
      db   0FFH                    ;; bInterfaceClass: vendor
      db   00H
      db   00H
      db   0                       ;; iInterface

;; EP2 OUT bulk (PC -> STM32)
      db   DSCR_ENDPNT_LEN
      db   DSCR_ENDPNT
      db   02H                     ;; bEndpointAddress
      db   ET_BULK
      db   40H, 00H                ;; wMaxPacketSize
      db   00H                     ;; bInterval

;; EP6 IN bulk (STM32 -> PC)
      db   DSCR_ENDPNT_LEN
      db   DSCR_ENDPNT
      db   86H
      db   ET_BULK
      db   40H, 00H
      db   00H
FullSpeedConfigDscrEnd:

;; ---------------------------------------------------------------------
StringDscr:

StringDscr0:
      db   StringDscr0End-StringDscr0
      db   DSCR_STRING
      db   09H, 04H                ;; LANGID en-US
StringDscr0End:

StringDscr1:
      db   StringDscr1End-StringDscr1
      db   DSCR_STRING
      db   'O',00, 'p',00, 'e',00, 'n',00, 'B',00, 'r',00, 'i',00, 'd',00
      db   'g',00, 'e',00
StringDscr1End:

StringDscr2:
      db   StringDscr2End-StringDscr2
      db   DSCR_STRING
      db   'F',00, 'X',00, '2',00, '-',00, 'S',00, 'T',00, 'M',00, '3',00
      db   '2',00, ' ',00, '8',00, '-',00, 'b',00, 'i',00, 't',00, ' ',00
      db   'B',00, 'r',00, 'i',00, 'd',00, 'g',00, 'e',00
StringDscr2End:

StringDscr3:
      db   StringDscr3End-StringDscr3
      db   DSCR_STRING
      db   '0',00, '0',00, '0',00, '1',00
StringDscr3End:

UserDscr:
      dw   0000H
      end

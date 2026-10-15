#include <gccore.h>
#define USB_CLASS_HID 3
typedef struct {s32 device_id;u16 vid,pid;u32 token;} usb_device_entry;
typedef s32 (*usbcallback)(s32,void *);
s32 USB_Initialize(void);
s32 USB_CloseDevice(s32 *);
s32 USB_GetDeviceList(usb_device_entry *,u8,u8,u8 *);
s32 USB_OpenDevice(s32,u16,u16,s32 *);
s32 USB_WriteIntrMsgAsync(s32,u8,u16,void *,usbcallback,void *);
s32 USB_ReadIntrMsgAsync(s32,u8,u16,void *,usbcallback,void *);

#ifndef GEKKO2_CONTROLLER_USB_H
#define GEKKO2_CONTROLLER_USB_H
#include "core/hw/controller_input.h"
void controller_usb_init(void);
void controller_usb_scan(controller_pad pads[4]);
void controller_usb_shutdown(void);
int controller_usb_status(void);
#endif

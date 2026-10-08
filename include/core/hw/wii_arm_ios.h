#ifndef GEKKO2_WII_ARM_IOS_H
#define GEKKO2_WII_ARM_IOS_H
/* Startup only, before BIOS/disc/log handles and worker jobs exist.
 * 1 selected MLOAD IOS, 0 unchanged, negative reason for CPU fallback. */
int wii_arm_ios_start(unsigned slot,int *mounted);
int wii_arm_ios_status(void);
unsigned wii_arm_ios_original(void);
unsigned wii_arm_ios_active(void);
const char *wii_arm_ios_message(void);
#endif

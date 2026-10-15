#ifndef GEKKO2_IOP_SERVICE_HLE_H
#define GEKKO2_IOP_SERVICE_HLE_H
#include "core/iop/iop_core.h"
void iop_service_hle_reset(void);
void iop_service_hle_register(iop_state_t *st,const char *library,uint32_t table,unsigned count);
int iop_service_hle_try_handle(iop_state_t *st,uint32_t pc);
uint64_t iop_service_hle_stat(unsigned index);
#endif

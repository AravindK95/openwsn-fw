#ifndef __CMOTE_H
#define __CMOTE_H

/**
 Why is this included here?
 */
#include "opencoap.h"

//=========================== define ==========================================

//=========================== typedef =========================================

typedef struct {
   coap_resource_desc_t desc;
   opentimer_id_t timerId;
} cmote_vars_t;

//=========================== variables =======================================

//=========================== prototypes ======================================

void cmote_init(void);

#endif

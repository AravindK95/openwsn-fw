/**
 * Test program for developing a low-power sensor measuring the current
 * flow from S. Oneidensis or E. Coli bacteria. Uses an external DAC, digital
 * potentiometer, and op-amp.
 */

#include "opendefs.h"
#include "cmote.h"
#include "opencoap.h"
#include "opentimers.h"
#include "packetfunctions.h"
#include "openqueue.h"
#include "openserial.h"			// Only needed for debugging
#include "scheduler.h"
#include "idmanager.h"
#include "IEEE802154E.h"
// peripherals
#include "adc.h"
#include "ssi.h"

//=========================== defines =========================================

#define CMOTEPERIOD  15000		// inter-packet period (in ms)
#define PAYLOADLEN      40		// what is the limitation on payload length?

const uint8_t cmote_path0[] = "cmote";

//=========================== variables =======================================

cmote_vars_t cmote_vars;

//=========================== prototypes ======================================

void 		cmote_timer_cb(void);
void 		cmote_task_cb(void);
owerror_t 	cmote_recieve(OpenQueueEntry_t* msg,
	  	  				  coap_header_iht*  coap_header,
	  	  				  coap_option_iht*  coap_options);
void 		cmote_sendDone(OpenQueueEntry_t* msg, owerror_t error);

//=========================== public ==========================================

void cmote_init() {
	// prepare the resource descriptor for the /cmote path
	cmote_vars.desc.path0len 			= sizeof(cmote_path0)-1;
	cmote_vars.desc.path0val 			= (uint8_t*) (&cmote_path0);
	cmote_vars.desc.path1len 			= 0;
	cmote_vars.desc.path1val 			= NULL;
	cmote_vars.desc.componentID 		= COMPONENT_CMOTE;
	cexample_vars.desc.discoverable     = TRUE;
	cmote_vars.desc.callbackRx 			= &cmote_recieve;
	cmote_vars.desc.callbackSendDone 	= &cmote_sendDone;

	// initialize ADC
	SOCADCSingleConfigure(SOCADC_12_BIT, SOCADC_REF_INTERNAL);

	// initialize SSI Bus

	// set DAC voltage

	// initialize potentiometer

	// Register and start timer
	opencoap_register(&cmote_vars.desc);
	cmote_vars.timerId = opentimers_start(CMOTEPERIOD,
										  TIMER_PERIODIC,
										  TIME_MS,
										  (opentimers_cbt) cmote_timer_cb);
}

//=========================== private =========================================

// Timer has fired, push task to scheduler with CoAP priority
void cmote_timer_cb() {
	scheduler_push_task(cmote_task_cb, TASKPRIO_COAP);
}

// Execute measurement and send results in a CoAP packet
void cmote_task_cb() {
	// declare variables
	OpenQueueEntry_t* pkt;
	owerror_t		  outcome;
	uint16_t		  adc_value;
	uint8_t			  i;

	// don't run if not synch
	if (ieee154e_isSynch() == FALSE) return;

	// don't run on DAGroot
	if (idmanager_getIsDAGroot()) {
		opentimers_stop(cmote_vars.timerId);
		return;
	}

	// Take ADC reading
	SOCADCSingleStart(SOCADC_AIN6);							// start adc conversion on PA6

	while(!SOCADCEndOfCOnversionGet());						// wait until conversion is done
	adc_value = SOCADCDataGet() >> SOCADC_12_BIT_RSHIFT;	// record results

	// Adjust ADC resolution

	// create a CoAP RD packet
	pkt = openqueue_getFreePacketBuffer(COMPONENT_CMOTE);
	if (pkt == NULL) {
		openserial_printError(COMPONENT_CMOTE, ERR_NO_FREE_PACKET_BUFFER,
							  (errorparameter_t) 0,
							  (errorparameter_t) 0);
		openqueue_freePacketBuffer(pkt);
		return;
	}

	// take ownership of the packet
	pkt->creator = COMPONENT_CMOTE;
	pkt->owner   = COMPONENT_CMOTE;

	// CoAP payload
	packetfunctions_reserveHeaderSize(pkt, PAYLOADLEN);
	for (i=0; i<PAYLOADLEN; i++) {
		pkt->payload[i] = i;		// why do we do this?
	}

	pkt->payload[0] = (adc_value>>8) & 0xFF;
	pkt->payload[1] = (adc_value>>0) & 0xFF;

	// Different examples have slightly different packet headers.
	// Where to find documentation on CoAP Packet format?

	// content-type option
	packetfunctions_reserveHeaderSize(pkt, 2);
	pkt->payload[0]                = (COAP_OPTION_NUM_CONTENTFORMAT - COAP_OPTION_NUM_URIPATH) << 4 | 1;
	pkt->payload[1]                = COAP_MEDTYPE_APPOCTETSTREAM;

	// location-path option
	packetfunctions_reserveHeaderSize(pkt, sizeof(cmote_path0)-1);
	memcpy(&pkt->payload[0], cmote_path0, sizeof(cmote_path0)-1);
	packetfunctions_reserveHeaderSize(pkt, 1);
	pkt->payload[0]                = ((COAP_OPTION_NUM_URIPATH) << 4) | (sizeof(cmote_path0)-1);

	// metadata
	pkt->l4_destination_port       = WKP_UDP_COAP;
	pkt->l3_destinationAdd.type    = ADDR_128B;
	memcpy(&pkt->l3_destinationAdd.addr_128b[0], &ipAddr_motesEecs, 16);

	// send
	outcome = opencoap_send(pkt,
							COAP_TYPE_NON,
							COAP_CODE_REQ_PUT,
							1,
							&cmote_vars.desc);

	// avoid overflowing the queue if sending the packet fails
	if (outcome==E_FAIL) {
	  openqueue_freePacketBuffer(pkt);
	}

	return;
}

/**
\brief Called when a CoAP message is received for this resource.

\param[in] msg          The received message. CoAP header and options already
   parsed.
\param[in] coap_header  The CoAP header contained in the message.
\param[in] coap_options The CoAP options contained in the message.

\return Whether the response is prepared successfully.
*/
owerror_t cmote_recieve(OpenQueueEntry_t* msg,
	      	  	  	  	coap_header_iht*  coap_header,
	      	  	  	  	coap_option_iht*  coap_options) {
	// we shouldn't be sending any packets to the mote (though we could!)
	return E_FAIL;
}

/**
\brief The stack indicates that the packet was sent.

\param[in] msg The CoAP message just sent.
\param[in] error The outcome of sending it.
*/
void cmote_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}

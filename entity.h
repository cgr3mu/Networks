#pragma once

/******************************************************************************/
/*                                                                            */
/* ENTITY DEFINITIONS                                                         */
/*                                                                            */
/******************************************************************************/

// These are the functions that must be implemented to achieve the reliable
// transport protocols. These functions will be called by simulator in response
// to the various networking events.
//
// DO NOT MODIFY THIS FILE. All grading will be done with an original copy of
// this file even if this file is included in the submission.

#include "simulator.h"


/****** FUNCTION SIGNATURES ***************************************************/

// These are the public functions provided by the entities that the simulator
// will call.


/**** A ENTITY ****/

// This routine will be called once by the simulator before any other entity
// "A" routines are called. This can be used to initialize any state if needed.
void A_init();

// This function is called when layer 5 on the "A" entity has data that should
// be sent to entity "B". That is, the passed in `message` is being output from
// entity "A" and should be passed over the network to entity "B".
void A_output(struct msg message);

// This function is called when a packet arrives from the network destined for
// entity "A". That is, the `packet` is passed from layer 3 to layer 4 and is
// being input to entity "A".
void A_input(struct pkt packet);

// This function will be called when entity "A"'s timer has fired.
void A_timerinterrupt();


/**** B ENTITY ****/

// This routine will be called once by the simulator before any other entity
// "B" routines are called. This can be used to initialize any state if needed.
void B_init();

// This function is called when a packet arrives from the network destined for
// entity "B". That is, the `packet` is passed from layer 3 to layer 4 and is
// being input to entity "B".
void B_input(struct pkt packet);

// This function will be called when entity "B"'s timer has fired.
void B_timerinterrupt();

#pragma once

/******************************************************************************/
/*                                                                            */
/* SIMULATOR PUBLIC INTERFACE                                                 */
/*                                                                            */
/******************************************************************************/

// These structure and function interfaces should be used by the "A" and "B"
// entities to implement the reliable transport protocols.
//
// DO NOT MODIFY THIS FILE. All grading will be done with an original copy of
// this file even if this file is included in the submission.



/****** DATA STRUCTURES *******************************************************/

// A `msg` is the data unit passed from layer 5 (by the simulator) to layer 4
// (the "A" entity). It contains the data to be delivered to layer 5 on the "B"
// entity using transport level protocol. This is also passed back to layer 5 on
// the "B" side.
//
// Each message can contain up to 20 bytes. The `length` field indicates how
// many of the bytes in `data` are actually valid. Note: every packet
// transmitted between the two entities will 20 bytes of data, but not all of it
// may be valid.
struct msg {
  int length;
  char data[20];
};

// A `pkt` is the data unit passed from layer 4 (the "A" and "B" entities) to layer
// 3 (the simulator). The `pkt` structure is pre-defined and may not be changed.
struct pkt {
  int seqnum;
  int acknum;
  int checksum;
  int length;
  char payload[20];
};


/****** FUNCTION SIGNATURES ***************************************************/

// These are the public functions provided by the simulator that student code
// may call.


/**** A ENTITY ****/

// Start a timer for entity "A".
//
// - `increment`: How many simulator time units in the future the timer should
//                triggered at.
void starttimer_A (float increment);

// Stop the entity "A" timer. It is OK to call this even if there is no active
// timer.
void stoptimer_A ();

// Allow entity "A" to pass a packet from layer 4 to layer 3. This will send it
// on the network. Since there are only two entities on this network, this will
// send the packet to entity "B".
void tolayer3_A (struct pkt packet);


/**** B ENTITY ****/

// Start a timer for entity "B".
//
// - `increment`: How many simulator time units in the future the timer should
//                triggered at.
void starttimer_B (float increment);

// Stop the entity "B" timer. It is OK to call this even if there is no active
// timer.
void stoptimer_B ();

// Allow entity "B" to pass a packet from layer 4 to layer 3. This will send it
// on the network. Since there are only two entities on this network, this will
// send the packet to entity "A".
void tolayer3_B (struct pkt packet);

// Allows entity "B" to pass a message from layer 4 to layer 5. This represents
// properly received data passed over the network.
void tolayer5_B (struct msg message);

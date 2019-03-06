/******************************************************************************/
/*                                                                            */
/* NETWORK SIMULATOR                                                          */
/*                                                                            */
/******************************************************************************/

// This simulates the layer 3 and below network environment:
// - Emulates the transmission and delivery (possibly with bit-level corruption
//   and packet loss) of packets across the layer 3/4 interface.
// - Handles the starting/stopping of a timer, and generates timer interrupts
//   (resulting in calling students timer handler).
// - Generates message to be sent (passed from later 5 to 4) based on the passed
//   in input file.
//
// DO NOT MODIFY THIS FILE. All grading will be done with an original copy of
// this file even if this file is included in the submission.
//
// If you're interested in how the simulator is designed, you're welcome to look
// at the code. However, you shouldn't need to.

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "simulator.h"
#include "entity.h"

// Generic event object that is added to a link list and used to represent
// the various events: timers, packets, and outgoing messages.
struct event {
  float evtime;           // event time
  int evtype;             // event type code
  int eventity;           // entity where event occurs
  struct pkt *pktptr;     // ptr to packet (if any) assoc w/ this event
  struct event *prev;
  struct event *next;
};

// The event list
struct event *evlist = NULL;

// Possible events
#define  TIMER_INTERRUPT 0
#define  FROM_LAYER5     1
#define  FROM_LAYER3     2

// The two entities
#define  A               0
#define  B               1

// Global state
int   TRACE = 1;           // How much debugging to display.
int   nsim = 0;            // Number of messages from 5 to 4 on "A" so far.
float time = 0.000;        // Current simulator time.
float lossprob;            // Probability that a packet is dropped.
float corruptprob;         // Probability that one bit is packet is flipped.
float lambda;              // Arrival rate of messages from layer 5.
int   ntolayer3;           // Number of packets sent into layer 3.
int   nlost;               // Number of packets lost in the network.
int   ncorrupt;            // Number of packets corrupted by media.
int   random_seed;         // Seed to use for the random number generator.
FILE* tx_file;             // File object to be transmitted.
FILE* rx_file;             // File that will be created with received data.


/********* FUNCTION SIGNATURES *********/

void init();
void generate_next_arrival();
void insertevent();
void starttimer(int AorB, float increment);
void stoptimer(int AorB);
void tolayer3(int AorB, struct pkt packet);


int main(int argc, char* argv[]) {
  struct event *eventptr;
  struct msg  msg2give;
  struct pkt  pkt2give;

  int i,j;
  char c;

  // Get the command line arguments.
  //
  // Command should be:
  // ./program <loss prob> <corrupt prob> <pkt interval> <seed> <debug> <input file>
  //
  // loss prob    : Probability of a packet being lost. 0.0 for no loss. 1.0 for complete loss.
  // corrupt prob : Probability of a packet being corrupted. 0.0 for no corruption. 1.0 for every packet being corrupted.
  // pkt interval : Average time between packets being sent from layer5. (> 0.0)
  // seed         : Value to use as the random seed.
  // debug        : Level of debugging output requested. 0, 1, 2, or 3.
  // input file   : Path to file with contents to be transmitted over simulated network.

  if (argc != 7) {
    printf("Error: Incorrect number of command line arguments\n");
    printf("usage: %s <loss prob> <corrupt prob> <pkt interval> <seed> <debug> <input file>\n", argv[0]);
    exit(-1);
  }

  // Parse all command line arguments.
  sscanf(argv[1], "%f", &lossprob);
  sscanf(argv[2], "%f", &corruptprob);
  sscanf(argv[3], "%f", &lambda);
  sscanf(argv[4], "%d", &random_seed);
  sscanf(argv[5], "%d", &TRACE);


  // Open the file that contains the message that should be transmitted from A
  // to B.
  tx_file = fopen(argv[6], "rb");
  if (tx_file == NULL) {
    printf("Could not open input file.\n");
    exit(-1);
  }

  // Open a file to save the received data in.
  rx_file = fopen("output.dat", "wb");
  if (rx_file == NULL) {
    printf("Could not open output file.\n");
    exit(-1);
  }


  // Simulator init.
  init();

  // Init for each of the two hosts communicating.
  A_init();
  B_init();

  // Main emulator loop.
  while (1) {
    // Get next event to simulate.
    eventptr = evlist;
    if (eventptr == NULL) {
      // There is nothing left to do.
      goto terminate;
    }

    // Remove this event from the list by setting the initial pointer to the
    // next item in the list.
    evlist = evlist->next;
    if (evlist != NULL) {
      // Update our linked list structure if we still have events to do.
      evlist->prev = NULL;
    }

    if (TRACE>=2) {
      printf("\nEVENT time: %f,",eventptr->evtime);
      printf("  type: %d",eventptr->evtype);
      if (eventptr->evtype==0) {
         printf(", timerinterrupt  ");
      } else if (eventptr->evtype==1) {
        printf(", fromlayer5 ");
      } else {
        printf(", fromlayer3 ");
      }
      printf(" entity: %d\n",eventptr->eventity);
    }

    // Update time to next event time.
    time = eventptr->evtime;

    // Handle the event correctly.
    if (eventptr->evtype == FROM_LAYER5 ) {

      // Copy up to the next 20 bytes of the input file into the message.
      size_t bytes_read = fread(msg2give.data, 1, 20, tx_file);
      msg2give.length = bytes_read;
      if (bytes_read == 20 && feof(tx_file) == 0) {
        // If we got the full amount and we are not at the end of the file
        // then we want to schedule another transmission.
        generate_next_arrival();
      }

      if (TRACE>2) {
        printf("          MAINLOOP: data given to student: ");
        for (i=0; i<msg2give.length; i++) {
          printf("%c", msg2give.data[i]);
        }
        printf("\n");
      }
      nsim++;
      if (eventptr->eventity == A) {
        A_output(msg2give);
      } else {
        printf("INTERNAL ERROR: we should not be passing packets to B output\n");
      }

    } else if (eventptr->evtype ==  FROM_LAYER3) {
      pkt2give.seqnum = eventptr->pktptr->seqnum;
      pkt2give.acknum = eventptr->pktptr->acknum;
      pkt2give.checksum = eventptr->pktptr->checksum;
      pkt2give.length = eventptr->pktptr->length;

      for (i=0; i<20; i++) {
        pkt2give.payload[i] = eventptr->pktptr->payload[i];
      }

      // Deliver packet by calling appropriate entity.
      if (eventptr->eventity == A) {
        A_input(pkt2give);
      } else {
        B_input(pkt2give);
      }
      // Free the memory for packet.
      free(eventptr->pktptr);

    } else if (eventptr->evtype ==  TIMER_INTERRUPT) {
      // Call correct entity's timer fired method.
      if (eventptr->eventity == A) {
        A_timerinterrupt();
      } else {
        B_timerinterrupt();
      }
    } else {
      printf("INTERNAL PANIC: unknown event type \n");
    }

    free(eventptr);
  }

terminate:
  fclose(tx_file);
  fclose(rx_file);
  printf(" Simulator terminated at time %f\n after sending %d msgs from layer5\n", time, nsim);
}

// Initialize the simulator.
void init() {
  int i;
  float sum, avg;
  float jimsrand();

  // init random number generator
  srand(random_seed);

  // test random number generator for students
  sum = 0.0;
  for (i=0; i<1000; i++) {
    sum=sum+jimsrand();    // jimsrand() should be uniform in [0,1]
  }
  avg = sum/1000.0;
  if (avg < 0.25 || avg > 0.75) {
    printf("It is likely that random number generation on your machine\n" );
    printf("is different from what this simulator expects.  Please take\n");
    printf("a look at the routine jimsrand() in the simulator code. Sorry. \n");
    exit(0);
  }

  ntolayer3 = 0;
  nlost     = 0;
  ncorrupt  = 0;
  time      = 0.0;             // initialize time to 0.0

  generate_next_arrival();     // initialize event list
}

// Return a float in range [0,1]. The routine below is used to
// isolate all random number generation in one location. We assume that the
// system-supplied rand() function return an int in therange [0,mmm].
float jimsrand() {
  double mmm = 2147483647;   // largest int  - MACHINE DEPENDENT!!!!!!!!
  float x;                   // individual students may need to change mmm
  x = rand()/mmm;            // x should be uniform in [0,1]
  return x;
}

/************ EVENT HANDLING ROUTINES ****************/
/*  The next set of routines handle the event list   */
/*****************************************************/

// Generate a new event from layer 5 to A (the sending side). This just adds
// an event at a particular time to the simulator, the content is filled in
// when this event is actually processed.
void generate_next_arrival() {
  double x;
  struct event *evptr;
  float ttime;
  int tempint;

  if (TRACE > 2) {
    printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
  }

  // x is uniform on [0,2*lambda], having mean of lambda.
  x = lambda * jimsrand() * 2;

  evptr = (struct event*) malloc(sizeof(struct event));

  // This gets triggered at some random, but bounded, time in the future.
  evptr->evtime   = time + x;
  evptr->evtype   = FROM_LAYER5;
  evptr->eventity = A;

  insertevent(evptr);
}

void insertevent(struct event *p) {
  struct event *q, *qold;

  if (TRACE > 2) {
    printf("            INSERTEVENT: time is %lf\n",time);
    printf("            INSERTEVENT: future time will be %lf\n",p->evtime);
  }

  // q points to header of list in which p struct inserted
  q = evlist;

  if (q == NULL) {
    // list is empty
    evlist=p;
    p->next=NULL;
    p->prev=NULL;

  } else {
    for (qold = q; q !=NULL && p->evtime > q->evtime; q=q->next) {
      qold = q;
    }
    if (q==NULL) {
      // end of list
      qold->next = p;
      p->prev = qold;
      p->next = NULL;
    } else if (q == evlist) {
      // front of list
      p->next=evlist;
      p->prev=NULL;
      p->next->prev=p;
      evlist = p;
    } else {
      // middle of list
      p->next=q;
      p->prev=q->prev;
      q->prev->next=p;
      q->prev=p;
    }
  }
}

void printevlist() {
  struct event *q;
  int i;
  printf("--------------\nEvent List Follows:\n");
  for(q = evlist; q!=NULL; q=q->next) {
    printf("Event time: %f, type: %d entity: %d\n", q->evtime, q->evtype, q->eventity);
  }
  printf("--------------\n");
}



/********************** STUDENT-CALLABLE ROUTINES ***********************/

void starttimer_A(float increment) {
  starttimer(A, increment);
}

void starttimer_B(float increment) {
  starttimer(B, increment);
}

void stoptimer_A() {
  stoptimer(A);
}

void stoptimer_B() {
  stoptimer(B);
}

void tolayer3_A(struct pkt packet) {
  tolayer3(A, packet);
}

void tolayer3_B(struct pkt packet) {
  tolayer3(B, packet);
}

void starttimer(int AorB, float increment) {
  struct event *q;
  struct event *evptr;

  if (TRACE>2) {
    printf("          START TIMER: starting timer at %f\n",time);
  }
  // be nice: check to see if timer is already started, if so, then warn
  for (q=evlist; q!=NULL ; q = q->next) {
    if ( (q->evtype == TIMER_INTERRUPT && q->eventity == AorB) ) {
      printf("Warning: attempt to start a timer that is already started\n");
      return;
    }
  }

  // create future event for when timer goes off
  evptr = (struct event*) malloc(sizeof(struct event));
  evptr->evtime   = time + increment;
  evptr->evtype   = TIMER_INTERRUPT;
  evptr->eventity = AorB;
  insertevent(evptr);
}

void stoptimer(int AorB) {
  struct event *q, *qold;

  if (TRACE>2) {
    printf("          STOP TIMER: stopping timer at %f\n",time);
  }

  for (q=evlist; q != NULL ; q = q->next) {
    if ( (q->evtype == TIMER_INTERRUPT  && q->eventity==AorB) ) {
      // remove this event
      if (q->next == NULL && q->prev == NULL) {
        // remove first and only event on list
        evlist=NULL;
      } else if (q->next == NULL) {
        // end of list - there is one in front
        q->prev->next = NULL;
      } else if (q==evlist) {
        // front of list - there must be event after
        q->next->prev = NULL;
        evlist = q->next;
      } else {
        // middle of list
        q->next->prev = q->prev;
        q->prev->next = q->next;
      }
      free(q);
      return;
    }
  }
  printf("Warning: unable to cancel your timer. It wasn't running.\n");
}

// Pass a packet from layer4 to layer3. This will send it on the network.
void tolayer3(int AorB, struct pkt packet) {
  struct pkt *mypktptr;
  struct event *evptr, *q;
  float lastime, x;
  int i;

  // Increment the count of how many packets have been sent to layer 3.
  ntolayer3++;

  // simulate losses:
  if (jimsrand() < lossprob) {
    nlost++;
    if (TRACE > 0) {
      printf("          TOLAYER3: packet being lost\n");
    }
    return;
  }

  // make a copy of the packet student just gave me since they may decide
  // to do something with the packet after we return back
  mypktptr = (struct pkt*) malloc(sizeof(struct pkt));
  mypktptr->seqnum   = packet.seqnum;
  mypktptr->acknum   = packet.acknum;
  mypktptr->checksum = packet.checksum;
  mypktptr->length   = packet.length;
  for (i=0; i<20; i++) {
    mypktptr->payload[i] = packet.payload[i];
  }
  if (TRACE>2)  {
    printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
    mypktptr->acknum,  mypktptr->checksum);
    for (i=0; i<mypktptr->length; i++) {
      printf("%c", mypktptr->payload[i]);
    }
    printf("\n");
  }

  // create future event for arrival of packet at the other side
  evptr = (struct event *)malloc(sizeof(struct event));
  evptr->evtype   = FROM_LAYER3;    // packet will pop out from layer3
  evptr->eventity = (AorB + 1) % 2; // event occurs at other entity
  evptr->pktptr   = mypktptr;       // save ptr to my copy of packet

  // Finally, compute the arrival time of packet at the other end.
  // medium can not reorder, so make sure packet arrives between 1 and 10
  // time units after the latest arrival time of packets
  // currently in the medium on their way to the destination
  lastime = time;
  for (q=evlist; q!=NULL; q = q->next) {
    if (q->evtype==FROM_LAYER3 && q->eventity==evptr->eventity) {
      lastime = q->evtime;
    }
  }

  evptr->evtime = lastime + 1 + (9*jimsrand());

  // simulate corruption
  if (jimsrand() < corruptprob)  {
    ncorrupt++;
    x = jimsrand();
    if (x < .75) {
      mypktptr->payload[0] = 'Z';   /* corrupt payload */
    } else if (x < .85) {
      mypktptr->seqnum = 999999;
    } else if (x < .925) {
      mypktptr->acknum = 999999;
    } else {
      mypktptr->length = 656565;
    }

    if (TRACE > 0) {
      printf("          TOLAYER3: packet being corrupted\n");
    }

    if (TRACE > 2) {
      printf("          TOLAYER3: scheduling arrival on other side\n");
    }
  }

  insertevent(evptr);
}

// Called to pass a packet up to layer5 on the receiver side
void tolayer5_B(struct msg message) {
  int i;
  if (TRACE >= 2) {
    printf("          TOLAYER5: data received: ");
    for (i=0; i<message.length; i++) {
      printf("%c", message.data[i]);
    }
    printf("\n");
  }

  fwrite(message.data, 1, message.length, rx_file);
}

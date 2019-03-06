/******************************************************************************/
/*                                                                            */
/* ENTITY IMPLEMENTATIONS                                                     */
/*                                                                            */
/******************************************************************************/

// Student names: David Xue, Connor Roos
// Student computing IDs: djx7et, cgr3mu
//
//
// This file contains the actual code for the functions that will implement the
// reliable transport protocols enabling entity "A" to reliably send information
// to entity "B".
//
// This is where you should write your code, and you should submit a modified
// version of this file.
//
// Notes:
// - One way network delay averages five time units (longer if there are other
//   messages in the channel for GBN), but can be larger.
// - Packets can be corrupted (either the header or the data portion) or lost,
//   according to user-defined probabilities entered as command line arguments.
// - Packets will be delivered in the order in which they were sent (although
//   some can be lost).
// - You may have global state in this file, BUT THAT GLOBAL STATE MUST NOT BE
//   SHARED BETWEEN THE TWO ENTITIES' FUNCTIONS. "A" and "B" are simulating two
//   entities connected by a network, and as such they cannot access each
//   other's variables and global state. Entity "A" can access its own state,
//   and entity "B" can access its own state, but anything shared between the
//   two must be passed in a `pkt` across the simulated network. Violating this
//   requirement will result in a very low score for this project (or a 0).
//
// To run this project you should be able to compile it with something like:
//
//     $ gcc entity.c simulator.c -o myproject
//
// and then run it like:
//
//     $ ./myproject 0.0 0.0 10 500 3 test1.txt
//
// Of course, that will cause the channel to be perfect, so you should test
// with a less ideal channel, and you should vary the random seed. However, for
// testing it can be helpful to keep the seed constant.
//
// The simulator will write the received data on entity "B" to a file called
// `output.dat`.

#include <stdio.h>
#include "simulator.h"
#include <stdlib.h>
#include <string.h>

int WINDOW_SIZE = 8;         // size of the window
int LIMIT_SEQNUM = 1000;        // maximum sequence number for 16-bit GBN
double RXMT_TIMEOUT = 20;     // retransmission timeout
extern float time;         // simulation time

// Entity A
int firstPack;                     // first sequence number in the window
int lastPack;                 // last sequence number in the window
int nextMsg;                    // message index
struct msg msgBuffer[1000];                 // message buffer
struct pkt txPktBuffer[1000];               // packet buffer
int msgCount;                   // message count

// Entity B
int expectSeqNum;               // expected sequence number
int lastAckNum;                 // last acknowledgement number

/**** A ENTITY ****/
void printWindow(int base)
{
    int i, end = (base + WINDOW_SIZE) % LIMIT_SEQNUM;

    printf("    WINDOW: [");
    for (i = base; i != end; i = (i + 1) % LIMIT_SEQNUM)
        printf(" %d", i);
    printf(" ]\n");
}

// Check if number is within window
int isWithinWindow(int base, int i)
{
    // Number located right of base
    int right = i >= base && i < base + WINDOW_SIZE;

    // Number located left of base
    int left = i < base && i + LIMIT_SEQNUM < base + WINDOW_SIZE;

    return right || left;
}

// Calculate checksum of packet
int calcChecksum(struct pkt packet)
{
    int i, checksum = 0;

    checksum += packet.seqnum;
    checksum += packet.acknum;

    for (i = 0; i < 20; i++)
        checksum += packet.payload[i];

    return checksum;
}

// Check packet for errors
int isCorrupt(struct pkt packet)
{
    return calcChecksum(packet);
}

// Called from layer 5, pass the data to be sent to other side
void A_output(struct msg message)
{
    printf("  A: Receiving MSG from above...\n");
    printf("    DATA: %.*s\n", 20, message.data);

    // Add message to buffer and update message count
    msgBuffer[msgCount] = message;
    msgCount++;
    
    // Next sequence number is within window
    if (isWithinWindow(firstPack, lastPack))
    {
        struct pkt new_packet;

        // Create DATA packet
        new_packet.seqnum = lastPack;
        new_packet.acknum = 0;
        // Copies message into payload
        memcpy(new_packet.payload, msgBuffer[nextMsg].data, message.length);
        
        new_packet.checksum = calcChecksum(new_packet);
        new_packet.length = message.length;
        // Add to packet buffer
       txPktBuffer[lastPack] = new_packet;
 
        
        // Send packet to network
        printf("  A: Sending new DATA to B...\n");
        printf("    SEQ, ACK: %d, %d\n", new_packet.seqnum, new_packet.acknum);
        printf("    CHECKSUM: %d\n", new_packet.checksum);
        printf("    PAYLOAD: %.*s\n", 20, new_packet.payload);
        //printWindow(firstPack);
        tolayer3_A(new_packet);

        // Set timer if packet is first in window
        if (lastPack == firstPack)
            starttimer_A(RXMT_TIMEOUT);

        // Update next sequence number
        lastPack = (lastPack + 1) % LIMIT_SEQNUM;

        // Update next message index
        nextMsg++;
    }
}

// Called from layer 3, when a packet arrives for layer 4
void A_input(struct pkt packet)
{
    printf("  A: Receiving ACK from B...\n");
    printf("    SEQ, ACK: %d, %d\n", packet.seqnum, packet.acknum);
    printf("    CHECKSUM: %d\n", packet.checksum);
    printf("    PAYLOAD: %.*s\n", 20, packet.payload);
    //printWindow(firstPack);

    // No errors and ACK number within window
    if (calcChecksum(packet) == packet.checksum && isWithinWindow(firstPack, packet.acknum))
    {
        int i, shift;

        printf("  A: Accepting ACK from B...\n");
	printf("    CHECKSUM Calculation: %d\n\n\n\n\n\n\n\n", calcChecksum(packet));

        // Stop timer
        stoptimer_A();

        // Find number of times window shifted
        if (packet.acknum < firstPack)
            shift = packet.acknum - firstPack + LIMIT_SEQNUM;
        else
            shift = packet.acknum - firstPack;

        // Update base
        firstPack = (packet.acknum + 1) % LIMIT_SEQNUM;

        // Iterate through newly available slots
        for (i = 0; i < shift + 1; i++)
        {
            // Outstanding messages available
            if (nextMsg < msgCount)
            {
                // Create DATA packet
                struct pkt new_packet;
                new_packet.seqnum = lastPack;
                new_packet.acknum = 0;
                memcpy(new_packet.payload, msgBuffer[nextMsg].data, msgBuffer[nextMsg].length);
                new_packet.checksum = calcChecksum(new_packet);
                new_packet.length = msgBuffer[nextMsg].length;
                // Add to packet buffer
                txPktBuffer[lastPack] = new_packet;

                // Send packet to network
                printf("  A: Sending new DATA to B...\n");
                printf("    SEQ, ACK: %d, %d\n", new_packet.seqnum, new_packet.acknum);
                printf("    CHECKSUM: %d\n", new_packet.checksum);
                printf("    PAYLOAD: %.*s\n", 20, new_packet.payload);
                //printWindow(firstPack);
                tolayer3_A(new_packet);

                // Update next sequence number
                lastPack = (lastPack + 1) % LIMIT_SEQNUM;

                // Update next message index
                nextMsg++;
            }
        }

        // Set timer if there are still packets to send
        if (firstPack != lastPack)
            starttimer_A(RXMT_TIMEOUT);
    }
    else
    {
        // Discard packet
        printf("  A: Rejecting ACK from B... (pending ACK %d)\n", firstPack);
        //printWindow(firstPack);
    }
}

// Called when A's timer goes off
void A_timerinterrupt(void)
{
    struct pkt new_packet;
    int i = firstPack;

    // Iterate through window
    while (i != lastPack)
    {
        // Resend packet to network
        new_packet = txPktBuffer[i];
        printf("  A: Resending DATA to B...\n");
        printf("    SEQ, ACK: %d, %d\n", new_packet.seqnum, new_packet.acknum);
        printf("    CHECKSUM: %d\n", new_packet.checksum);
        printf("    PAYLOAD: %.*s\n", 20, new_packet.payload);
        //printWindow(firstPack);
        tolayer3_A(new_packet);

        // Iterate
        i = (i + 1) % LIMIT_SEQNUM;
    }

    // Set timer
    starttimer_A(RXMT_TIMEOUT);
}
// Called when B's timer goes off
void B_timerinterrupt(void)
{
    struct pkt new_packet;
    int i = firstPack;

    // Iterate through window
    while (i != lastPack)
    {
        // Resend packet to network
        new_packet = txPktBuffer[i];
        printf("  B: Resending DATA to A...\n");
        printf("    SEQ, ACK: %d, %d\n", new_packet.seqnum, new_packet.acknum);
        printf("    CHECKSUM: %d\n", new_packet.checksum);
        printf("    PAYLOAD: %.*s\n", 20, new_packet.payload);
        //printWindow(firstPack);
        tolayer3_B(new_packet);

        // Iterate
        i = (i + 1) % LIMIT_SEQNUM;
    }

    // Set timer
    starttimer_B(RXMT_TIMEOUT);
}
// Called once before any other entity A routines are called
void A_init(void)
{
    // Allocate buffers
    nextMsg = 0;
    msgCount = 0;

    // State variables
    firstPack = 0;
    lastPack = 0;

}

// Called from layer 3, when a packet arrives for layer 4 at B
void B_input(struct pkt packet)
{
    printf("  B: Receiving DATA from A...\n");
    printf("    SEQ, ACK: %d, %d\n", packet.seqnum, packet.acknum);
    printf("    CHECKSUM: %d\n", packet.checksum);
    printf("    PAYLOAD: %.*s\n", 20, packet.payload);

    struct pkt new_packet;

    // Packet not corrupted and SEQ number is new
    if (calcChecksum(packet) == packet.checksum && packet.seqnum == expectSeqNum)
    {
        // Send message to above
	struct msg temp;
	temp.length = packet.length;
	printf("%i",temp.length);
	strcpy(temp.data,packet.payload);
        tolayer5_B(temp);


        // Create ACK packet
        new_packet.seqnum = 0;
        new_packet.acknum = packet.seqnum;
        memcpy(new_packet.payload, packet.payload, packet.length);
        new_packet.length = packet.length;
	new_packet.checksum = calcChecksum(new_packet);
        // Send packet to network
        printf("  B: Sending new ACK to A...\n");
        printf("    SEQ, ACK: %d, %d\n", new_packet.seqnum, new_packet.acknum);
        printf("    CHECKSUM: %d\n", new_packet.checksum);
        printf("    PAYLOAD: %.*s\n", 20, new_packet.payload);
        tolayer3_B(new_packet);

        // Record ACK number
        lastAckNum = new_packet.acknum;

        // Update expected sequence number
        expectSeqNum = (expectSeqNum + 1) % LIMIT_SEQNUM;
    }

    // Packet is corrupted or has invalid SEQ number
    else
    {
        // Create ACK packet for previously acknowledged DATA packet
        new_packet.seqnum = 0;
        new_packet.acknum = lastAckNum;
        memcpy(new_packet.payload, packet.payload, packet.length);  
	new_packet.length = packet.length;
	new_packet.checksum = calcChecksum(new_packet);
        // Send packet to network
        printf("  B: Resending previous ACK to A...\n");
        printf("    SEQ, ACK: %d, %d\n", new_packet.seqnum, new_packet.acknum);
        printf("    CHECKSUM: %d\n", new_packet.checksum);
        printf("    PAYLOAD: %.*s\n", 20, new_packet.payload);
        tolayer3_B(new_packet);
    }
}

// Called once before any other entity B routines are called
void B_init(void)
{
    // State variables
    expectSeqNum = 0; //Expecting the first packet

    lastAckNum = LIMIT_SEQNUM - 1; //Last possible packet to acknowledge
}

/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "iocinf.h"
#include "cac.h"
#include "virtualCircuit.h"

//
// the recv watchdog timer is active when this object is created
//
tcpRecvWatchdog::tcpRecvWatchdog 
    ( cac & cacIn, tcpiiu & iiuIn, double periodIn, epicsTimerQueue & queueIn ) :
        period ( periodIn ), timer ( queueIn.createTimer () ),
        iiu ( iiuIn ), cacRef ( cacIn ), responsePending ( false ),
        beaconAnomaly ( true )
{
}

tcpRecvWatchdog::~tcpRecvWatchdog ()
{
    this->timer.destroy ();
}

epicsTimerNotify::expireStatus
tcpRecvWatchdog::expire ( const epicsTime & /* currentTime */ ) // X aCC 361
{
    if ( this->responsePending ) {
        if ( ! this->cacRef.preemptiveCallbakIsEnabled() ) {
            if ( this->iiu.bytesArePendingInOS() ) {
                this->cacRef.printf ( 
        "The CA client library is disconnecting because a CA server isnt responding\n" );
                this->cacRef.printf ( 
        "when there are incoming messages pending probably because the application\n" );
                this->cacRef.printf ( 
        "isnt calling ca_pend_event().\n" );
            }
        }
        this->cancel ();
#       ifdef DEBUG
            char hostName[128];
            this->iiu.hostName ( hostName, sizeof (hostName) );
            debugPrintf ( ( "CA server \"%s\" unresponsive after %g inactive sec"
                            "- disconnecting.\n", 
                hostName, this->period ) );
#       endif
        this->cacRef.initiateAbortShutdown ( this->iiu );
        return noRestart;
    }
    else {
        this->responsePending = this->iiu.setEchoRequestPending ();
        debugPrintf ( ("TCP connection timed out - sending echo request\n") );
        return expireStatus ( restart, CA_ECHO_TIMEOUT );
    }
}

void tcpRecvWatchdog::beaconArrivalNotify ( const epicsTime & currentTime )
{
    if ( ! this->beaconAnomaly && ! this->responsePending ) {
        this->timer.start ( *this, currentTime + this->period );
        debugPrintf ( ("Saw a normal beacon - reseting TCP recv watchdog\n") );
    }
}

//
// be careful about using beacons to reset the connection
// time out watchdog until we have received a ping response 
// from the IOC (this makes the software detect reconnects
// faster when the server is rebooted twice in rapid 
// succession before a 1st or 2nd beacon has been received)
//
void tcpRecvWatchdog::beaconAnomalyNotify ()
{
    this->beaconAnomaly = true;
    debugPrintf ( ("Saw an abnormal beacon\n") );
}

void tcpRecvWatchdog::messageArrivalNotify ( const epicsTime & currentTime )
{
    this->beaconAnomaly = false;
    this->responsePending = false;
    this->timer.start ( *this, currentTime + this->period );
    debugPrintf ( ("received a message - reseting TCP recv watchdog\n") );
}

//
// The thread for outgoing requests in the client runs 
// at a higher priority than the thread in the client
// that receives responses. Therefore, there could 
// be considerable large array write send backlog that 
// is delaying departure of an echo request and also 
// interrupting delivery of an echo response. 
// We must be careful not to timeout the echo response as 
// long as we see indication of regular departures of  
// message buffers from the client in a situation where 
// we know that the TCP send queueing has been exceeded. 
// The send watchdog will be responsible for detecting 
// dead connections in this case.
//
void tcpRecvWatchdog::sendBacklogProgressNotify ()
{
    // We dont set "beaconAnomaly" to be false here because, after we see a
    // beacon anomaly (which could be transiently detecting a reboot) we will 
    // not trust the beacon as an indicator of a healthy server until we 
    // receive at least one message from the server.
    this->responsePending = false;
    this->timer.start ( *this, this->period );
    debugPrintf ( ("saw heavy send backlog - reseting TCP recv watchdog\n") );
}

void tcpRecvWatchdog::connectNotify ()
{
    this->timer.start ( *this, this->period );
    debugPrintf ( ("connected to the server - reseting TCP recv watchdog\n") );
}

void tcpRecvWatchdog::cancel ()
{
    this->timer.cancel ();
    debugPrintf ( ("canceling TCP recv watchdog\n") );
}

double tcpRecvWatchdog::delay () const
{
    return this->timer.getExpireDelay ();
}

void tcpRecvWatchdog::show ( unsigned level ) const
{
    ::printf ( "Receive virtual circuit watchdog at %p, period %f\n",
        static_cast <const void *> ( this ), this->period );
    if ( level > 0u ) {
        ::printf ( "\tresponse pending boolean %u, beacon anomaly boolean %u\n",
            this->responsePending, this->beaconAnomaly );
    }
}

/* devWfTestAsyn.c */
/* share/src/dev $Id$ */

/* devWfTestAsyn.c - Device Support Routines for testing asynchronous processing*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02  01-08-92        jba     Added cast in call to wdStart to avoid compile warning msg
 * .03  02-05-92	jba	Changed function arguments from paddr to precord 
 *      ...
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<wdLib.h>

#include	<alarm.h>
#include	<callback.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<dbCommon.h>
#include	<waveformRecord.h>

/* Create the dset for devWfTestAsyn */
long init_record();
long read_wf();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_wf;
	DEVSUPFUN	special_linconv;
}devWfTestAsyn={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_wf,
	NULL};

/* control block for callback*/
struct callback {
        CALLBACK        callback;
        struct dbCommon *precord;
        WDOG_ID wd_id;
};

static void myCallback(pcallback)
    struct callback *pcallback;
{
    struct dbCommon *precord=pcallback->precord;
    struct rset     *prset=(struct rset *)(precord->rset);

    dbScanLock(precord);
    (*prset->process)(precord);
    dbScanUnlock(precord);
}

static long init_record(pwf)
    struct waveformRecord	*pwf;
{
    char message[100];
    struct callback *pcallback;

    /* wf.inp must be a CONSTANT*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	pcallback = (struct callback *)(calloc(1,sizeof(struct callback)));
	pwf->dpvt = (void *)pcallback;
	callbackSetCallback(myCallback,pcallback);
        pcallback->precord = (struct dbCommon *)pwf;
	pcallback->wd_id = wdCreate();
	pwf->nord = 0;
	break;
    default :
	strcpy(message,pwf->name);
	strcat(message,": devWfTestAsyn (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_wf(pwf)
    struct waveformRecord	*pwf;
{
    char message[100];
    long status,options,nRequest;
    struct callback *pcallback=(struct callback *)(pwf->dpvt);
    int		wait_time;

    /* wf.inp must be a CONSTANT*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	if(pwf->pact) {
		printf("%s Completed\n",pwf->name);
		return(0); /* don`t convert*/
	} else {
		wait_time = (int)(pwf->disv * vxTicksPerSecond);
		if(wait_time<=0) return(0);
		callbackSetPriority(pwf->prio,pcallback);
		printf("%s Starting asynchronous processing\n",pwf->name);
		wdStart(pcallback->wd_id,wait_time,callbackRequest,(int)pcallback);
		return(1);
	}
    default :
        if(recGblSetSevr(pwf,SOFT_ALARM,VALID_ALARM)){
		if(pwf->stat!=SOFT_ALARM) {
			strcpy(message,pwf->name);
			strcat(message,": devWfTestAsyn (read_wf) Illegal INP field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}

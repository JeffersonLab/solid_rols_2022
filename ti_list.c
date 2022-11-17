/*************************************************************************
 *
 *  ti_list.c - Library of routines for readout and buffering of
 *                     events using a JLAB Trigger Interface V3 (TI) with
 *                     a Linux VME controller in CODA 3.0.
 *
 */

/* Event Buffer definitions */
#define MAX_EVENT_POOL     10
//#define MAX_EVENT_LENGTH   1024*64      /* Size in Bytes */
#define MAX_EVENT_LENGTH   16000*64      /* Size in Bytes */

/* TI_MASTER / TI_SLAVE defined in Makefile */
#ifdef TI_MASTER
/* EXTernal trigger source (e.g. front panel ECL input), POLL for available data */
#define TI_READOUT TI_READOUT_EXT_POLL
#else
#ifdef TI_SLAVE5
#define TI_SLAVE
#define TI_FLAG TI_INIT_SLAVE_FIBER_5
#endif
/* TS trigger source (e.g. fiber), POLL for available data */
#define TI_READOUT TI_READOUT_TS_POLL
#endif
#define TI_ADDR  0 /* Auto initialize (search for TI by slot */

/* Measured longest fiber length in system */
#define FIBER_LATENCY_OFFSET 0x4A

#include <unistd.h>
#include "dmaBankTools.h"   /* Macros for handling CODA banks */
#include "tiprimary_list.c" /* Source required for CODA readout lists using the TI */
#include "sdLib.h"

#ifdef USE_FA250
#include "fa250_rol_include.c"
#endif

#ifdef USE_SSP_MPD
#include "ssp_mpd_rol_include.c"
#endif

#ifdef USE_SSP_MAROC
#include "ssp_maroc_rol_include.c"
#endif

/* Define initial blocklevel and buffering level */
#define BLOCKLEVEL 1
#define BUFFERLEVEL 5


typedef struct
{
  int enable;       // enable (1) / disable (0)
  int interval;     // 1 sync event every <interval> events
  int pedestal;     // disable (0) / Disable after <pedestal> sync events
  int current;      // sync event counter for pedestal feature
  int pulser_arg1;  // fixed pulser <period> argument.  Units of 120ns.
} TI_SYNCEVENT_CONFIG;

TI_SYNCEVENT_CONFIG tiSyncEventConfig = {1, 1000, 100, 0, 83};
//TI_SYNCEVENT_CONFIG tiSyncEventConfig = {0, 1000, 0, 0, 83}; // disable pulser trigger



/*
  Global to configure the trigger source
  0 : tsinputs
  1 : TI random pulser
  2 : TI fixed pulser

  Set with rocSetTriggerSource(int source);
*/
int rocTriggerSource = 0;
void rocSetTriggerSource(int source); // routine prototype

/****************************************
 *  DOWNLOAD
 ****************************************/
void
rocDownload()
{
  int stat;

  /* Define BLock Level */
  blockLevel = BLOCKLEVEL;


  /*****************
   *   TI SETUP
   *****************/
#ifdef TI_MASTER
  /*
   * Set Trigger source
   *    For the TI-Master, valid sources:
   *      TI_TRIGGER_FPTRG     2  Front Panel "TRG" Input
   *      TI_TRIGGER_TSINPUTS  3  Front Panel "TS" Inputs
   *      TI_TRIGGER_TSREV2    4  Ribbon cable from Legacy TS module
   *      TI_TRIGGER_PULSER    5  TI Internal Pulser (Fixed rate and/or random)
   */
  if(rocTriggerSource == 0)
    {
      tiSetTriggerSource(TI_TRIGGER_TSINPUTS); /* TS Inputs enabled */
    }
  else
    {
      tiSetTriggerSource(TI_TRIGGER_PULSER); /* Internal Pulser */
    }

  /* Enable set specific TS input bits (1-6) */
  //tiEnableTSInput( TI_TSINPUT_1 | TI_TSINPUT_2 );
  tiEnableTSInput( TI_TSINPUT_1 | TI_TSINPUT_2 | TI_TSINPUT_3);
  //tiEnableTSInput( TI_TSINPUT_ALL );

  /* Load the trigger table that associates
   *  pins 21/22 | 23/24 | 25/26 : trigger1
   *  pins 29/30 | 31/32 | 33/34 : trigger2
   */
  tiLoadTriggerTable(0);

  /* Specific settings for MPD in ssp_mpd_rol_include.c */
  tiSetTriggerHoldoff(1,10,0);
  tiSetTriggerHoldoff(2,10,0);

  /* Set initial number of events per block */
  tiSetBlockLevel(blockLevel);

  /* Set Trigger Buffer Level */
  tiSetBlockBufferLevel(BUFFERLEVEL);

  /*Set prescale for each TS#*/
  tiSetInputPrescale(1,0);
  tiSetInputPrescale(2,0);
  tiSetInputPrescale(3,8);
  //tiSetInputPrescale(4,0);


#endif
  tiSetEvTypeScalers(1);

  /* Init the SD library so we can get status info */
  stat = sdInit(SD_INIT_IGNORE_VERSION);
  if(stat==0)
    {
      sdSetActiveVmeSlots(0);
      sdStatus(0);
    }

  /* Sync Event / Pedestal Config */
  printf("%s:  tiSyncEventConfig:   SyncEvent %s\n", __func__,
	 (tiSyncEventConfig.enable) ? "Enabled" : "Disabled");

  if(tiSyncEventConfig.enable)
    {
      tiSetSyncEventInterval(tiSyncEventConfig.interval);

      printf("\tinterval = %d\n", tiSyncEventConfig.interval);

      if(tiSyncEventConfig.pedestal > 0)
	{
	  printf("\tpedestal for first %d sync events\n", tiSyncEventConfig.pedestal);
	}
    }

  tiStatus(0);

#ifdef USE_FA250
  fa250_Download(NULL);
#endif

#ifdef USE_SSP_MPD
  sspMpd_Download(NULL);
#endif

#ifdef USE_SSP_MAROC
  sspMaroc_Download(NULL);
#endif

  printf("rocDownload: User Download Executed\n");

}

/****************************************
 *  PRESTART
 ****************************************/
void
rocPrestart()
{

  tiStatus(0);

#ifdef USE_FA250
  fa250_Prestart();
#endif

#ifdef USE_SSP_MPD
  sspMpd_Prestart();
#endif

#ifdef USE_SSP_MAROC
  sspMaroc_Prestart();
#endif

  printf("rocPrestart: User Prestart Executed\n");

}

/****************************************
 *  GO
 ****************************************/
void
rocGo()
{

  /* Print out the Run Number and Run Type (config id) */
  printf("rocGo: Activating Run Number %d, Config id = %d\n",
	 rol->runNumber,rol->runType);

  int bufferLevel = 0;
  /* Get the current buffering settings (blockLevel, bufferLevel) */
  blockLevel = tiGetCurrentBlockLevel();
  bufferLevel = tiGetBroadcastBlockBufferLevel();
  printf("%s: Block Level = %d,  Buffer Level (broadcasted) = %d (%d)\n",
	 __func__,
	 blockLevel,
	 tiGetBlockBufferLevel(),
	 bufferLevel);

#ifdef TI_SLAVE
  /* In case of slave, set TI busy to be enabled for full buffer level */

  /* Check first for valid blockLevel and bufferLevel */
  if((bufferLevel > 10) || (blockLevel > 1))
    {
      daLogMsg("ERROR","Invalid blockLevel / bufferLevel received: %d / %d",
	       blockLevel, bufferLevel);
      tiUseBroadcastBufferLevel(0);
      tiSetBlockBufferLevel(1);

      /* Cannot help the TI blockLevel with the current library.
	 modules can be spared, though
      */
      blockLevel = 1;
    }
  else
    {
      tiUseBroadcastBufferLevel(1);
    }
#endif

  /* Enable/Set Block Level on modules, if needed, here */


#ifdef TI_MASTER
  if(rocTriggerSource != 0)
    {
      printf("************************************************************\n");
      daLogMsg("INFO","TI Configured for Internal Pulser Triggers");
      printf("************************************************************\n");

      if(rocTriggerSource == 1)
	{
	  /* Enable Random at rate 500kHz/(2^7) = ~3.9kHz */
	  tiSetRandomTrigger(1,0xf);
	}

      if(rocTriggerSource == 2)
	{
	  /*    Enable fixed rate with period (ns)
		120 +30*700*(1024^0) = 21.1 us (~47.4 kHz)
		- arg2 = 0xffff - Continuous
		- arg2 < 0xffff = arg2 times
	  */
	  tiSoftTrig(1,0xffff,100,0);
	}
    }
  else
    {
      /* Check for pedestal configuration */
      if(tiSyncEventConfig.pedestal > 1)
	{
	  unsigned int trigenable;
	  trigenable  = TI_TRIGSRC_VME | TI_TRIGSRC_LOOPBACK;
	  trigenable |= TI_TRIGSRC_TSINPUTS;
	  trigenable |= TI_TRIGSRC_PULSER;
	  tiSetTriggerSourceMask(trigenable);

	  tiSyncEventConfig.current = 0;

	  tiSoftTrig(1, 0xffff, tiSyncEventConfig.pulser_arg1, 0);
	}
    }

#endif

#ifdef USE_FA250
  fa250_Go();
#endif

#ifdef USE_SSP_MPD
  sspMpd_Go();
#endif

#ifdef USE_SSP_MAROC
  sspMaroc_Go();
#endif
}

/****************************************
 *  END
 ****************************************/
void
rocEnd()
{

#ifdef TI_MASTER
  if(rocTriggerSource == 1)
    {
      /* Disable random trigger */
      tiDisableRandomTrigger();
    }

  if(rocTriggerSource == 2)
    {
      /* Disable Fixed Rate trigger */
      tiSoftTrig(1,0,100,0);
    }
#endif

  tiStatus(0);

#ifdef USE_FA250
  fa250_End();
#endif

  printf("rocEnd: Ended after %d blocks\n",tiGetIntCount());

}

/****************************************
 *  TRIGGER
 ****************************************/
void
rocTrigger(int arg)
{
  int dCnt;

  /* Set TI output 1 high for diagnostics */
  tiSetOutputPort(1,0,0,0);

  /* Readout the trigger block from the TI
     Trigger Block MUST be readout first */
  dCnt = tiReadTriggerBlock(dma_dabufp);

  if(dCnt<=0)
    {
      printf("No TI Trigger data or error.  dCnt = %d\n",dCnt);
    }
  else
    { /* TI Data is already in a bank structure.  Bump the pointer */
      dma_dabufp += dCnt;
    }

#ifdef USE_FA250
  fa250_Trigger(arg);
#endif

#ifdef USE_SSP_MPD
  sspMpd_Trigger(arg);
#endif

#ifdef USE_SSP_MAROC
  sspMaroc_Trigger(arg);
#endif

  if(tiGetSyncEventFlag() == 1)
    {
      /* Update counter */
      tiSyncEventConfig.current++;

      if(tiSyncEventConfig.current >= tiSyncEventConfig.pedestal)
	{
	  static int32_t doOnce = 0;
	  /* Disable pulser */
	  if(doOnce++ == 1) tiSoftTrig(1, 0, 0, 0);
	}
      else
	{
	  /* Make sure it continues */
	  tiSoftTrig(1, 0xffff, tiSyncEventConfig.pulser_arg1, 0);
	}

      /* Check for data available */
      int davail = tiBReady();
      if(davail > 0)
	{
	  printf("%s: ERROR: TI Data available (%d) after readout in SYNC event \n",
		 __func__, davail);

	  while(tiBReady())
	    {
	      vmeDmaFlush(tiGetAdr32());
	    }
	}
    }

  /* Set TI output 0 low */
  tiSetOutputPort(0,0,0,0);

}

void
rocLoad()
{

}

void
rocCleanup()
{
  printf("%s: Reset all Modules\n",__FUNCTION__);
#ifdef TI_MASTER
  tiResetSlaveConfig();
#endif

#ifdef USE_FA250
  fa250_Cleanup();
#endif

#ifdef USE_SSP_MPD
  sspMpd_Cleanup();
#endif

#ifdef USE_SSP_MAROC
  sspMaroc_Cleanup();
#endif

}
/*
  Routine to configure pedestal subtraction mode
  0 : subtraction mode DISABLED
  1 : subtraction mode ENABLED
*/

void
rocSetTriggerSource(int source)
{
#ifdef TI_MASTER
  if(TIPRIMARYflag == 1)
    {
      printf("%s: ERROR: Trigger Source already enabled.  Ignoring change to %d.\n",
	     __func__, source);
    }
  else
    {
      rocTriggerSource = source;

      if(rocTriggerSource == 0)
	{
	  tiSetTriggerSource(TI_TRIGGER_TSINPUTS); /* TS Inputs enabled */
	}
      else
	{
	  tiSetTriggerSource(TI_TRIGGER_PULSER); /* Internal Pulser */
	}

      daLogMsg("INFO","Setting trigger source (%d)", rocTriggerSource);
    }
#else
  printf("%s: ERROR: TI is not Master  Ignoring change to %d.\n",
	 __func__, source);
#endif
}



/*
  Local Variables:
  compile-command: "make -k"
  End:
 */

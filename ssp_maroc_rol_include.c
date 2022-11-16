#pragma once
/*************************************************************************
 *
 *  ssp_maroc_rol_include.c -
 *
 *   Library of routines readout and buffering of events from an SSP
 *
 */
#include "sspLib.h"
#include "sspConfig.h"

#ifndef SSP_MAROC_SLOT
#define SSP_MAROC_SLOT 13
#endif
#ifndef SSP_MPD_SLOT
#define SSP_MPD_SLOT 20
#endif
#define SSP_MAROC_BANK 18

extern int nSSP;
extern unsigned int sspA32Base;

static int ssp_not_ready_errors[21];

const char *rol_usrConfig = "/home/solid/sbsvme22/bryan/ssp/test/sbsvme22.cnf";

#define DEBUG

#define SSP_READ_CONF_FILE  {			\
    sspInitGlobals();				\
    sspConfig("");				\
    if(rol_usrConfig)				\
      sspConfig((char *)rol_usrConfig);		\
  }

void
sspMaroc_Download()
{
  printf("%s: Download Executed\n",
	 __func__);
}

/****************************************
 *  PRESTART
 ****************************************/
void
sspMaroc_Prestart()
{
  unsigned int iFlag;
  int id, SSP_SLOT;

  /*****************
   *   SSP SETUP
   *****************/

  if(nSSP <= 0)
    {
      extern unsigned int sspAddrList[MAX_VME_SLOTS+1];

      sspAddrList[0] = SSP_MAROC_SLOT << 19;
      sspAddrList[1] = SSP_MPD_SLOT << 19;

      iFlag = 0xFFFF0000 | SSP_INIT_MODE_VXS; // MAROC SSP Uses clock from TI
      iFlag |= SSP_INIT_USE_ADDRLIST;

      sspInit(0, 0, 0, iFlag); /* Scan for, and initialize all SSPs in crate */
      printf("%s: found %d SSPs (using iFlag=0x%08x)\n",
	 __func__, nSSP,iFlag);

      /* Add SSP as Busy sources to TI.  Will not reset previous settings (reset=0) */
      int32_t reset=0;
      sdSetBusyVmeSlots(sspSlotMask(), reset);
    }
  else
    {
      printf("%s: SSPs previously initialized (%d)\n", __func__, nSSP);
      /* FIXME: Do MAROC specific SSP init here... (check clock), FiberScan */
    }

  if(nSSP>0)
    {
      SSP_READ_CONF_FILE;
      printf("=======================> sspSlotMask=0x%08x\n",sspSlotMask());

    }

  printf("%s: Prestart Executed\n",
	 __func__);

}

/****************************************
 *  GO
 ****************************************/
void
sspMaroc_Go()
{
  int id, slot;

  /* Set the blocklevel in the SSP */
  uint32_t BLOCKLEVEL = tiGetCurrentBlockLevel();

  sspSetBlockLevel(SSP_MAROC_SLOT, BLOCKLEVEL);
  sspStatus(SSP_MAROC_SLOT, 1);


}

/****************************************
 *  TRIGGER
 ****************************************/
void
sspMaroc_Trigger(int arg)
{
  int ii, slot, itime, gbready;
  int dCnt, len=0;

  BANKOPEN(SSP_MAROC_BANK, BT_UI4, blockLevel);
  ///////////////////////////////////////
  // SSP_CFG_SSPTYPE_HALLBRICH Readout //
  ///////////////////////////////////////
  dCnt=0;

  slot = SSP_MAROC_SLOT;

#ifdef DEBUG
  printf("Calling sspBReady(%d) ...\n", slot); fflush(stdout);
#endif
  for(itime=0; itime<100000; itime++)
    {
      gbready = sspBReady(slot);

      if(gbready)
	break;
#ifdef DEBUG
      else
	printf("SSP NOT READY (slot=%d)\n",slot);
#endif
    }

  if(!gbready)
    {
      printf("SSP NOT READY (slot=%d)\n",slot);

      ssp_not_ready_errors[slot]++;
    }
#ifdef DEBUG
  else
    printf("SSP IS READY (slot=%d)\n",slot);

  sspPrintEbStatus(slot);
  printf(" ");
#endif
  len = sspReadBlock(slot,dma_dabufp,0x10000,1);

#ifdef DEBUG
  // need to redefine tdcbuff to the_event->data[]
  /* printf("ssp tdcbuf[%2d]:", len); */
  /* for(jj=0;jj<(len>40?40:len);jj++) */
  /*   printf(" 0x%08x",tdcbuf[jj]); */

  printf(" ");
  sspPrintEbStatus(slot);
  printf("\n");
#endif

  dma_dabufp += len;
  dCnt += len;

  if(dCnt>0)
    {
      slot = SSP_MAROC_SLOT;
      if( ssp_not_ready_errors[slot] )
	{
	  printf("%s: SSP Read Errors: %4d\n",
		 __func__, ssp_not_ready_errors[slot]);
	}
    }

  BANKCLOSE;

}

/****************************************
 *  END
 ****************************************/
void
sspMaroc_End()
{
  tiStatus(0);

  sspStatus(SSP_MAROC_SLOT,1);

  printf("%s: Ended after %d blocks\n",
	 __func__, tiGetIntCount());

}

void
sspMaroc_Cleanup()
{

}

/*
  Local Variables:
  compile-command: "make -k ti_fa250_ssp_maroc_list.so"
  End:
*/

/*************************************************************************
 *
 *  fa250_rol_include.c -
 *
 *   Library of routines readout and buffering of events from JLab FADC
 *
 *    This one uses the VXS Signal Distribution (SD) for Clock,
 *        SyncReset, Busy, and Trigger distribution.
 */

#include "fadcLib.h"        /* library of FADC250 routines */
#include "fadc250Config.h"

/* FADC Library Variables */
extern int32_t nfadc;
extern uint32_t fadcA32Base;
#define NFADC     1
/* Address of first fADC250 */
#define FADC_ADDR (3<<19)
/* Increment address to find next fADC250 */
#define FADC_INCR (1<<19)
#define FADC_BANK 0x3

#define FADC_READ_CONF_FILE {			\
    fadc250Config("");				\
    if(configFilename)				\
      fadc250Config(configFilename);		\
  }

/* for the calculation of maximum data words in the block transfer */
unsigned int MAXFADCWORDS=0;

void
fa250_Download(char* configFilename)
{
  unsigned short iflag;
  int ifa, stat;

  /*****************
   *   FADC SETUP
   *****************/

  /* FADC Initialization flags */
  iflag = 0; /* NO SDC */
  iflag |= (1<<0);  /* VXS sync-reset */
  iflag |= FA_INIT_VXS_TRIG;  /* VXS trigger source */
  iflag |= FA_INIT_VXS_CLKSRC;  /* VXS 250MHz Clock source */

  fadcA32Base = 0x09000000;

  vmeSetQuietFlag(1);
  faInit(FADC_ADDR, FADC_INCR, NFADC, iflag);
  vmeSetQuietFlag(0);

  /* Just one FADC250 */
  if(nfadc == 1)
    faDisableMultiBlock();
  else
    faEnableMultiBlock(1);

  /* configure all modules based on config file */
  FADC_READ_CONF_FILE;

  for(ifa = 0; ifa < nfadc; ifa++)
    {
      /* Bus errors to terminate block transfers (preferred) */
      faEnableBusError(faSlot(ifa));

      /*trigger-related*/
      faResetMGT(faSlot(ifa),1);
      faSetTrigOut(faSlot(ifa), 7);

      /* Enable busy output when too many events are being processed */
      faSetTriggerBusyCondition(faSlot(ifa), 3);
    }

  sdSetActiveVmeSlots(faScanMask()); /* Tell the sd where to find the fadcs */

  sdStatus(0);
  faGStatus(0);

  printf("%s: done\n", __func__);

}

void
fa250_Prestart()
{
  int ifa;

  /* Program/Init VME Modules Here */
  for(ifa=0; ifa < nfadc; ifa++)
    {
      faSoftReset(faSlot(ifa),0);
      faResetToken(faSlot(ifa));
      faResetTriggerCount(faSlot(ifa));
      faEnableSyncReset(faSlot(ifa));
    }

  faGStatus(0);

  printf("%s: done\n", __func__);

}

void
fa250_Go()
{
  int32_t fadc_mode = 0;
  uint32_t pl=0, ptw=0, nsb=0, nsa=0, np=0;

  /* Get the current block level */
  blockLevel = tiGetCurrentBlockLevel();
  printf("%s: Current Block Level = %d\n",
	 __FUNCTION__,blockLevel);

  faGSetBlockLevel(blockLevel);

  /* Get the FADC mode and window size to determine max data size */
  faGetProcMode(faSlot(0), &fadc_mode, &pl, &ptw,
		&nsb, &nsa, &np);

  /* Set Max words from fadc (proc mode == 1 produces the most)
     nfadc * ( Block Header + Trailer + 2  # 2 possible filler words
               blockLevel * ( Event Header + Header2 + Timestamp1 + Timestamp2 +
	                      nchan * (Channel Header + (WindowSize / 2) )
             ) +
     scaler readout # 16 channels + header/trailer
   */
  MAXFADCWORDS = nfadc * (4 + blockLevel * (4 + 16 * (1 + (ptw / 2))) + 18);

  /*  Enable FADC */
  faGEnable(0, 0);

  /* Interrupts/Polling enabled after conclusion of rocGo() */
}

void
fa250_End()
{

  /* FADC Disable */
  faGDisable(0);

  /* FADC Event status - Is all data read out */
  faGStatus(0);

  printf("%s: done\n", __func__);

}

void
fa250_Trigger(int arg)
{
  int ifa = 0, stat, nwords, dCnt;
  unsigned int datascan, scanmask;
  int roType = 2, roCount = 0, blockError = 0;

  roCount = tiGetIntCount();

  /* Setup Address and data modes for DMA transfers
   *
   *  vmeDmaConfig(addrType, dataType, sstMode);
   *
   *  addrType = 0 (A16)    1 (A24)    2 (A32)
   *  dataType = 0 (D16)    1 (D32)    2 (BLK32) 3 (MBLK) 4 (2eVME) 5 (2eSST)
   *  sstMode  = 0 (SST160) 1 (SST267) 2 (SST320)
   */
  vmeDmaConfig(2,5,1);

  /* fADC250 Readout */
  BANKOPEN(FADC_BANK,BT_UI4,0);

  /* Mask of initialized modules */
  scanmask = faScanMask();
  /* Check scanmask for block ready up to 100 times */
  datascan = faGBlockReady(scanmask, 100);
  stat = (datascan == scanmask);

  if(stat)
    {
      if(nfadc == 1)
	roType = 1;   /* otherwise roType = 2   multiboard reaodut with token passing */
      nwords = faReadBlock(0, dma_dabufp, MAXFADCWORDS, roType);

      /* Check for ERROR in block read */
      blockError = faGetBlockError(1);

      if(blockError)
	{
	  printf("ERROR: Slot %d: in transfer (event = %d), nwords = 0x%x\n",
		 faSlot(ifa), roCount, nwords);

	  for(ifa = 0; ifa < nfadc; ifa++)
	    faResetToken(faSlot(ifa));

	  if(nwords > 0)
	    dma_dabufp += nwords;
	}
      else
	{
	  dma_dabufp += nwords;
	  faResetToken(faSlot(0));
	}
    }
  else
    {
      printf("ERROR: Event %d: Datascan != Scanmask  (0x%08x != 0x%08x)\n",
	     roCount, datascan, scanmask);
    }
  BANKCLOSE;


  /* Check for SYNC Event */
  if(tiGetSyncEventFlag() == 1)
    {
      for(ifa = 0; ifa < nfadc; ifa++)
	{
	  int davail = faBready(faSlot(ifa));
	  if(davail > 0)
	    {
	      printf("%s: ERROR: fADC250 Data available (%d) after readout in SYNC event \n",
		     __func__, davail);

	      while(faBready(faSlot(ifa)))
		{
		  vmeDmaFlush(faGetA32(faSlot(ifa)));
		}
	    }
	}
    }

}

void
fa250_Cleanup()
{

  printf("%s: Reset all FADCs\n",__func__);
  faGReset(1);

}

/*
  Local Variables:
  compile-command: "make -k ti_fa250_list.so"
  End:
 */

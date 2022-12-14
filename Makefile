#
# File:
#    Makefile
#
# Description:
#    Makefile for the coda primary and secondary readout lists
#    running on an Intel-based controller running Linux
#
#  2022 SOLID Beamtest
#
#
# Uncomment DEBUG line for debugging info ( -g and -Wall )
DEBUG=1
QUIET=1
#
ifeq ($(QUIET),1)
        Q = @
else
        Q =
endif

# Plug in your primary readout lists here.. CRL are found automatically
VMEROL	= ti_list.so ti_fa250_list.so ti_ssp_list.so ti_maroc_list.so
VMEROL += ti_fa250_ssp_list.so ti_fa250_ssp_maroc_list.so

# Add shared library dependencies here.  (jvme, ti, are already included)
ROLLIBS	= -lfadc -lmpd -lssp -lconfig

ifdef CODA_VME
INC_CODA_VME	= -isystem${CODA_VME}/include
endif

ifdef CODA_VME_LIB
LIB_CODA_VME	= -L${CODA_VME_LIB}
endif

LINUXVME_LIB	?= ../linuxvme/Linux-x86_64/lib
LINUXVME_INC	?= ../linuxvme/include

# DEFs for compiling primary readout lists
CC			= gcc
AR                      = ar
RANLIB                  = ranlib
ifdef DEBUG
CFLAGS			= -Wall -Wno-unused -g
else
CFLAGS			= -O3
endif
CFLAGS			+= -DLINUX -DDAYTIME=\""`date`"\"

INCS			= -I. -I${LINUXVME_INC} ${INC_CODA_VME} \
				-isystem${CODA}/common/include
LIBS			= -L. -L${LINUXVME_LIB} ${LIB_CODA_VME} -DJLAB \
				-lrt -lpthread -ljvme -lti -lsd -lts $(ROLLIBS)

# DEFs for compiling CODA readout lists
CCRL			= ${CODA_BIN}/ccrl
CODA_INCS		= -I. -I${LINUXVME_INC} ${INC_CODA_VME} -isystem${CODA}/common/include
CODA_LIBDIRS            = -L.
CODA_LIBS		=
CODA_DEFS		= -DLINUX -DDAYTIME=\""`date`"\"
ifdef DEBUG
CODA_CFLAGS		= -Wall -Wno-unused -g
else
CODA_CFLAGS		= -O
endif
CODA_CFLAGS		+= -w -fpic -shared ${CODA_INCS} ${CODA_LIBDIRS} \
			  ${CODA_LIBS} ${CODA_DEFS}
CRLFILES		= $(wildcard *.crl)
CFILES			= $(CRLFILES:.crl=.c)
SOBJS			= $(CRLFILES:.crl=.so)
DEPS			= $(VMEROL:%.so=%.d)
DEPS			+= $(CFILES:%.c=%.d)


all:  $(VMEROL) $(SOBJS)

%.c: %.crl
	@echo " CCRL   $@"
	${Q}${CCRL} $<

%.so: %.c
	@echo " CC     $@"
	${Q}$(CC) -fpic -shared  $(CFLAGS) $(INCS) $(LIBS) -DTI_MASTER \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) -o $@ $<

ti_fa250_list.so: ti_list.c
	@echo " CC     $@"
	${Q}$(CC) -fpic -shared  $(CFLAGS) $(INCS) $(LIBS) -DTI_MASTER -DUSE_FA250 \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) -o $@ $<

ti_ssp_list.so: ti_list.c
	@echo " CC     $@"
	${Q}$(CC) -fpic -shared  $(CFLAGS) $(INCS) $(LIBS) -DTI_MASTER -DUSE_SSP_MPD \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) -o $@ $<

ti_maroc_list.so: ti_list.c
	@echo " CC     $@"
	${Q}$(CC) -fpic -shared  $(CFLAGS) $(INCS) $(LIBS) -DTI_MASTER -DUSE_SSP_MAROC \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) -o $@ $<

ti_fa250_ssp_list.so: ti_list.c
	@echo " CC     $@"
	${Q}$(CC) -fpic -shared  $(CFLAGS) $(INCS) $(LIBS) -DTI_MASTER -DUSE_FA250 -DUSE_SSP_MPD \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) -o $@ $<

ti_fa250_ssp_maroc_list.so: ti_list.c
	@echo " CC     $@"
	${Q}$(CC) -fpic -shared  $(CFLAGS) $(INCS) $(LIBS) -DTI_MASTER -DUSE_FA250 -DUSE_SSP_MPD -DUSE_SSP_MAROC \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) -o $@ $<

%slave_list.so: %list.c
	@echo " CC     $@"
	${Q}$(CC) -fpic -shared  $(CFLAGS) $(INCS) $(LIBS) -DTI_SLAVE \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) -o $@ $<

# Prescription in case you use port 5 as the Slave port
%slave5_list.so: %list.c
	@echo " CC     $@"
	${Q}$(CC) -fpic -shared  $(CFLAGS) $(INCS) $(LIBS) -DTI_SLAVE5 \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) -o $@ $<

clean distclean:
	${Q}rm -f  $(VMEROL) $(SOBJS) $(CFILES) *~ $(DEPS) *.d.*

%.d: %.c
	@echo " DEP    $@"
	${Q}set -e; rm -f $@; \
	$(CC) -MM -shared $(INCS) $(CFLAGS) -DINIT_NAME=$(@:.so=__init) \
		-DINIT_NAME_POLL=$(@:.so=__poll) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.so $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

ti_fa250_list.d: ti_list.c
	@echo " DEP    $@"
	${Q}set -e; rm -f $@; \
	$(CC) -MM -shared  $(INCS) $(CFLAGS) -DUSE_FA250 \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) $< > $@.$$$$; \
	sed 's,\(.*\)\.o[ :]*,$(@:.d=).so $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

ti_ssp_list.d: ti_list.c
	@echo " DEP    $@"
	${Q}set -e; rm -f $@; \
	$(CC) -MM -shared  $(INCS) $(CFLAGS) -DUSE_SSP_MPD \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) $< > $@.$$$$; \
	sed 's,\(.*\)\.o[ :]*,$(@:.d=).so $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

ti_maroc_list.d: ti_list.c
	@echo " DEP    $@"
	${Q}set -e; rm -f $@; \
	$(CC) -MM -shared  $(INCS) $(CFLAGS) -DUSE_SSP_MAROC \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) $< > $@.$$$$; \
	sed 's,\(.*\)\.o[ :]*,$(@:.d=).so $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

ti_fa250_ssp_list.d: ti_list.c
	@echo " DEP    $@"
	${Q}set -e; rm -f $@; \
	$(CC) -MM -shared  $(INCS) $(CFLAGS) -DUSE_FA250 -DUSE_SSP_MPD \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) $< > $@.$$$$; \
	sed 's,\(.*\)\.o[ :]*,$(@:.d=).so $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

ti_fa250_ssp_maroc_list.d: ti_list.c
	@echo " DEP    $@"
	${Q}set -e; rm -f $@; \
	$(CC) -MM -shared  $(INCS) $(CFLAGS) -DUSE_FA250 -DUSE_SSP_MPD -DUSE_SSP_MAROC \
		-DINIT_NAME=$(@:.so=__init) -DINIT_NAME_POLL=$(@:.so=__poll) $< > $@.$$$$; \
	sed 's,\(.*\)\.o[ :]*,$(@:.d=).so $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$


-include $(DEPS)

.PHONY: all

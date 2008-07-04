#-----------------------------------------------------------------------------
#
# $Id$
#
#-----------------------------------------------------------------------------

FIGS := \
	app-config.fig \
	architecture.fig \
	fmmus.fig \
	fsm-change.fig \
	fsm-coedown.fig \
	fsm-eoe.fig \
	fsm-idle.fig \
	fsm-op.fig \
	fsm-sii.fig \
	fsm-slaveconf.fig \
	fsm-slavescan.fig \
	interrupt.fig \
	master-locks.fig \
	masters.fig \
	phases.fig \
	statetrans.fig

PDFS = $(FIGS:.fig=.pdf)

all: $(PDFS)

%.pdf: %.fig
	fig2dev -L pdf -z A4 -p xxx -c $< $@	

clean:
	@rm -rv $(PDFS)

#-----------------------------------------------------------------------------

#-----------------------------------------------------------------------------
#
# $Id$
#
#-----------------------------------------------------------------------------

FIGS := \
	app-config.fig \
	attach.fig \
	dc.fig \
	fmmus.fig \
	fsm-coedown.fig \
	fsm-eoe.fig \
	interrupt.fig \
	master-locks.fig \
	masters.fig \
	phases.fig \
	statetrans.fig

FIGPDFS = $(FIGS:.fig=.pdf)

SVGS := \
	architecture.svg

SVGPDFS = $(SVGS:.svg=.pdf)

all: $(FIGPDFS) $(SVGPDFS)

%.pdf: %.fig
	fig2dev -L pdf -z A4 -p xxx -c $< $@	

%.pdf: %.svg
	inkscape --export-pdf=$@ $<

clean:
	@rm -rv $(FIGPDFS) $(SVGPDFS)

#-----------------------------------------------------------------------------

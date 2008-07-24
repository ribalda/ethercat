#------------------------------------------------------------------------------
#
#  LaTeX Makefile
#
#  $Id$
#
#------------------------------------------------------------------------------

FILE := ethercat_doc

LATEX_OPTIONS := -file-line-error-style

COMMANDS := \
	alias \
	config \
	data \
	debug \
	domains \
	master \
	pdos \
	sdos \
	download \
	upload \
	slaves \
	sii_read \
	sii_write \
	states \
	xml

EXT_PREFIX := external/ethercat_
EXT_FILES := $(addprefix $(EXT_PREFIX),$(COMMANDS))
ETHERCAT_CMD := ../tool/ethercat
ETHERCAT_HELP := $(ETHERCAT_CMD) -h

#------------------------------------------------------------------------------

all: pdf

$(EXT_FILES): $(ETHERCAT_CMD)
	@mkdir -pv external
	@echo Creating $@...
	$(shell $(subst $(EXT_PREFIX),$(ETHERCAT_HELP) ,$@) > $@)

pdf: $(EXT_FILES)
	pdflatex $(LATEX_OPTIONS) $(FILE)

index:
	makeindex $(FILE)
	makeindex $(FILE).glo -s nomencl.ist -o $(FILE).gls

clean:
	@rm -f $(FILE).aux $(FILE).dvi $(FILE).idx \
		$(FILE).ilg $(FILE).ind $(FILE).log \
		$(FILE).out $(FILE).pdf $(FILE).ps \
		$(FILE).toc $(FILE).lot $(FILE).lof \
		$(FILE).lol $(FILE).glo $(FILE).gls \
		images/*.bak *~

#------------------------------------------------------------------------------

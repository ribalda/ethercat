#------------------------------------------------------------------------------
#
#  LaTeX Makefile
#
#  $Id$
#
#------------------------------------------------------------------------------

FILE := ethercat_doc

LATEX_OPTIONS := -file-line-error-style

#------------------------------------------------------------------------------

first: pdf

all: dvi pdf

dvi:
	latex $(LATEX_OPTIONS) \\nonstopmode\\input{$(FILE).tex}

pdf:
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

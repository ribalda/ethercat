#------------------------------------------------------------------------------
#
#  LaTeX Makefile
#
#  $Id$
#
#------------------------------------------------------------------------------

FILE := ethercat_doc

DVI_DEPEND := images/*.eps
PDF_DEPEND := images/*.pdf

LATEX_OPTIONS := -file-line-error-style

LATEX_CMD := latex $(LATEX_OPTIONS) \\nonstopmode\\input{$(FILE).tex}
PDFLATEX_CMD := pdflatex $(LATEX_OPTIONS) $(FILE)

#------------------------------------------------------------------------------

dvi: $(FILE).dvi

pdf: $(FILE).pdf

all: dvi pdf

$(FILE).dvi: $(FILE).tex $(DVI_DEPEND)
	@if [ ! -f $(FILE).toc ]; then touch $(FILE).toc; fi
	@if [ ! -f $(FILE).idx ]; then touch $(FILE).idx; fi
	@if [ ! -f $(FILE).glo ]; then touch $(FILE).glo; fi
	@cp $(FILE).toc $(FILE).toc.bak
	@cp $(FILE).idx $(FILE).idx.bak
	@cp $(FILE).glo $(FILE).glo.bak
	$(LATEX_CMD)
	@count=5; \
	while (grep -s 'cross-references' $(FILE).log || \
		! diff -q $(FILE).toc $(FILE).toc.bak) && \
		[ $$count -gt 0 ]; \
	do \
		cp $(FILE).toc $(FILE).toc.bak; \
		$(LATEX_CMD); \
		count=`expr $$count - 1`; \
	done
	@rerunlatex=0
	@if ! diff -q $(FILE).idx $(FILE).idx.bak; then \
		echo "Rebuilding Index"; \
		makeindex $(FILE); \
		rerunlatex=1; \
	fi
	@if ! diff -q $(FILE).glo $(FILE).glo.bak; then \
		echo "Rebuilding Glossary"; \
		makeindex $(FILE).glo -s nomencl.ist -o $(FILE).gls; \
		rerunlatex=1; \
	fi
	@if [ ! $$rerunlatex ]; then \
		echo "Re-running LaTeX..."; \
		$(LATEX_CMD); \
	fi
	@rm -f $(FILE).toc.bak
	@rm -f $(FILE).idx.bak
	@rm -f $(FILE).glo.bak

$(FILE).pdf: $(FILE).tex $(PDF_DEPEND)
	@if [ ! -f $(FILE).toc ]; then touch $(FILE).toc; fi
	@if [ ! -f $(FILE).idx ]; then touch $(FILE).idx; fi
	@if [ ! -f $(FILE).glo ]; then touch $(FILE).glo; fi
	@cp $(FILE).toc $(FILE).toc.bak
	@cp $(FILE).idx $(FILE).idx.bak
	@cp $(FILE).glo $(FILE).glo.bak
	$(PDFLATEX_CMD)
	@count=5; \
	while (grep -s 'cross-references' $(FILE).log || \
		! diff -q $(FILE).toc $(FILE).toc.bak) && \
		[ $$count -gt 0 ]; \
	do \
		cp $(FILE).toc $(FILE).toc.bak; \
		$(PDFLATEX_CMD); \
		count=`expr $$count - 1`; \
	done
	@rerunlatex=0
	@if ! diff -q $(FILE).idx $(FILE).idx.bak; then \
		echo "Rebuilding Index"; \
		makeindex $(FILE); \
		rerunlatex=1; \
	fi
	@if ! diff -q $(FILE).glo $(FILE).glo.bak; then \
		echo "Rebuilding Glossary"; \
		makeindex $(FILE).glo -s nomencl.ist -o $(FILE).gls; \
		rerunlatex=1; \
	fi
	@if [ ! $$rerunlatex ]; then \
		echo "Re-running LaTeX..."; \
		$(PDFLATEX_CMD); \
	fi
	@rm -f $(FILE).toc.bak
	@rm -f $(FILE).idx.bak
	@rm -f $(FILE).glo.bak

clean:
	@rm -f $(FILE).aux $(FILE).dvi $(FILE).idx \
			$(FILE).ilg $(FILE).ind $(FILE).log \
			$(FILE).out $(FILE).pdf $(FILE).ps \
			$(FILE).toc $(FILE).lot $(FILE).lof \
			$(FILE).lol $(FILE).glo $(FILE).gls \
			images/*.bak *~

#------------------------------------------------------------------------------

TEXDOC=trojita

cat rfc.bib trojita-misc.bib > trojita.bib

pdflatex -shell-escape "${TEXDOC}.tex" && \
    bibtex "${TEXDOC}.aux" && \
    pdflatex -shell-escape "${TEXDOC}.tex" && \
    pdflatex -shell-escape "${TEXDOC}.tex"

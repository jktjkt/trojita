TEXDOC=trojita

pdflatex -shell-escape "${TEXDOC}.tex" && \
    bibtex "${TEXDOC}.aux" && \
    pdflatex -shell-escape "${TEXDOC}.tex" && \
    pdflatex -shell-escape "${TEXDOC}.tex"

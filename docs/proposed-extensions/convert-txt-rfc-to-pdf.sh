#/bin/bash
FILE=`basename "${1}" .txt`

paps --font="Monospace 8" \
    --lpi 14 --paper=A4 --left-margin=140 --top-margin=120 \
    "${FILE}.txt" > "${FILE}.ps" \
    && ps2pdf "${FILE}.ps" > "${FILE}.pdf"

rm "${FILE}.ps"

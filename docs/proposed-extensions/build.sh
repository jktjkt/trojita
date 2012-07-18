#!/bin/bash

for DRAFT in draft-imap-qresync-arrived draft-imap-sendmail; do
    ~/.local/bin/xml2rfc "${DRAFT}.xml" --html --text \
        && ./convert-txt-rfc-to-pdf.sh "${DRAFT}"
done

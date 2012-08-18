#!/bin/bash

for DRAFT in draft-kundrat-qresync-arrived draft-kundrat-imap-submit draft-imap-incthread; do
    ~/.local/bin/xml2rfc "${DRAFT}.xml" --html --text \
        && ./convert-txt-rfc-to-pdf.sh "${DRAFT}"
done

#!/bin/bash

VERSION=$1
if [[ "x$VERSION" == "x" ]]; then
    echo "Usage: $0 version"
    exit 1
fi

TAG=v$VERSION
git describe $TAG &>/dev/null
if [[ $? -ne 0 ]]; then
    echo "Cannot find the git tag $TAG"
    exit 1
fi

git archive --format=tar --prefix=trojita-$VERSION/ $TAG \
    LICENSE README *.pri *.pro src tests qtc_packaging -o trojita-$VERSION.tar
ln -s . trojita-$VERSION
python l10n-fetch-po-files.py
tar rf trojita-$VERSION.tar trojita-$VERSION/po/trojita_common_*.po
rm trojita-$VERSION
bzip2 -9 trojita-$VERSION.tar
gpg --armor --detach-sign trojita-$VERSION.tar.bz2

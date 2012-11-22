#! /bin/sh

$EXTRACTRC `find src -name '*.ui' -o -name '*.rc'` >> rc.cpp
$XGETTEXT_QT rc.cpp `find src/ -name '*.cpp'` -o $podir/trojita_common.pot

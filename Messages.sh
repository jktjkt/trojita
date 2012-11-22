#! /bin/sh

$EXTRACTRC `find src -name '*.ui' -o -name '*.rc'` >> rc.cpp
$XGETTEXT rc.cpp -o $podir/trojita.pot
$XGETTEXT_QT `find src/ -name '*.cpp'` -o $podir/trojita_qt.pot

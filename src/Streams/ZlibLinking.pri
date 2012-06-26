unix {
    CONFIG += link_pkgconfig
    PKGCONFIG += zlib
    DEFINES += TROJITA_HAVE_ZLIB
    HEADERS += TrojitaZlibStatus.h
}

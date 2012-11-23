unix.!disable_zlib {
    CONFIG += link_pkgconfig
    PKGCONFIG += zlib
    DEFINES += TROJITA_HAVE_ZLIB
}

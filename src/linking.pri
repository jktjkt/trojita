for(what, trojita_libs) {
    mylib = $$replace(what,/,)
    unix {
        mypath = $$join(what,,$${myprefix},)
    }
    win32 {
        CONFIG( debug, debug|release ) {
            mypath = $$join(what,,$${myprefix},/debug)
        } else {
            mypath = $$join(what,,$${myprefix},/release)
        }
    }
    LIBS += $$join(mypath,,-L,)
    LIBS += $$join(mylib,,-l,)
    !win32-msvc* {
        PRE_TARGETDEPS += $$join(mypath,,,$$join(mylib,,/lib,.a))
    }
}

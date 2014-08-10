/*
 * This is based on work by D.J. Capelis, modified by Jan Kundrát.
 * http://stackoverflow.com/questions/69859/how-could-i-intercept-linux-sys-calls
 * */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#define _FCNTL_H
#include <bits/fcntl.h>
#include <string.h>

extern int errorno;

int (*_open)(const char * pathname, int flags, ...);
int (*_open64)(const char * pathname, int flags, ...);

const char * dev_random = "/dev/random";
const char * dev_urandom = "/dev/urandom";

int open(const char * pathname, int flags, mode_t mode)
{
    _open = (int (*)(const char * pathname, int flags, ...)) dlsym(RTLD_NEXT, "open");
    if (strcmp(pathname, dev_random) == 0)
        return _open(dev_urandom, flags, mode);
    else
        return _open(pathname, flags, mode);
}

int open64(const char * pathname, int flags, mode_t mode)
{
    _open64 = (int (*)(const char * pathname, int flags, ...)) dlsym(RTLD_NEXT, "open64");
    if (strcmp(pathname, dev_random) == 0)
        return _open64(dev_urandom, flags, mode);
    else
        return _open64(pathname, flags, mode);
}

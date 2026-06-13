#ifndef ERRNO_HPP
#define ERRNO_HPP
#include <cerrno>
void resetErrno(){
    errno=0;
}

#endif
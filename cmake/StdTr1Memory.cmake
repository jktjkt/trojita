# make sure we can use std::tr1::memory
include( CheckCXXSourceCompiles )
CHECK_CXX_SOURCE_COMPILES(
    "#include <tr1/memory>
    int main() { std::tr1::shared_ptr<int> i;
    return 0;}"
    TR1_SHARED_PTR_FOUND)
if(NOT TR1_SHARED_PTR_FOUND)
    find_package(Boost 1.34 REQUIRED)
    # FIXME: FindBoost.cmake doesn't check version, so we should check it here...
    FILE(MAKE_DIRECTORY tr1)
    FILE(WRITE "tr1/memory"
"
#ifndef TROJITA_INCLUDED_TR1_MEMORY
#define TROJITA_INCLUDED_TR1_MEMORY 1
// This file was created because CMake didn't find working
// std::tr1::shared_ptr<>. Its purpose is to use classes from Boost for such
// purposes.
#include <boost/shared_ptr.hpp>
namespace std
{
    namespace tr1
    {
        using boost::shared_ptr;
    }
}
#endif // TROJITA_INCLUDED_TR1_MEMORY
")
    SET(CMAKE_REQUIRED_LIBRARIES Boost)
endif(NOT TR1_SHARED_PTR_FOUND)



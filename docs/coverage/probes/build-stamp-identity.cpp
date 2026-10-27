// Diagnostic only: call the real runtime getter used by troubleshooting/UI.
#include "Source/Support/Diagnostics.h"
#include <cstdio>
int main(int argc,char** argv)
{
    const auto actual=luthier::getFullVersionString();
    std::printf("%s\n",actual.toRawUTF8());
    return argc==2 && actual==argv[1] ? 0:1;
}

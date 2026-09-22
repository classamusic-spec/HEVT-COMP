#include "TestFramework.h"

int main (int argc, char** argv)
{
    return heat::test::runAll (argc > 1 ? argv[1] : nullptr);
}

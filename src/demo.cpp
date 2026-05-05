#include <cstdio>

#include "kaslr.hpp"

int main()
{
    if (const auto base = kaslr::leak())
    {
        std::printf(
            "kernel_base hint = 0x%016llx\n",
            static_cast<unsigned long long>(*base)
        );
        return 0;
    }
    else
    {
        std::printf(
            "status = %u\n",
            static_cast<unsigned>(base.error())
        );
        return 1;
    }
}

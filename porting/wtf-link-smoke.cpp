// Link-only fixture: no execution or RNG behavior is claimed by the builder.
#include "config.h"
#include <wtf/RandomDevice.h>
#include <array>

int main()
{
    std::array<uint8_t, 32> bytes { };
    WTF::RandomDevice device;
    device.cryptographicallyRandomValues(bytes);
    return 0;
}

#include <iostream>
#include "mapper_resolve.hpp"

ROMMemManager *mapper_resolve(struct nes_header *nesh, struct nes_data *nesd) {
    std::printf("Resolving mapper %d\n", nesh->MAPPER_NB);
    switch (nesh->MAPPER_NB)
    {
    case 0x00:
        /* Default */
        return new ROMDefault(nesd);
    case 0x02:
        /* UNROM */
        return new ROMMapper2(nesd);
    
    default:
        std::printf("Mapper not recognized\n");
        return nullptr;
    }
}
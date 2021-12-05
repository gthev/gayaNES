#ifndef GAYA_MAPPER_RESOLVE_HPP
#define GAYA_MAPPER_RESOLVE_HPP
#include "../mem.hpp"
#include "../exceptions.hpp"
#include "../nes_loaders/ines.hpp"

/*
=============
Mappers List
=============
*/

/*
Mapper 2, or UNROM
*/

class ROMMapper2 : public ROMDefault
{
public:
    ROMMapper2(){};
    ROMMapper2(struct nes_data *nesdata);
    virtual ~ROMMapper2(){};
    virtual void        write(MEMADDR a, UINT8 val);
    virtual void        write_pt(MEMADDR a, UINT8 val);
    virtual ROMMemManager
                        *save_state();
};

/*
Mapper resolution
*/

ROMMemManager *mapper_resolve(struct nes_header *nesh, struct nes_data *nesd);

#endif
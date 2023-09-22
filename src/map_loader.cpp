
#include <bank.h>
#include <neslib.h>
#include <peekpoke.h>

#include "common.hpp"
#include "dungeon_generator.hpp"
#include "graphics.hpp"
#include "map_loader.hpp"
#include "map.hpp"
#include "rand.hpp"
#include "soa.h"

struct SectionLookup {
    const char* nametable;
    const char* chr;
    const char* attribute;
    const char* palette;
    u8 mirroring;
};

__attribute__((section(".noinit.late"))) const soa::Array<SectionLookup, 6> section_lut = {
    {bottom_bin, room_updown_chr, bottom_attr, updown_palette, MIRROR_HORIZONTAL},
    {left_bin, room_leftright_chr, left_attr, leftright_palette, MIRROR_VERTICAL},
    {right_bin, room_leftright_chr, right_attr, leftright_palette, MIRROR_VERTICAL},
    {single_bin, room_single_chr, single_attr, single_palette, MIRROR_VERTICAL},
    {start_bin, room_start_chr, start_attr, start_palette, MIRROR_VERTICAL},
    {top_bin, room_updown_chr, top_attr, updown_palette, MIRROR_HORIZONTAL},
};
#define SOA_STRUCT SectionLookup
#define SOA_MEMBERS MEMBER(nametable) MEMBER(chr) MEMBER(attribute) MEMBER(palette) MEMBER(mirroring)
#include "soa-struct.inc"

Room room;
Section lead;
Section side;

// u8 section_id;

static void read_map_room(u8 section_id) {
    // switch the bank to the bank with the save data
    set_chr_bank(3);

    // Really lame, but i don't have a better idea for this yet
    // just temporarily load the section into lead to get the room id
    u8 room_id = Dungeon::load_room_id_by_section(section_id);
    Dungeon::load_room_from_chrram(room_id);
    Dungeon::load_section_to_lead(room.lead_id);
    Dungeon::load_section_to_side(room.side_id);
}

static void load_section(const Section& section) {

    u16 nmt_addr = ((u16)section.nametable) << 8;
    vram_adr(nmt_addr);
    const char* nametable = section_lut[static_cast<u8>(section.room_base)].nametable.get();
    donut_decompress(nametable);

    // load the attributes for this nametable into a buffer that we can update with the
    // objects as they are loaded
    std::array<u8, 64> attr_buffer;
    const char* attr = section_lut[static_cast<u8>(section.room_base)].attribute.get();
    for (u8 i = 0; i < 64; ++i) {
        attr_buffer[i] = attr[i];
    }

    // Load all objects for this side of the map

    // and then write out all the attributes
    vram_adr(nmt_addr + 0x3c0);
    vram_write(attr_buffer.data(), 64);
}

namespace MapLoader {

    void load_map(u8 section_id) {
        // TODO turn off DPCM if its playing
        // ppu_off();

        // switch to the 16kb bank that holds the level
        set_prg_bank(1);

        // TODO save the previous room state when leaving?
        read_map_room(section_id);

        set_chr_bank(0);
        vram_adr(0x00);
        const char* chr = section_lut[static_cast<u8>(lead.room_base)].chr.get();
        donut_decompress(chr);

        // always add the kitty tile to the CHR
        vram_adr(0x1000);
        donut_decompress(&kitty_chr);

        load_section(lead);

        if (side.room_id != Dungeon::NO_EXIT) {
            load_section(side);
        }

        // if we are using a vertical configuration, update the mirroring config
        u8 mirroring = section_lut[static_cast<u8>(lead.room_base)].mirroring.get();
        set_mirroring(mirroring);


        const char* palette = section_lut[static_cast<u8>(lead.room_base)].palette.get();
        pal_bg(palette);

        pal_bright(0);

        // TODO figure out the proper scroll position
        scroll(0, 0);

        // restore prg bank to the code bank
        set_prg_bank(2);
    }
}

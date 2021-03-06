// -*- mode: c++; -*-

#ifndef FREESPACE2_GRAPHICS_SOFTWARE_FONT_INTERNAL_HH
#define FREESPACE2_GRAPHICS_SOFTWARE_FONT_INTERNAL_HH

#include "defs.hh"

namespace font {
struct font_char  {
    int spacing;
    int byte_width;
    int offset;
    short kerning_entry;
    short user_data;
};

struct font_kernpair  {
    char c1, c2;
    signed char offset;
};

struct font  {
    char filename[MAX_FILENAME_LEN];
    int id;      //!< Should be 'VFNT'
    int version; //!< font version
    int num_chars;
    int first_ascii;
    int w;
    int h;
    int num_kern_pairs;
    int kern_data_size;
    int char_data_size;
    int pixel_data_size;
    font_kernpair* kern_data;
    font_char* char_data;
    ubyte* pixel_data;

    // Data for 3d cards
    int bitmap_id;  //!< A bitmap representing the font data
    int bm_w, bm_h; //!< Bitmap width and height
    ubyte* bm_data; //!< The actual font data
    int* bm_u;      //!< U offset of each character
    int* bm_v;      //!< V offset of each character

    font ();
    ~font ();
};
} // namespace font

#endif // FREESPACE2_GRAPHICS_SOFTWARE_FONT_INTERNAL_HH

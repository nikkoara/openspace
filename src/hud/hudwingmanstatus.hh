// -*- mode: c++; -*-

#ifndef FREESPACE2_HUD_HUDWINGMANSTATUS_HH
#define FREESPACE2_HUD_HUDWINGMANSTATUS_HH

#include "defs.hh"

#include "hud/hud.hh"

struct wing;
class ship;
class p_object;

void hud_init_wingman_status_gauge ();
void hud_wingman_status_update ();
void hud_wingman_status_render ();
void hud_wingman_status_init_flash ();
int hud_wingman_status_maybe_flash (int wing_index, int wing_pos);
void hud_set_wingman_status_dead (int wing_index, int wing_pos);
void hud_set_wingman_status_departed (int wing_index, int wing_pos);
void hud_set_wingman_status_alive (int wing_index, int wing_pos);
void hud_set_wingman_status_none (int wing_index, int wing_pos);
void hud_wingman_status_start_flash (int wing_index, int wing_pos);
void hud_wingman_status_set_index (wing* wingp, ship* shipp, p_object* pobjp);

class HudGaugeWingmanStatus : public HudGauge {
protected:
    hud_frames Wingman_status_left;
    hud_frames Wingman_status_middle;
    hud_frames Wingman_status_right;
    hud_frames Wingman_status_dots;

    int header_offsets[2];
    bool fixed_header_position;
    int left_frame_end_x;
    int right_frame_start_offset;

    int actual_origin[2];
    int single_wing_offsets[2];
    int multiple_wing_offsets[2];
    int wing_width;
    int wing_name_offsets[2];

    enum { GROW_LEFT, GROW_RIGHT, GROW_DOWN };
    int grow_mode;

    int wingmate_offsets[MAX_SHIPS_PER_WING][2];

    int next_flash[MAX_SQUADRON_WINGS][MAX_SHIPS_PER_WING];
    int flash_status;

public:
    HudGaugeWingmanStatus ();
    void initBitmaps (
        char* fname_left, char* fname_middle, char* fname_right,
        char* fname_dots);
    void initHeaderOffsets (int x, int y);
    void initFixedHeaderPosition (bool fixed);
    void initLeftFrameEndX (int x);
    void initSingleWingOffsets (int x, int y);
    void initMultipleWingOffsets (int x, int y);
    void initWingWidth (int w);
    void initRightBgOffset (int offset);
    void initWingNameOffsets (int x, int y);
    void initWingmate1Offsets (int x, int y);
    void initWingmate2Offsets (int x, int y);
    void initWingmate3Offsets (int x, int y);
    void initWingmate4Offsets (int x, int y);
    void initWingmate5Offsets (int x, int y);
    void initWingmate6Offsets (int x, int y);
    void initGrowMode (int mode);
    void pageIn () override;
    void initialize () override;
    void render (float frametime) override;
    void renderBackground (int num_wings_to_draw);
    void renderDots (int wing_index, int screen_index, int num_wings_to_draw);
    void initFlash ();
    bool maybeFlashStatus (int wing_index, int wing_pos);
};

#endif // FREESPACE2_HUD_HUDWINGMANSTATUS_HH
// -*- mode: c++; -*-

#include "defs.hh"
#include "cutscene/cutscenes.hh"
#include "freespace2/freespace.hh"
#include "gamesnd/eventmusic.hh"
#include "hud/hudconfig.hh"
#include "io/joy.hh"
#include "io/mouse.hh"
#include "menuui/techmenu.hh"
#include "mission/missioncampaign.hh"
#include "mission/missionload.hh"
#include "missionui/missionscreencommon.hh"
#include "missionui/missionshipchoice.hh"
#include "pilotfile/pilotfile.hh"
#include "playerman/player.hh"
#include "ship/ship.hh"
#include "sound/audiostr.hh"
#include "stats/medals.hh"
#include "weapon/weapon.hh"
#include "assert/assert.hh"
#include "log/log.hh"

#define REDALERT_INTERNAL
#include <iostream>
#include <sstream>
#include <limits>
#include <filesystem>

#include "missionui/redalert.hh"

namespace fs = std::filesystem;

void pilotfile::csg_read_flags () {
    // tips?
    p->tips = (int)cfread_ubyte (cfp);

    // avoid having to read everything to get the rank
    if (csg_ver >= 5) { p->stats.rank = cfread_int (cfp); }
}

void pilotfile::csg_write_flags () {
    startSection (Section::Flags);

    // tips
    cfwrite_ubyte ((ubyte)p->tips, cfp);

    // avoid having to read everything to get the rank
    cfwrite_int (p->stats.rank, cfp);

    endSection ();
}

void pilotfile::csg_read_info () {
    char t_string[NAME_LENGTH + 1] = { '\0' };
    index_list_t ilist;
    int idx, list_size = 0;
    ubyte allowed = 0;

    if (!m_have_flags) { throw "Info before Flags!"; }

    //
    // NOTE: lists may contain missing/invalid entries for current data
    // this is not necessarily fatal
    //

    // ship list (NOTE: may contain more than MAX_SHIP_CLASSES)
    list_size = cfread_int (cfp);

    for (idx = 0; idx < list_size; idx++) {
        cfread_string_len (t_string, NAME_LENGTH, cfp);

        ilist.name = t_string;
        ilist.index = ship_info_lookup (t_string);

        ship_list.push_back (ilist);
    }

    // weapon list (NOTE: may contain more than MAX_WEAPON_TYPES)
    list_size = cfread_int (cfp);

    for (idx = 0; idx < list_size; idx++) {
        cfread_string_len (t_string, NAME_LENGTH, cfp);

        ilist.name = t_string;
        ilist.index = weapon_info_lookup (t_string);

        weapon_list.push_back (ilist);
    }

    // intel list (NOTE: may contain more than MAX_INTEL_ENTRIES)
    list_size = cfread_int (cfp);

    for (idx = 0; idx < list_size; idx++) {
        cfread_string_len (t_string, NAME_LENGTH, cfp);

        ilist.name = t_string;
        ilist.index = intel_info_lookup (t_string);

        intel_list.push_back (ilist);
    }

    // medals list (NOTE: may contain more than Num_medals)
    list_size = cfread_int (cfp);

    for (idx = 0; idx < list_size; idx++) {
        cfread_string_len (t_string, NAME_LENGTH, cfp);

        ilist.name = t_string;
        ilist.index = medals_info_lookup (t_string);

        medals_list.push_back (ilist);
    }

    // last ship flown (index into ship_list)
    idx = cfread_int (cfp);

    // check the idx is within bounds
    ASSERTX (
        (idx < (int)ship_list.size ()),
        "Campaign file contains an incorrect value for the last flown ship "
        "class. No data in ship_list for ship number %d.",
        idx);
    if (idx >= (int)ship_list.size ())
        idx = -1;
    else if (idx != -1)
        p->last_ship_flown_si_index = ship_list[idx].index;
    else
        p->last_ship_flown_si_index = -1;

    // progression state
    Campaign.prev_mission = cfread_int (cfp);
    Campaign.next_mission = cfread_int (cfp);

    // loop state
    Campaign.loop_reentry = cfread_int (cfp);
    Campaign.loop_enabled = cfread_int (cfp);

    // missions completed
    Campaign.num_missions_completed = cfread_int (cfp);

    // allowed ships
    list_size = (int)ship_list.size ();
    for (idx = 0; idx < list_size; idx++) {
        allowed = cfread_ubyte (cfp);

        if (allowed) {
            if (ship_list[idx].index >= 0) {
                Campaign.ships_allowed[ship_list[idx].index] = 1;
            }
            else {
                WARNINGF (LOCATION,"Found invalid ship \"%s\" in campaign save file. Skipping...",ship_list[idx].name.c_str ());
            }
        }
    }

    // allowed weapons
    list_size = (int)weapon_list.size ();
    for (idx = 0; idx < list_size; idx++) {
        allowed = cfread_ubyte (cfp);

        if (allowed) {
            if (weapon_list[idx].index >= 0) {
                Campaign.weapons_allowed[weapon_list[idx].index] = 1;
            }
            else {
                WARNINGF (LOCATION,"Found invalid weapon \"%s\" in campaign save file. Skipping...",weapon_list[idx].name.c_str ());
            }
        }
    }

    if (csg_ver >= 2) {
        // single/campaign squad name & image
        cfread_string_len (p->s_squad_name, NAME_LENGTH, cfp);
        cfread_string_len (p->s_squad_filename, MAX_FILENAME_LEN, cfp);
    }

    // if anything we need/use was missing then it should be considered fatal
    if (m_data_invalid) { throw "Invalid data for CSG!"; }
}

void pilotfile::csg_write_info () {
    int idx;

    startSection (Section::Info);

    // ship list
    cfwrite_int (static_cast< int > (Ship_info.size ()), cfp);

    for (auto it = Ship_info.cbegin (); it != Ship_info.cend (); ++it) {
        cfwrite_string_len (it->name, cfp);
    }

    // weapon list
    cfwrite_int (Num_weapon_types, cfp);

    for (idx = 0; idx < Num_weapon_types; idx++) {
        cfwrite_string_len (Weapon_info[idx].name, cfp);
    }

    // intel list
    cfwrite_int (Intel_info_size, cfp);

    for (idx = 0; idx < Intel_info_size; idx++) {
        cfwrite_string_len (Intel_info[idx].name, cfp);
    }

    // medals list
    cfwrite_int (Num_medals, cfp);

    for (idx = 0; idx < Num_medals; idx++) {
        cfwrite_string_len (Medals[idx].name, cfp);
    }

    // last ship flown
    cfwrite_int (p->last_ship_flown_si_index, cfp);

    // progression state
    cfwrite_int (Campaign.prev_mission, cfp);
    cfwrite_int (Campaign.next_mission, cfp);

    // loop state
    cfwrite_int (Campaign.loop_reentry, cfp);
    cfwrite_int (Campaign.loop_enabled, cfp);

    // missions completed
    cfwrite_int (Campaign.num_missions_completed, cfp);

    // allowed ships
    for (idx = 0; idx < static_cast< int > (Ship_info.size ()); idx++) {
        cfwrite_ubyte (Campaign.ships_allowed[idx], cfp);
    }

    // allowed weapons
    for (idx = 0; idx < Num_weapon_types; idx++) {
        cfwrite_ubyte (Campaign.weapons_allowed[idx], cfp);
    }

    // single/campaign squad name & image
    cfwrite_string_len (p->s_squad_name, cfp);
    cfwrite_string_len (p->s_squad_filename, cfp);

    endSection ();
}

void pilotfile::csg_read_missions () {
    int i, j, idx, list_size;
    cmission* missionp;

    if (!m_have_info) { throw "Missions before Info!"; }

    for (i = 0; i < Campaign.num_missions_completed; i++) {
        idx = cfread_int (cfp);
        missionp = &Campaign.missions[idx];

        missionp->completed = 1;

        // flags
        missionp->flags = cfread_int (cfp);

        // goals
        missionp->num_goals = cfread_int (cfp);

        if (missionp->num_goals > 0) {
            missionp->goals =
                (mgoal*)malloc (missionp->num_goals * sizeof (mgoal));
            ASSERT (missionp->goals != NULL);

            memset (missionp->goals, 0, missionp->num_goals * sizeof (mgoal));

            for (j = 0; j < missionp->num_goals; j++) {
                cfread_string_len (missionp->goals[j].name, NAME_LENGTH, cfp);
                missionp->goals[j].status = cfread_char (cfp);
            }
        }

        // events
        missionp->num_events = cfread_int (cfp);

        if (missionp->num_events > 0) {
            missionp->events =
                (mevent*)malloc (missionp->num_events * sizeof (mevent));
            ASSERT (missionp->events != NULL);

            memset (
                missionp->events, 0, missionp->num_events * sizeof (mevent));

            for (j = 0; j < missionp->num_events; j++) {
                cfread_string_len (missionp->events[j].name, NAME_LENGTH, cfp);
                missionp->events[j].status = cfread_char (cfp);
            }
        }

        // variables
        missionp->num_variables = cfread_int (cfp);

        if (missionp->num_variables > 0) {
            missionp->variables = (sexp_variable*)malloc (
                missionp->num_variables * sizeof (sexp_variable));
            ASSERT (missionp->variables != NULL);

            memset (
                missionp->variables, 0,
                missionp->num_variables * sizeof (sexp_variable));

            for (j = 0; j < missionp->num_variables; j++) {
                missionp->variables[j].type = cfread_int (cfp);
                cfread_string_len (
                    missionp->variables[j].text, TOKEN_LENGTH, cfp);
                cfread_string_len (
                    missionp->variables[j].variable_name, TOKEN_LENGTH, cfp);
            }
        }

        // scoring stats
        missionp->stats.score = cfread_int (cfp);
        missionp->stats.rank = cfread_int (cfp);
        missionp->stats.assists = cfread_int (cfp);
        missionp->stats.kill_count = cfread_int (cfp);
        missionp->stats.kill_count_ok = cfread_int (cfp);
        missionp->stats.bonehead_kills = cfread_int (cfp);

        missionp->stats.p_shots_fired = cfread_uint (cfp);
        missionp->stats.p_shots_hit = cfread_uint (cfp);
        missionp->stats.p_bonehead_hits = cfread_uint (cfp);

        missionp->stats.s_shots_fired = cfread_uint (cfp);
        missionp->stats.s_shots_hit = cfread_uint (cfp);
        missionp->stats.s_bonehead_hits = cfread_uint (cfp);

        // ship kills (scoring)
        list_size = (int)ship_list.size ();
        for (j = 0; j < list_size; j++) {
            idx = cfread_int (cfp);

            if (ship_list[j].index >= 0) {
                missionp->stats.kills[ship_list[j].index] = idx;
            }
        }

        // medals (scoring)
        list_size = (int)medals_list.size ();
        for (j = 0; j < list_size; j++) {
            idx = cfread_int (cfp);

            if (medals_list[j].index >= 0) {
                missionp->stats.medal_counts[medals_list[j].index] = idx;
            }
        }
    }
}

void pilotfile::csg_write_missions () {
    int idx, j;
    cmission* missionp;

    startSection (Section::Missions);

    for (idx = 0; idx < MAX_CAMPAIGN_MISSIONS; idx++) {
        if (Campaign.missions[idx].completed) {
            missionp = &Campaign.missions[idx];

            cfwrite_int (idx, cfp);

            // flags
            cfwrite_int (missionp->flags, cfp);

            // goals
            cfwrite_int (missionp->num_goals, cfp);

            for (j = 0; j < missionp->num_goals; j++) {
                cfwrite_string_len (missionp->goals[j].name, cfp);
                cfwrite_char (missionp->goals[j].status, cfp);
            }

            // events
            cfwrite_int (missionp->num_events, cfp);

            for (j = 0; j < missionp->num_events; j++) {
                cfwrite_string_len (missionp->events[j].name, cfp);
                cfwrite_char (missionp->events[j].status, cfp);
            }

            // variables
            cfwrite_int (missionp->num_variables, cfp);

            for (j = 0; j < missionp->num_variables; j++) {
                cfwrite_int (missionp->variables[j].type, cfp);
                cfwrite_string_len (missionp->variables[j].text, cfp);
                cfwrite_string_len (missionp->variables[j].variable_name, cfp);
            }

            // scoring stats
            cfwrite_int (missionp->stats.score, cfp);
            cfwrite_int (missionp->stats.rank, cfp);
            cfwrite_int (missionp->stats.assists, cfp);
            cfwrite_int (missionp->stats.kill_count, cfp);
            cfwrite_int (missionp->stats.kill_count_ok, cfp);
            cfwrite_int (missionp->stats.bonehead_kills, cfp);

            cfwrite_uint (missionp->stats.p_shots_fired, cfp);
            cfwrite_uint (missionp->stats.p_shots_hit, cfp);
            cfwrite_uint (missionp->stats.p_bonehead_hits, cfp);

            cfwrite_uint (missionp->stats.s_shots_fired, cfp);
            cfwrite_uint (missionp->stats.s_shots_hit, cfp);
            cfwrite_uint (missionp->stats.s_bonehead_hits, cfp);

            // ship kills (scoring)
            for (j = 0; j < static_cast< int > (Ship_info.size ()); j++) {
                cfwrite_int (missionp->stats.kills[j], cfp);
            }

            // medals earned (scoring)
            for (j = 0; j < Num_medals; j++) {
                cfwrite_int (missionp->stats.medal_counts[j], cfp);
            }
        }
    }

    endSection ();
}

void pilotfile::csg_read_techroom () {
    int idx, list_size = 0;
    ubyte visible;

    if (!m_have_info) { throw "Techroom before Info!"; }

    // visible ships
    list_size = (int)ship_list.size ();
    for (idx = 0; idx < list_size; idx++) {
        visible = cfread_ubyte (cfp);

        if (visible) {
            if (ship_list[idx].index >= 0) {
                Ship_info[ship_list[idx].index].flags.set (
                    Ship::Info_Flags::In_tech_database);
            }
            else {
                WARNINGF (LOCATION,"Found invalid ship \"%s\" in campaign save file. Skipping...",ship_list[idx].name.c_str ());
            }
        }
    }

    // visible weapons
    list_size = (int)weapon_list.size ();
    for (idx = 0; idx < list_size; idx++) {
        visible = cfread_ubyte (cfp);

        if (visible) {
            if (weapon_list[idx].index >= 0) {
                Weapon_info[weapon_list[idx].index].wi_flags.set (
                    Weapon::Info_Flags::In_tech_database);
            }
            else {
                WARNINGF (LOCATION,"Found invalid weapon \"%s\" in campaign save file. Skipping...",weapon_list[idx].name.c_str ());
            }
        }
    }

    // visible intel entries
    list_size = (int)intel_list.size ();
    for (idx = 0; idx < list_size; idx++) {
        visible = cfread_ubyte (cfp);

        if (visible) {
            if (intel_list[idx].index >= 0) {
                Intel_info[intel_list[idx].index].flags |=
                    IIF_IN_TECH_DATABASE;
            }
            else {
                WARNINGF (LOCATION,"Found invalid intel entry \"%s\" in campaign save file. Skipping...",intel_list[idx].name.c_str ());
            }
        }
    }

    // if anything we need/use was missing then it should be considered fatal
    if (m_data_invalid) { throw "Invalid data for CSG!"; }
}

void pilotfile::csg_write_techroom () {
    int idx;
    ubyte visible;

    startSection (Section::Techroom);

    // visible ships
    for (auto it = Ship_info.cbegin (); it != Ship_info.cend (); ++it) {
        if ((it->flags[Ship::Info_Flags::In_tech_database]) &&
            !(it->flags[Ship::Info_Flags::Default_in_tech_database])) {
            visible = 1;
        }
        else {
            visible = 0;
        }

        cfwrite_ubyte (visible, cfp);
    }

    // visible weapons
    for (idx = 0; idx < Num_weapon_types; idx++) {
        // only visible if not in techroom by default
        if ((Weapon_info[idx]
                 .wi_flags[Weapon::Info_Flags::In_tech_database]) &&
            !(Weapon_info[idx]
                  .wi_flags[Weapon::Info_Flags::Default_in_tech_database])) {
            visible = 1;
        }
        else {
            visible = 0;
        }

        cfwrite_ubyte (visible, cfp);
    }

    // visible intel entries
    for (idx = 0; idx < Intel_info_size; idx++) {
        // only visible if not in techroom by default
        if ((Intel_info[idx].flags & IIF_IN_TECH_DATABASE) &&
            !(Intel_info[idx].flags & IIF_DEFAULT_IN_TECH_DATABASE)) {
            visible = 1;
        }
        else {
            visible = 0;
        }

        cfwrite_ubyte (visible, cfp);
    }

    endSection ();
}

void pilotfile::csg_read_loadout () {
    int j, count, ship_idx = -1, wep_idx = -1;
    size_t idx, list_size = 0;

    if (!m_have_info) { throw "Loadout before Info!"; }

    // base info
    cfread_string_len (Player_loadout.filename, MAX_FILENAME_LEN, cfp);
    cfread_string_len (Player_loadout.last_modified, DATE_TIME_LENGTH, cfp);

    // ship pool
    list_size = ship_list.size ();
    for (idx = 0; idx < list_size; idx++) {
        count = cfread_int (cfp);

        if (ship_list[idx].index >= 0) {
            Player_loadout.ship_pool[ship_list[idx].index] = count;
        }
    }

    // weapon pool
    list_size = weapon_list.size ();
    for (idx = 0; idx < list_size; idx++) {
        count = cfread_int (cfp);

        if (weapon_list[idx].index >= 0) {
            Player_loadout.weapon_pool[weapon_list[idx].index] = count;
        }
    }

    // player ship loadout
    list_size = (uint)cfread_ushort (cfp);
    for (uint i = 0; i < list_size; i++) {
        wss_unit* slot = NULL;

        if (i < MAX_WSS_SLOTS) { slot = &Player_loadout.unit_data[i]; }

        // ship
        ship_idx = cfread_int (cfp);

        if ((ship_idx >= (int)ship_list.size ()) ||
            (ship_idx < -1)) { // on the casts, assume that ship & weapon lists
                               // will never exceed ~2 billion
            WARNINGF (LOCATION,"CSG => Parse Warning: Invalid value for ship index (%d), emptying slot.",ship_idx);
            ship_idx = -1;
        }

        if (slot) {
            if (ship_idx == -1) { // -1 means no ship in this slot
                slot->ship_class = -1;
            }
            else {
                slot->ship_class = ship_list[ship_idx].index;
            }
        }

        // primary weapons
        count = cfread_int (cfp);

        for (j = 0; j < count; j++) {
            wep_idx = cfread_int (cfp);

            if ((wep_idx >= (int)weapon_list.size ()) || (wep_idx < -1)) {
                WARNINGF (LOCATION,"CSG => Parse Warning: Invalid value for primary weapon index (%d), emptying slot.",wep_idx);
                wep_idx = -1;
            }

            if (slot && (j < MAX_SHIP_PRIMARY_BANKS)) {
                if (wep_idx == -1) { // -1 means no weapon in this slot
                    slot->wep[j] = -1;
                }
                else {
                    slot->wep[j] = weapon_list[wep_idx].index;
                }
            }

            int read_idx = cfread_int (cfp);

            if (slot && (j < MAX_SHIP_PRIMARY_BANKS)) {
                slot->wep_count[j] = read_idx;
            }
        }

        // secondary weapons
        count = cfread_int (cfp);

        for (j = 0; j < count; j++) {
            wep_idx = cfread_int (cfp);

            if ((wep_idx >= (int)weapon_list.size ()) || (wep_idx < -1)) {
                WARNINGF (LOCATION,"CSG => Parse Warning: Invalid value for secondary weapon index (%d), emptying slot.",wep_idx);
                wep_idx = -1;
            }

            if (slot && (j < MAX_SHIP_SECONDARY_BANKS)) {
                if (wep_idx == -1) { // -1 means no weapon in this slot
                    slot->wep[j + MAX_SHIP_PRIMARY_BANKS] = -1;
                }
                else {
                    slot->wep[j + MAX_SHIP_PRIMARY_BANKS] =
                        weapon_list[wep_idx].index;
                }
            }

            int read_idx = cfread_int (cfp);

            if (slot && (j < MAX_SHIP_SECONDARY_BANKS)) {
                slot->wep_count[j + MAX_SHIP_PRIMARY_BANKS] = read_idx;
            }
        }
    }
}

void pilotfile::csg_write_loadout () {
    int idx, j;

    startSection (Section::Loadout);

    // base info
    cfwrite_string_len (Player_loadout.filename, cfp);
    cfwrite_string_len (Player_loadout.last_modified, cfp);

    // ship pool
    for (idx = 0; idx < static_cast< int > (Ship_info.size ()); idx++) {
        cfwrite_int (Player_loadout.ship_pool[idx], cfp);
    }

    // weapon pool
    for (idx = 0; idx < Num_weapon_types; idx++) {
        cfwrite_int (Player_loadout.weapon_pool[idx], cfp);
    }

    // play ship loadout
    cfwrite_ushort (MAX_WSS_SLOTS, cfp);

    for (idx = 0; idx < MAX_WSS_SLOTS; idx++) {
        wss_unit* slot = &Player_loadout.unit_data[idx];

        // ship
        cfwrite_int (slot->ship_class, cfp);

        // primary weapons
        cfwrite_int (MAX_SHIP_PRIMARY_BANKS, cfp);

        for (j = 0; j < MAX_SHIP_PRIMARY_BANKS; j++) {
            cfwrite_int (slot->wep[j], cfp);
            cfwrite_int (slot->wep_count[j], cfp);
        }

        // secondary weapons
        cfwrite_int (MAX_SHIP_SECONDARY_BANKS, cfp);

        for (j = 0; j < MAX_SHIP_SECONDARY_BANKS; j++) {
            cfwrite_int (slot->wep[j + MAX_SHIP_PRIMARY_BANKS], cfp);
            cfwrite_int (slot->wep_count[j + MAX_SHIP_PRIMARY_BANKS], cfp);
        }
    }

    endSection ();
}

void pilotfile::csg_read_stats () {
    int idx, list_size = 0;
    int count;

    if (!m_have_info) { throw "Stats before Info!"; }

    // scoring stats
    p->stats.score = cfread_int (cfp);
    p->stats.rank = cfread_int (cfp);
    p->stats.assists = cfread_int (cfp);
    p->stats.kill_count = cfread_int (cfp);
    p->stats.kill_count_ok = cfread_int (cfp);
    p->stats.bonehead_kills = cfread_int (cfp);

    p->stats.p_shots_fired = cfread_uint (cfp);
    p->stats.p_shots_hit = cfread_uint (cfp);
    p->stats.p_bonehead_hits = cfread_uint (cfp);

    p->stats.s_shots_fired = cfread_uint (cfp);
    p->stats.s_shots_hit = cfread_uint (cfp);
    p->stats.s_bonehead_hits = cfread_uint (cfp);

    p->stats.flight_time = cfread_uint (cfp);
    p->stats.missions_flown = cfread_uint (cfp);
    p->stats.last_flown = (_fs_time_t)cfread_int (cfp);
    p->stats.last_backup = (_fs_time_t)cfread_int (cfp);

    // ship kills (scoring)
    list_size = (int)ship_list.size ();
    for (idx = 0; idx < list_size; idx++) {
        count = cfread_int (cfp);

        if (ship_list[idx].index >= 0) {
            p->stats.kills[ship_list[idx].index] = count;
        }
    }

    // medals earned (scoring)
    list_size = (int)medals_list.size ();
    for (idx = 0; idx < list_size; idx++) {
        count = cfread_int (cfp);

        if (medals_list[idx].index >= 0) {
            p->stats.medal_counts[medals_list[idx].index] = count;
        }
    }
}

void pilotfile::csg_write_stats () {
    int idx;

    startSection (Section::Scoring);

    // scoring stats
    cfwrite_int (p->stats.score, cfp);
    cfwrite_int (p->stats.rank, cfp);
    cfwrite_int (p->stats.assists, cfp);
    cfwrite_int (p->stats.kill_count, cfp);
    cfwrite_int (p->stats.kill_count_ok, cfp);
    cfwrite_int (p->stats.bonehead_kills, cfp);

    cfwrite_uint (p->stats.p_shots_fired, cfp);
    cfwrite_uint (p->stats.p_shots_hit, cfp);
    cfwrite_uint (p->stats.p_bonehead_hits, cfp);

    cfwrite_uint (p->stats.s_shots_fired, cfp);
    cfwrite_uint (p->stats.s_shots_hit, cfp);
    cfwrite_uint (p->stats.s_bonehead_hits, cfp);

    cfwrite_uint (p->stats.flight_time, cfp);
    cfwrite_uint (p->stats.missions_flown, cfp);
    cfwrite_int ((int)p->stats.last_flown, cfp);
    cfwrite_int ((int)p->stats.last_backup, cfp);

    // ship kills (scoring)
    for (idx = 0; idx < static_cast< int > (Ship_info.size ()); idx++) {
        cfwrite_int (p->stats.kills[idx], cfp);
    }

    // medals earned (scoring)
    for (idx = 0; idx < Num_medals; idx++) {
        cfwrite_int (p->stats.medal_counts[idx], cfp);
    }

    endSection ();
}

void pilotfile::csg_read_redalert () {
    int idx, i, j, list_size = 0;
    int count;
    char t_string[MAX_FILENAME_LEN + NAME_LENGTH + 1] = { '\0' };
    float hit;
    wep_t weapons;

    if (!m_have_info) { throw "RedAlert before Info!"; }

    list_size = cfread_int (cfp);

    if (list_size <= 0) { return; }

    cfread_string_len (t_string, MAX_FILENAME_LEN, cfp);

    Red_alert_precursor_mission = t_string;

    for (idx = 0; idx < list_size; idx++) {
        red_alert_ship_status ras;

        cfread_string_len (t_string, NAME_LENGTH, cfp);
        ras.name = t_string;

        ras.hull = cfread_float (cfp);

        // ship class, index into ship_list[]
        i = cfread_int (cfp);
        if ((i >= (int)ship_list.size ()) ||
            (i < RED_ALERT_LOWEST_VALID_SHIP_CLASS)) {
            WARNINGF (LOCATION,"CSG => Parse Warning: Invalid value for red alert ship index (%d), emptying slot.",i);
            ras.ship_class = RED_ALERT_DESTROYED_SHIP_CLASS;
        }
        else if (
            (i < 0) &&
            (i >=
             RED_ALERT_LOWEST_VALID_SHIP_CLASS)) { // ship destroyed/exited
            ras.ship_class = i;
        }
        else {
            ras.ship_class = ship_list[i].index;
        }

        // subsystem hits
        count = cfread_int (cfp);

        for (j = 0; j < count; j++) {
            hit = cfread_float (cfp);
            ras.subsys_current_hits.push_back (hit);
        }

        // subsystem aggregate hits
        count = cfread_int (cfp);

        for (j = 0; j < count; j++) {
            hit = cfread_float (cfp);
            ras.subsys_aggregate_current_hits.push_back (hit);
        }

        // primary weapon loadout and status
        count = cfread_int (cfp);

        for (j = 0; j < count; j++) {
            i = cfread_int (cfp);
            weapons.index = weapon_list[i].index;
            weapons.count = cfread_int (cfp);

            // triggering this means something is really fubar
            if (weapons.index < 0) { continue; }

            ras.primary_weapons.push_back (weapons);
        }

        // secondary weapon loadout and status
        count = cfread_int (cfp);

        for (j = 0; j < count; j++) {
            i = cfread_int (cfp);
            weapons.index = weapon_list[i].index;
            weapons.count = cfread_int (cfp);

            // triggering this means something is really fubar
            if (weapons.index < 0) { continue; }

            ras.secondary_weapons.push_back (weapons);
        }

        // this is quite likely a *bad* thing if it doesn't happen
        if (ras.ship_class >= RED_ALERT_LOWEST_VALID_SHIP_CLASS) {
            Red_alert_wingman_status.push_back (ras);
        }
    }
}

void pilotfile::csg_write_redalert () {
    int idx, j, list_size = 0;
    int count;
    red_alert_ship_status* ras;

    startSection (Section::RedAlert);

    list_size = (int)Red_alert_wingman_status.size ();

    cfwrite_int (list_size, cfp);

    if (list_size) {
        cfwrite_string_len (Red_alert_precursor_mission.c_str (), cfp);

        for (idx = 0; idx < list_size; idx++) {
            ras = &Red_alert_wingman_status[idx];

            cfwrite_string_len (ras->name.c_str (), cfp);

            cfwrite_float (ras->hull, cfp);

            // ship class, should be index into ship_list[] on load
            cfwrite_int (ras->ship_class, cfp);

            // subsystem hits
            count = (int)ras->subsys_current_hits.size ();
            cfwrite_int (count, cfp);

            for (j = 0; j < count; j++) {
                cfwrite_float (ras->subsys_current_hits[j], cfp);
            }

            // subsystem aggregate hits
            count = (int)ras->subsys_aggregate_current_hits.size ();
            cfwrite_int (count, cfp);

            for (j = 0; j < count; j++) {
                cfwrite_float (ras->subsys_aggregate_current_hits[j], cfp);
            }

            // primary weapon loadout and status
            count = (int)ras->primary_weapons.size ();
            cfwrite_int (count, cfp);

            for (j = 0; j < count; j++) {
                cfwrite_int (ras->primary_weapons[j].index, cfp);
                cfwrite_int (ras->primary_weapons[j].count, cfp);
            }

            // secondary weapon loadout and status
            count = (int)ras->secondary_weapons.size ();
            cfwrite_int (count, cfp);

            for (j = 0; j < count; j++) {
                cfwrite_int (ras->secondary_weapons[j].index, cfp);
                cfwrite_int (ras->secondary_weapons[j].count, cfp);
            }
        }
    }

    endSection ();
}

void pilotfile::csg_read_hud () {
    int idx;

    // flags
    HUD_config.show_flags = cfread_int (cfp);
    HUD_config.show_flags2 = cfread_int (cfp);

    HUD_config.popup_flags = cfread_int (cfp);
    HUD_config.popup_flags2 = cfread_int (cfp);

    // settings
    HUD_config.num_msg_window_lines = cfread_ubyte (cfp);

    HUD_config.rp_flags = cfread_int (cfp);
    HUD_config.rp_dist = cfread_int (cfp);

    // basic colors
    HUD_config.main_color = cfread_int (cfp);
    HUD_color_alpha = cfread_int (cfp);

    if (HUD_color_alpha < HUD_COLOR_ALPHA_USER_MIN) {
        HUD_color_alpha = HUD_COLOR_ALPHA_DEFAULT;
    }

    hud_config_record_color (HUD_config.main_color);

    // gauge-specific colors
    int num_gauges = cfread_int (cfp);

    for (idx = 0; idx < num_gauges; idx++) {
        ubyte red = cfread_ubyte (cfp);
        ubyte green = cfread_ubyte (cfp);
        ubyte blue = cfread_ubyte (cfp);
        ubyte alpha = cfread_ubyte (cfp);

        if (idx >= NUM_HUD_GAUGES) { continue; }

        HUD_config.clr[idx].red = red;
        HUD_config.clr[idx].green = green;
        HUD_config.clr[idx].blue = blue;
        HUD_config.clr[idx].alpha = alpha;
    }
}

void pilotfile::csg_write_hud () {
    int idx;

    startSection (Section::HUD);

    // flags
    cfwrite_int (HUD_config.show_flags, cfp);
    cfwrite_int (HUD_config.show_flags2, cfp);

    cfwrite_int (HUD_config.popup_flags, cfp);
    cfwrite_int (HUD_config.popup_flags2, cfp);

    // settings
    cfwrite_ubyte (HUD_config.num_msg_window_lines, cfp);

    cfwrite_int (HUD_config.rp_flags, cfp);
    cfwrite_int (HUD_config.rp_dist, cfp);

    // basic colors
    cfwrite_int (HUD_config.main_color, cfp);
    cfwrite_int (HUD_color_alpha, cfp);

    // gauge-specific colors
    cfwrite_int (NUM_HUD_GAUGES, cfp);

    for (idx = 0; idx < NUM_HUD_GAUGES; idx++) {
        cfwrite_ubyte (HUD_config.clr[idx].red, cfp);
        cfwrite_ubyte (HUD_config.clr[idx].green, cfp);
        cfwrite_ubyte (HUD_config.clr[idx].blue, cfp);
        cfwrite_ubyte (HUD_config.clr[idx].alpha, cfp);
    }

    endSection ();
}

void pilotfile::csg_read_variables () {
    int idx;

    int num_variables = cfread_int (cfp);

    if (num_variables > 0) {
        for (idx = 0; idx < num_variables; idx++) {
            sexp_variable temp_var;
            memset (&temp_var, 0, sizeof (sexp_variable));
            temp_var.type = cfread_int (cfp);
            cfread_string_len (temp_var.text, TOKEN_LENGTH, cfp);
            cfread_string_len (temp_var.variable_name, TOKEN_LENGTH, cfp);
            Campaign.persistent_variables.push_back (temp_var);
        }
    }

    Campaign.red_alert_variables.clear ();
    if (csg_ver < 4) { // CSG files before version 4 don't have a Red Alert set
                       // of CPVs to load, so just copy the regular set.
        for (auto& current_pv : Campaign.persistent_variables) {
            Campaign.red_alert_variables.push_back (current_pv);
        }
    }
    else {
        int redalert_num_variables = cfread_int (cfp);

        if (redalert_num_variables > 0) {
            for (idx = 0; idx < redalert_num_variables; idx++) {
                sexp_variable temp_var;
                memset (&temp_var, 0, sizeof (sexp_variable));
                temp_var.type = cfread_int (cfp);
                cfread_string_len (temp_var.text, TOKEN_LENGTH, cfp);
                cfread_string_len (temp_var.variable_name, TOKEN_LENGTH, cfp);
                Campaign.red_alert_variables.push_back (temp_var);
            }
        }
    }
}

void pilotfile::csg_write_variables () {
    int idx;

    startSection (Section::Variables);

    cfwrite_int ((int)Campaign.persistent_variables.size (), cfp);

    for (idx = 0; idx < (int)Campaign.persistent_variables.size (); idx++) {
        if (!(Campaign.persistent_variables[idx].type &
              SEXP_VARIABLE_SAVE_TO_PLAYER_FILE)) {
            cfwrite_int (Campaign.persistent_variables[idx].type, cfp);
            cfwrite_string_len (Campaign.persistent_variables[idx].text, cfp);
            cfwrite_string_len (
                Campaign.persistent_variables[idx].variable_name, cfp);
        }
    }

    cfwrite_int ((int)Campaign.red_alert_variables.size (), cfp);

    for (idx = 0; idx < (int)Campaign.red_alert_variables.size (); idx++) {
        cfwrite_int (Campaign.red_alert_variables[idx].type, cfp);
        cfwrite_string_len (Campaign.red_alert_variables[idx].text, cfp);
        cfwrite_string_len (
            Campaign.red_alert_variables[idx].variable_name, cfp);
    }

    endSection ();
}

void pilotfile::csg_read_settings () {
    // sound/voice/music
    Master_sound_volume = cfread_float (cfp);
    Master_event_music_volume = cfread_float (cfp);
    Master_voice_volume = cfread_float (cfp);

    audiostream_set_volume_all (Master_voice_volume, ASF_VOICE);
    audiostream_set_volume_all (Master_event_music_volume, ASF_EVENTMUSIC);

    if (Master_event_music_volume > 0.0f) { Event_music_enabled = 1; }
    else {
        Event_music_enabled = 0;
    }

    Briefing_voice_enabled = cfread_int (cfp);

    // skill level
    Game_skill_level = cfread_int (cfp);

    // input options
    Use_mouse_to_fly = cfread_int (cfp);
    Mouse_sensitivity = cfread_int (cfp);
    Joy_sensitivity = cfread_int (cfp);
    Joy_dead_zone_size = cfread_int (cfp);

    if (csg_ver < 3) {
        // detail
        int dummy = cfread_int (cfp);
        FS2_UNUSED (dummy);
        dummy = cfread_int (cfp);
        dummy = cfread_int (cfp);
        dummy = cfread_int (cfp);
        dummy = cfread_int (cfp);
        dummy = cfread_int (cfp);
        dummy = cfread_int (cfp);
        dummy = cfread_int (cfp);
        dummy = cfread_int (cfp);
        dummy = cfread_int (cfp);
        dummy = cfread_int (cfp);
        dummy = cfread_int (cfp);
    }
}

void pilotfile::csg_write_settings () {
    startSection (Section::Settings);

    // sound/voice/music
    cfwrite_float (Master_sound_volume, cfp);
    cfwrite_float (Master_event_music_volume, cfp);
    cfwrite_float (Master_voice_volume, cfp);

    cfwrite_int (Briefing_voice_enabled, cfp);

    // skill level
    cfwrite_int (Game_skill_level, cfp);

    // input options
    cfwrite_int (Use_mouse_to_fly, cfp);
    cfwrite_int (Mouse_sensitivity, cfp);
    cfwrite_int (Joy_sensitivity, cfp);
    cfwrite_int (Joy_dead_zone_size, cfp);

    endSection ();
}

void pilotfile::csg_read_controls () {
    int idx, list_size;
    short id1, id2, id3;
    FS2_UNUSED (id3);

    list_size = (int)cfread_ushort (cfp);

    for (idx = 0; idx < list_size; idx++) {
        id1 = cfread_short (cfp);
        id2 = cfread_short (cfp);
        id3 = cfread_short (cfp); // unused, at the moment

        if (idx < CCFG_MAX) {
            Control_config[idx].key_id = id1;
            Control_config[idx].joy_id = id2;
        }
    }
}

void pilotfile::csg_write_controls () {
    int idx;

    startSection (Section::Controls);

    cfwrite_ushort (CCFG_MAX, cfp);

    for (idx = 0; idx < CCFG_MAX; idx++) {
        cfwrite_short (Control_config[idx].key_id, cfp);
        cfwrite_short (Control_config[idx].joy_id, cfp);
        // placeholder? for future mouse_id?
        cfwrite_short (-1, cfp);
    }

    endSection ();
}

void pilotfile::csg_read_cutscenes () {
    size_t list_size = cfread_uint (cfp);

    for (size_t i = 0; i < list_size; i++) {
        char tempFilename[MAX_FILENAME_LEN];

        cfread_string_len (tempFilename, MAX_FILENAME_LEN, cfp);
        cutscene_mark_viewable (tempFilename);
    }
}

void pilotfile::csg_write_cutscenes () {
    std::vector< cutscene_info >::iterator cut;

    startSection (Section::Cutscenes);

    size_t viewableScenes = 0;
    for (cut = Cutscenes.begin (); cut != Cutscenes.end (); ++cut) {
        if (cut->viewable) viewableScenes++;
    }

    // Check for possible overflow because we can only write 32 bit integers
    ASSERTX (
        viewableScenes <= std::numeric_limits< uint >::max (),
        "Too many viewable cutscenes! Maximum is %ud!",
        std::numeric_limits< uint >::max ());

    cfwrite_uint ((uint)viewableScenes, cfp);

    for (cut = Cutscenes.begin (); cut != Cutscenes.end (); ++cut) {
        if (cut->viewable) cfwrite_string_len (cut->filename, cfp);
    }

    endSection ();
}

/*
 * Only used for quick start missions
 */
void pilotfile::csg_read_lastmissions () {
    int i;

    // restore list of most recently played missions
    Num_recent_missions = cfread_int (cfp);
    ASSERT (Num_recent_missions <= MAX_RECENT_MISSIONS);
    for (i = 0; i < Num_recent_missions; i++) {
        char* cp;

        cfread_string_len (Recent_missions[i], MAX_FILENAME_LEN, cfp);
        // Remove the extension (safety check: shouldn't exist anyway)
        cp = strchr (Recent_missions[i], '.');
        if (cp) *cp = 0;
    }
}

/*
 * Only used for quick start missions
 */

void pilotfile::csg_write_lastmissions () {
    int i;

    startSection (Section::LastMissions);

    // store list of most recently played missions
    cfwrite_int (Num_recent_missions, cfp);
    for (i = 0; i < Num_recent_missions; i++) {
        cfwrite_string_len (Recent_missions[i], cfp);
    }

    endSection ();
}

void pilotfile::csg_reset_data () {
    int idx;
    cmission* missionp;

    // internals
    m_have_flags = false;
    m_have_info = false;

    m_data_invalid = false;

    // init stats
    p->stats.init ();

    // zero out allowed ships/weapons
    memset (Campaign.ships_allowed, 0, sizeof (Campaign.ships_allowed));
    memset (Campaign.weapons_allowed, 0, sizeof (Campaign.weapons_allowed));

    // reset campaign status
    Campaign.prev_mission = -1;
    Campaign.next_mission = -1;
    Campaign.num_missions_completed = 0;

    // techroom reset
    tech_reset_to_default ();

    // clear out variables
    Campaign.persistent_variables.clear ();
    Campaign.red_alert_variables.clear ();

    // clear red alert data
    Red_alert_wingman_status.clear ();

    // clear out mission stuff
    for (idx = 0; idx < MAX_CAMPAIGN_MISSIONS; idx++) {
        missionp = &Campaign.missions[idx];

        if (missionp->goals) {
            missionp->num_goals = 0;
            free (missionp->goals);
            missionp->goals = NULL;
        }

        if (missionp->events) {
            missionp->num_events = 0;
            free (missionp->events);
            missionp->events = NULL;
        }

        if (missionp->variables) {
            missionp->num_variables = 0;
            free (missionp->variables);
            missionp->variables = NULL;
        }

        missionp->stats.init ();
    }
}

void pilotfile::csg_close () {
    if (cfp) {
        cfclose (cfp);
        cfp = NULL;
    }

    p = NULL;
    filename = "";

    ship_list.clear ();
    weapon_list.clear ();
    intel_list.clear ();
    medals_list.clear ();

    m_have_flags = false;
    m_have_info = false;
}

bool pilotfile::load_savefile (const char* campaign) {
    if (0 == campaign || 0 == campaign [0])
        return false;

    ASSERT (0 <= Player_num && Player_num < MAX_PLAYERS);
    p = &Players[Player_num];
    ASSERT (p);

    //
    // Create filename:
    //
    auto stem = fs::path (campaign).stem ();
    filename = ((fs::path (p->callsign) += ".") += stem) += ".csg";

    auto campaign_file = fs::path (stem) += FS_CAMPAIGN_FILE_EXT;

    if (!cf_exists_full (campaign_file.c_str (), CF_TYPE_MISSIONS)) {
        WARNINGF (LOCATION, "CSG => Unable to find campaign file '%s'!",campaign_file.c_str ());
        return false;
    }

    // we need to reset this early, in case open fails and we need to create
    m_data_invalid = false;

    // open it, hopefully...
    cfp = cfopen (
        filename.c_str (), "rb", CFILE_NORMAL, CF_TYPE_PLAYERS, false,
        CF_LOCATION_ROOT_USER | CF_LOCATION_ROOT_GAME | CF_LOCATION_TYPE_ROOT);

    if (!cfp) {
        WARNINGF (LOCATION, "CSG => Unable to open '%s' for reading!",filename.c_str ());
        return false;
    }

    unsigned int csg_id = cfread_uint (cfp);

    if (csg_id != CSG_FILE_ID) {
        WARNINGF (LOCATION, "CSG => Invalid header id for '%s'!",filename.c_str ());
        csg_close ();
        return false;
    }

    // version, now used
    csg_ver = cfread_ubyte (cfp);

    WARNINGF (LOCATION, "CSG => Loading '%s' with version %d...",filename.c_str (), (int)csg_ver);

    csg_reset_data ();

    // the point of all this: read in the CSG contents
    while (!cfeof (cfp)) {
        ushort section_id = cfread_ushort (cfp);
        uint section_size = cfread_uint (cfp);

        size_t start_pos = cftell (cfp);

        // safety, to help protect against long reads
        cf_set_max_read_len (cfp, section_size);

        try {
            switch (section_id) {
            case Section::Flags:
                WARNINGF (LOCATION, "CSG => Parsing:  Flags...");
                m_have_flags = true;
                csg_read_flags ();
                break;

            case Section::Info:
                WARNINGF (LOCATION, "CSG => Parsing:  Info...");
                m_have_info = true;
                csg_read_info ();
                break;

            case Section::Variables:
                WARNINGF (LOCATION, "CSG => Parsing:  Variables...");
                csg_read_variables ();
                break;

            case Section::HUD:
                WARNINGF (LOCATION, "CSG => Parsing:  HUD...");
                csg_read_hud ();
                break;

            case Section::RedAlert:
                WARNINGF (LOCATION, "CSG => Parsing:  RedAlert...");
                csg_read_redalert ();
                break;

            case Section::Scoring:
                WARNINGF (LOCATION, "CSG => Parsing:  Scoring...");
                csg_read_stats ();
                break;

            case Section::Loadout:
                WARNINGF (LOCATION, "CSG => Parsing:  Loadout...");
                csg_read_loadout ();
                break;

            case Section::Techroom:
                WARNINGF (LOCATION, "CSG => Parsing:  Techroom...");
                csg_read_techroom ();
                break;

            case Section::Missions:
                WARNINGF (LOCATION, "CSG => Parsing:  Missions...");
                csg_read_missions ();
                break;

            case Section::Settings:
                WARNINGF (LOCATION, "CSG => Parsing:  Settings...");
                csg_read_settings ();
                break;

            case Section::Controls:
                WARNINGF (LOCATION, "CSG => Parsing:  Controls...");
                csg_read_controls ();
                break;

            case Section::Cutscenes:
                WARNINGF (LOCATION, "CSG => Parsing:  Cutscenes...");
                csg_read_cutscenes ();
                break;

            case Section::LastMissions:
                WARNINGF (LOCATION, "CSG => Parsing:  Last Missions...");
                csg_read_lastmissions ();
                break;

            default:
                WARNINGF (LOCATION, "CSG => Skipping unknown section 0x%04x!",(uint32_t)section_id);
                break;
            }
        }
        catch (cfile::max_read_length& msg) {
            // read to max section size, move to next section, discarding
            // extra/unknown data
            WARNINGF (LOCATION, "CSG => Warning: (0x%04x) %s",(uint32_t)section_id, msg.what ());
        }
        catch (const char* err) {
            ERRORF (LOCATION, "CSG => ERROR: %s\n", err);
            csg_close ();
            return false;
        }

        // reset safety catch
        cf_set_max_read_len (cfp, 0);

        // skip to next section (if not already there)
        size_t offset_pos = (start_pos + section_size) - cftell (cfp);

        if (offset_pos) {
            WARNINGF (LOCATION,"CSG => Warning: (0x%04x) Short read, information may have been lost!",(uint32_t)section_id);
            cfseek (cfp, (int)offset_pos, CF_SEEK_CUR);
        }
    }

    // if the campaign (for whatever reason) doesn't have a squad image, use
    // the multi one
    if (p->s_squad_filename[0] == '\0') {
        strcpy (p->s_squad_filename, p->m_squad_filename);
    }
    player_set_squad_bitmap (p, p->s_squad_filename, false);

    WARNINGF (LOCATION, "CSG => Loading complete!");

    // cleanup and return
    csg_close ();

    return true;
}

bool pilotfile::save_savefile () {
    // set player ptr first thing
    ASSERT (0 <= Player_num && Player_num < MAX_PLAYERS);
    p = &Players[Player_num];
    ASSERT (p);

    if (0 == Campaign.filename [0]) {
        return false;
    }

    const auto basename = fs::path (Campaign.filename).stem ();
    const auto filename = (
        (fs::path (p->callsign) += ".") += basename) += ".csg";

    // make sure that we can actually save this safely
    if (m_data_invalid) {
        WARNINGF (LOCATION,"CSG => Skipping save of '%s' due to invalid data check!",filename.c_str ());
        return false;
    }

    // validate the number of red alert entries
    // assertion before writing so that we don't corrupt the .csg by asserting
    // halfway through writing assertion should also prevent loss of major
    // campaign progress i.e. lose one mission, not several missions worth (in
    // theory)
    ASSERTX (
        Red_alert_wingman_status.size () <= MAX_SHIPS,
        "Invalid number of Red_alert_wingman_status entries: %zu\n",
        Red_alert_wingman_status.size ());

    // open it, hopefully...
    cfp = cfopen (
        filename.c_str (), "wb", CFILE_NORMAL, CF_TYPE_PLAYERS, false,
        CF_LOCATION_ROOT_USER | CF_LOCATION_ROOT_GAME | CF_LOCATION_TYPE_ROOT);

    if (!cfp) {
        WARNINGF (LOCATION, "CSG => Unable to open '%s' for saving!",filename.c_str ());
        return false;
    }

    // header and version
    cfwrite_int (CSG_FILE_ID, cfp);
    cfwrite_ubyte (CSG_VERSION, cfp);

    WARNINGF (LOCATION, "CSG => Saving '%s' with version %d...", filename.c_str (),(int)CSG_VERSION);

    // flags and info sections go first
    csg_write_flags ();
    csg_write_info ();

    // everything else is next, not order specific
    csg_write_missions ();
    csg_write_techroom ();
    csg_write_loadout ();
    csg_write_stats ();
    csg_write_redalert ();
    csg_write_hud ();
    csg_write_variables ();
    csg_write_settings ();
    csg_write_controls ();
    csg_write_cutscenes ();
    csg_write_lastmissions ();

    csg_close ();

    return true;
}

/*
 * get_csg_rank: this function is called from plr.cpp & is
 * tightly linked with pilotfile::verify()
 */
bool pilotfile::get_csg_rank (int* rank) {
    player t_csg;

    // set player ptr first thing
    p = &t_csg;

    // filename has already been set
    cfp = cfopen (
        filename.c_str (), "rb", CFILE_NORMAL, CF_TYPE_PLAYERS, false,
        CF_LOCATION_ROOT_USER | CF_LOCATION_ROOT_GAME | CF_LOCATION_TYPE_ROOT);

    if (!cfp) {
        WARNINGF (LOCATION, "CSG => Unable to open '%s'!", filename.c_str ());
        return false;
    }

    unsigned int csg_id = cfread_uint (cfp);

    if (csg_id != CSG_FILE_ID) {
        WARNINGF (LOCATION, "CSG => Invalid header id for '%s'!",filename.c_str ());
        csg_close ();
        return false;
    }

    // version, now used
    csg_ver = cfread_ubyte (cfp);

    WARNINGF (LOCATION, "CSG => Get Rank from '%s' with version %d...",filename.c_str (), (int)csg_ver);

    // the point of all this: read in the CSG contents
    while (!m_have_flags && !cfeof (cfp)) {
        ushort section_id = cfread_ushort (cfp);
        uint section_size = cfread_uint (cfp);

        size_t start_pos = cftell (cfp);
        size_t offset_pos;

        // safety, to help protect against long reads
        cf_set_max_read_len (cfp, section_size);

        try {
            switch (section_id) {
            case Section::Flags:
                WARNINGF (LOCATION, "CSG => Parsing:  Flags...");
                m_have_flags = true;
                csg_read_flags ();
                break;

            default: break;
            }
        }
        catch (cfile::max_read_length& msg) {
            // read to max section size, move to next section, discarding
            // extra/unknown data
            WARNINGF (LOCATION, "CSG => (0x%04x) %s", (uint32_t)section_id,msg.what ());
        }
        catch (const char* err) {
            ERRORF (LOCATION, "CSG => ERROR: %s\n", err);
            csg_close ();
            return false;
        }

        // reset safety catch
        cf_set_max_read_len (cfp, 0);

        // skip to next section (if not already there)
        offset_pos = (start_pos + section_size) - cftell (cfp);

        if (offset_pos) {
            WARNINGF (LOCATION,"CSG => Warning: (0x%04x) Short read, information may have been lost!",(uint32_t)section_id);
            cfseek (cfp, (int)offset_pos, CF_SEEK_CUR);
        }
    }

    // this is what we came for...
    *rank = p->stats.rank;

    WARNINGF (LOCATION, "CSG => Get Rank complete!");

    // cleanup & return
    csg_close ();

    return true;
}
// -*- mode: c++; -*-

#include "defs.hh"
#include "ai/ai_profiles.hh"
#include "debugconsole/console.hh"
#include "freespace2/freespace.hh"
#include "hud/hud.hh"
#include "hud/hudmessage.hh"
#include "iff_defs/iff_defs.hh"
#include "localization/localize.hh"
#include "mission/missionparse.hh"
#include "object/object.hh"
#include "parse/parselo.hh"
#include "pilotfile/pilotfile.hh"
#include "playerman/player.hh"
#include "ship/ship.hh"
#include "stats/medals.hh"
#include "stats/scoring.hh"
#include "weapon/weapon.hh"
#include "log/log.hh"

/*
// uncomment to get extra debug messages when a player scores
#define SCORING_DEBUG
*/
// what percent of points of total damage to a ship a player has to have done
// to get an assist (or a kill) when it is killed
float Kill_percentage = 0.30f;
float Assist_percentage = 0.15f;

// these tables are overwritten with the values from rank.tbl
rank_stuff Ranks[NUM_RANKS];

// scoring scale factors by skill level
float Scoring_scale_factors[NUM_SKILL_LEVELS] = {
    0.2f, // very easy
    0.4f, // easy
    0.7f, // medium
    1.0f, // hard
    1.25f // insane
};

void scoreing_close ();

void parse_rank_tbl () {
    atexit (scoreing_close);
    char buf[MULTITEXT_LENGTH];
    int idx, persona;

    try {
        read_file_text ("rank.tbl", CF_TYPE_TABLES);
        reset_parse ();

        // parse in all the rank names
        idx = 0;
        skip_to_string ("[RANK NAMES]");
        ignore_white_space ();
        while (required_string_either ("#End", "$Name:")) {
            ASSERT (idx < NUM_RANKS);
            required_string ("$Name:");
            stuff_string (Ranks[idx].name, F_NAME, NAME_LENGTH);
            required_string ("$Points:");
            stuff_int (&Ranks[idx].points);
            required_string ("$Bitmap:");
            stuff_string (Ranks[idx].bitmap, F_NAME, MAX_FILENAME_LEN);
            required_string ("$Promotion Voice Base:");
            stuff_string (
                Ranks[idx].promotion_voice_base, F_NAME, MAX_FILENAME_LEN);
            while (check_for_string ("$Promotion Text:")) {
                required_string ("$Promotion Text:");
                stuff_string (buf, F_MULTITEXT, sizeof (buf));
                drop_white_space (buf);
                compact_multitext_string (buf);
                persona = -1;
                if (optional_string ("+Persona:")) {
                    stuff_int (&persona);
                    if (persona < 0) {
                        WARNINGF (LOCATION,"Debriefing text for %s rank is assigned to an invalid persona: %i (must be 0 or greater).",Ranks[idx].name, persona);
                        continue;
                    }
                }
                Ranks[idx].promotion_text[persona] = std::string (buf);
            }
            if (Ranks[idx].promotion_text.find (-1) ==
                Ranks[idx].promotion_text.end ()) {
                WARNINGF (LOCATION, "%s rank is missing default debriefing text.",Ranks[idx].name);
                Ranks[idx].promotion_text[-1] = "";
            }
            idx++;
        }

        required_string ("#End");

        // be sure that all rank points are in order
        for (idx = 0; idx < NUM_RANKS - 1; idx++) {
            if (Ranks[idx].points >= Ranks[idx + 1].points)
                WARNINGF (LOCATION,"Rank #%d (%s) has a higher \"$Points:\" value (%d) than the following rank (%s, %d points). This shouldn't actually crash FSO, but it might result in unexpected or incorrect behavior.",idx + 1, Ranks[idx].name, Ranks[idx].points,Ranks[idx + 1].name, Ranks[idx + 1].points);
        }
    }
    catch (const parse::ParseException& e) {
        ERRORF (
            LOCATION, "parse failed '%s'!  Error message = %s.\n", "rank.tbl",
            e.what ());
        return;
    }
}

// initialize a nice blank scoring element
void scoring_struct::init () {
    flags = 0;
    score = 0;
    rank = RANK_ENSIGN;

    medal_counts.assign (Num_medals, 0);

    memset (kills, 0, MAX_SHIP_CLASSES * sizeof (int));
    assists = 0;
    kill_count = 0;
    kill_count_ok = 0;
    p_shots_fired = 0;
    s_shots_fired = 0;

    p_shots_hit = 0;
    s_shots_hit = 0;

    p_bonehead_hits = 0;
    s_bonehead_hits = 0;
    bonehead_kills = 0;

    missions_flown = 0;
    flight_time = 0;
    last_flown = 0;
    last_backup = 0;

    m_medal_earned = -1; // hasn't earned a medal yet
    m_promotion_earned = -1;
    m_badge_earned.clear ();

    m_score = 0;
    memset (m_kills, 0, MAX_SHIP_CLASSES * sizeof (int));
    memset (m_okKills, 0, MAX_SHIP_CLASSES * sizeof (int));
    m_kill_count = 0;
    m_kill_count_ok = 0;
    m_assists = 0;
    mp_shots_fired = 0;
    ms_shots_fired = 0;
    mp_shots_hit = 0;
    ms_shots_hit = 0;
    mp_bonehead_hits = 0;
    ms_bonehead_hits = 0;
    m_bonehead_kills = 0;
    m_player_deaths = 0;

    memset (m_dogfight_kills, 0, MAX_PLAYERS * sizeof (int));
}

// clone someone else's scoring element
void scoring_struct::assign (const scoring_struct& s) {
    flags = s.flags;
    score = s.score;
    rank = s.rank;

    medal_counts.assign (s.medal_counts.begin (), s.medal_counts.end ());

    memcpy (kills, s.kills, MAX_SHIP_CLASSES * sizeof (int));
    assists = s.assists;
    kill_count = s.kill_count;
    kill_count_ok = s.kill_count_ok;
    p_shots_fired = s.p_shots_fired;
    s_shots_fired = s.s_shots_fired;

    p_shots_hit = s.p_shots_hit;
    s_shots_hit = s.s_shots_hit;

    p_bonehead_hits = s.p_bonehead_hits;
    s_bonehead_hits = s.s_bonehead_hits;
    bonehead_kills = s.bonehead_kills;

    missions_flown = s.missions_flown;
    flight_time = s.flight_time;
    last_flown = s.last_flown;
    last_backup = s.last_backup;

    m_medal_earned = s.m_medal_earned;
    m_promotion_earned = s.m_promotion_earned;
    m_badge_earned = s.m_badge_earned;

    m_score = s.m_score;
    memcpy (m_kills, s.m_kills, MAX_SHIP_CLASSES * sizeof (int));
    memcpy (m_okKills, s.m_okKills, MAX_SHIP_CLASSES * sizeof (int));
    m_kill_count = s.m_kill_count;
    m_kill_count_ok = s.m_kill_count_ok;
    m_assists = s.m_assists;
    mp_shots_fired = s.mp_shots_fired;
    ms_shots_fired = s.ms_shots_fired;
    mp_shots_hit = s.mp_shots_hit;
    ms_shots_hit = s.ms_shots_hit;
    mp_bonehead_hits = s.mp_bonehead_hits;
    ms_bonehead_hits = s.ms_bonehead_hits;
    m_bonehead_kills = s.m_bonehead_kills;
    m_player_deaths = s.m_player_deaths;

    memcpy (m_dogfight_kills, s.m_dogfight_kills, MAX_PLAYERS * sizeof (int));
}

// initialize the Player's mission-based stats before he goes into a mission
void scoring_level_init (scoring_struct* scp) {
    scp->m_medal_earned = -1; // hasn't earned a medal yet
    scp->m_promotion_earned = -1;
    scp->m_badge_earned.clear ();
    scp->m_score = 0;
    scp->m_assists = 0;
    scp->mp_shots_fired = 0;
    scp->mp_shots_hit = 0;
    scp->ms_shots_fired = 0;
    scp->ms_shots_hit = 0;

    scp->mp_bonehead_hits = 0;
    scp->ms_bonehead_hits = 0;
    scp->m_bonehead_kills = 0;

    memset (scp->m_kills, 0, MAX_SHIP_CLASSES * sizeof (int));
    memset (scp->m_okKills, 0, MAX_SHIP_CLASSES * sizeof (int));

    scp->m_kill_count = 0;
    scp->m_kill_count_ok = 0;

    scp->m_player_deaths = 0;

    memset (scp->m_dogfight_kills, 0, MAX_PLAYERS * sizeof (int));

    if (The_mission.ai_profile != NULL) {
        Kill_percentage =
            The_mission.ai_profile->kill_percentage_scale[Game_skill_level];
        Assist_percentage =
            The_mission.ai_profile->assist_percentage_scale[Game_skill_level];
    }
    else {
        Kill_percentage = 0.30f;
        Assist_percentage = 0.15f;
    }
}

void scoring_eval_rank (scoring_struct* sc) {
    int i, score, new_rank, old_rank;

    old_rank = sc->rank;
    new_rank = old_rank;

    // first check to see if the promotion flag is set -- if so, return the new
    // rank
    if (Player->flags & PLAYER_FLAGS_PROMOTED) {
        // if the player does indeed get promoted, we should change his mission
        // score to reflect the difference between all time and new rank score
        if (old_rank < MAX_FREESPACE2_RANK) {
            new_rank++;
            if ((sc->m_score + sc->score) < Ranks[new_rank].points)
                sc->m_score = (Ranks[new_rank].points - sc->score);
        }
    }
    else {
        // we get here only if player wasn't promoted automatically.
        // it is possible to get a negative mission score but that will
        // never result in a degradation
        score = sc->m_score + sc->score;
        for (i = old_rank + 1; i < NUM_RANKS; i++) {
            if (score >= Ranks[i].points) new_rank = i;
        }
    }

    // if the ranks do not match, then "grant" the new rank
    if (old_rank != new_rank) {
        ASSERT (new_rank >= 0);
        sc->m_promotion_earned = new_rank;
        sc->rank = new_rank;
    }
}

// function to evaluate whether or not a new badge is going to be awarded. This
// function returns which medal is awarded.
void scoring_eval_badges (scoring_struct* sc) {
    int total_kills;

    // to determine badges, we count kills based on fighter/bomber types.  We
    // must count kills in all time stats + current mission stats.  And, only
    // for enemy fighters/bombers
    total_kills = 0;
    for (auto it = Ship_info.cbegin (); it != Ship_info.cend (); ++it) {
        if (it->is_fighter_bomber ()) {
            auto i = std::distance (Ship_info.cbegin (), it);
            total_kills += sc->m_okKills[i];
            total_kills += sc->kills[i];
        }
    }

    // total_kills should now reflect the number of kills on hostile
    // fighters/bombers.  Check this number against badge kill numbers, and
    // award the appropriate badges as neccessary.
    int last_badge_kills = 0;
    for (auto i = 0; i < Num_medals; i++) {
        if (total_kills >= Medals[i].kills_needed &&
            Medals[i].kills_needed > last_badge_kills &&
            Medals[i].kills_needed > 0) {
            last_badge_kills = Medals[i].kills_needed;
            if (sc->medal_counts[i] < 1) {
                sc->medal_counts[i] = 1;
                sc->m_badge_earned.push_back (i);
            }
        }
    }
}

// central point for dealing with accepting the score for a misison.
void scoring_do_accept (scoring_struct* score) {
    int idx;

    // do rank, badges, and medals first since they require the alltime stuff
    // to not be updated yet.

    // do medal stuff
    if (score->m_medal_earned != -1) {
        score->medal_counts[score->m_medal_earned]++;
    }

    // return when in training mission.  We can grant a medal in training, but
    // don't want to calculate any other statistics.
    if (The_mission.game_type == MISSION_TYPE_TRAINING) { return; }

    scoring_eval_rank (score);
    scoring_eval_badges (score);

    score->kill_count += score->m_kill_count;
    score->kill_count_ok += score->m_kill_count_ok;

    score->score += score->m_score;
    score->assists += score->m_assists;
    score->p_shots_fired += score->mp_shots_fired;
    score->s_shots_fired += score->ms_shots_fired;

    score->p_shots_hit += score->mp_shots_hit;
    score->s_shots_hit += score->ms_shots_hit;

    score->p_bonehead_hits += score->mp_bonehead_hits;
    score->s_bonehead_hits += score->ms_bonehead_hits;
    score->bonehead_kills += score->m_bonehead_kills;

    for (idx = 0; idx < MAX_SHIP_CLASSES; idx++) {
        score->kills[idx] = (int)(score->kills[idx] + score->m_okKills[idx]);
    }

    // add in mission time
    score->flight_time += (unsigned int)f2fl (Missiontime);
    score->last_backup = score->last_flown;
    score->last_flown = (_fs_time_t)time (NULL);
    score->missions_flown++;
}

// backout the score for a mission.  This function gets called when the player
// chooses to refly a misison after debriefing
void scoring_backout_accept (scoring_struct* score) {
    int idx;

    // if a badge was earned, take it back
    if (score->m_badge_earned.size ()) {
        for (size_t medal = 0; medal < score->m_badge_earned.size ();
             medal++) {
            score->medal_counts[score->m_badge_earned[medal]] = 0;
        }
    }

    // return when in training mission.  We can grant a medal in training, but
    // don't want to calculate any other statistics.
    if (The_mission.game_type == MISSION_TYPE_TRAINING) { return; }

    score->kill_count -= score->m_kill_count;
    score->kill_count_ok -= score->m_kill_count_ok;

    score->score -= score->m_score;
    score->assists -= score->m_assists;
    score->p_shots_fired -= score->mp_shots_fired;
    score->s_shots_fired -= score->ms_shots_fired;

    score->p_shots_hit -= score->mp_shots_hit;
    score->s_shots_hit -= score->ms_shots_hit;

    score->p_bonehead_hits -= score->mp_bonehead_hits;
    score->s_bonehead_hits -= score->ms_bonehead_hits;
    score->bonehead_kills -= score->m_bonehead_kills;

    for (idx = 0; idx < MAX_SHIP_CLASSES; idx++) {
        score->kills[idx] =
            (unsigned short)(score->kills[idx] - score->m_okKills[idx]);
    }

    // if the player was given a medal, take it back
    if (score->m_medal_earned != -1) {
        score->medal_counts[score->m_medal_earned]--;
        ASSERT (score->medal_counts[score->m_medal_earned] >= 0);
    }

    // if the player was promoted, take it back
    if (score->m_promotion_earned != -1) {
        score->rank--;
        ASSERT (score->rank >= 0);
    }

    score->flight_time -= (unsigned int)f2fl (Missiontime);
    score->last_flown = score->last_backup;
    score->missions_flown--;
}

// merge any mission stats accumulated into the alltime stats (as well as
// updating per campaign stats)
void scoring_level_close (int accepted) {
    // want to calculate any other statistics.
    if (The_mission.game_type == MISSION_TYPE_TRAINING) {
        // call scoring_do_accept
        // this will grant any potential medals and then early bail, and
        // then we will early bail
        scoring_do_accept (&Player->stats);
        Pilot.update_stats (&Player->stats, true);
        return;
    }

    if (accepted) {
        WARNINGF (LOCATION, "Storing stats now");
        scoring_do_accept (&Player->stats);

        // If this mission doesn't allow promotion or badges
        // then be sure that these don't get done.  Don't allow promotions or
        // badges when playing normally and not in a campaign.
        if (The_mission.flags[Mission::Mission_Flags::No_promotion] ||
            ((Game_mode & GM_NORMAL) && !(Game_mode & GM_CAMPAIGN_MODE))) {
            if (Player->stats.m_promotion_earned != -1) {
                Player->stats.rank--;
                Player->stats.m_promotion_earned = -1;
            }

            // if a badge was earned, take it back
            if (Player->stats.m_badge_earned.size ()) {
                for (size_t medal = 0;
                     medal < Player->stats.m_badge_earned.size (); medal++) {
                    Player->stats
                        .medal_counts[Player->stats.m_badge_earned[medal]] = 0;
                }
                Player->stats.m_badge_earned.clear ();
            }
        }

        Pilot.update_stats (&Player->stats);
    }
}

// STATS damage, assists recording stuff
void scoring_add_damage (object* ship_objp, object* other_obj, float damage) {
    int found_slot, signature;
    int lowest_index, idx;
    object* use_obj;
    ship* sp;

    // if we have no other object, bail
    if (other_obj == NULL) { return; }

    // for player kill/assist evaluation, we have to know exactly how much
    // damage really mattered. For example, if a ship had 1 hit point left, and
    // the player hit it with a nuke, it doesn't matter that it did 10,000,000
    // points of damage, only that 1 point would count
    float actual_damage = 0.0f;

    // other_obj might not always be the parent of other_obj (in the case of
    // debug code for sure).  See if the other_obj has a parent, and if so, use
    // the parent.  If no parent, see if other_obj is a ship and if so, use
    // that ship.
    if (other_obj->parent != -1) {
        use_obj = &Objects[other_obj->parent];
        signature = use_obj->signature;
    }
    else {
        signature = other_obj->signature;
        use_obj = other_obj;
    }

    // don't count damage done to a ship by himself
    if (use_obj == ship_objp) { return; }

    // get a pointer to the ship and add the actual amount of damage done to it
    // get the ship object, and determine the _actual_ amount of damage done
    sp = &Ships[ship_objp->instance];
    // see comments at beginning of function
    if (ship_objp->hull_strength < 0.0f) {
        actual_damage = damage + ship_objp->hull_strength;
    }
    else {
        actual_damage = damage;
    }
    if (actual_damage < 0.0f) { actual_damage = 0.0f; }
    sp->total_damage_received += actual_damage;

    // go through and clear out all old damagers
    for (idx = 0; idx < MAX_DAMAGE_SLOTS; idx++) {
        if ((sp->damage_ship_id[idx] >= 0) &&
            (ship_get_by_signature (sp->damage_ship_id[idx]) < 0)) {
            sp->damage_ship_id[idx] = -1;
            sp->damage_ship[idx] = 0;
        }
    }

    // only evaluate possible kill/assist numbers if the hitting object
    // (use_obj) is a piloted ship (ie, ignore asteroids, etc) don't store
    // damage a ship may do to himself
    if ((ship_objp->type == OBJ_SHIP) && (use_obj->type == OBJ_SHIP)) {
        found_slot = 0;
        // try and find an open slot
        for (idx = 0; idx < MAX_DAMAGE_SLOTS; idx++) {
            // if this ship object doesn't exist anymore, use the slot
            if ((sp->damage_ship_id[idx] == -1) ||
                (ship_get_by_signature (sp->damage_ship_id[idx]) < 0) ||
                (sp->damage_ship_id[idx] == signature)) {
                found_slot = 1;
                break;
            }
        }

        // if not found (implying all slots are taken), then find the slot with
        // the lowest damage % and use that
        if (!found_slot) {
            lowest_index = 0;
            for (idx = 0; idx < MAX_DAMAGE_SLOTS; idx++) {
                if (sp->damage_ship[idx] < sp->damage_ship[lowest_index]) {
                    lowest_index = idx;
                }
            }
        }
        else {
            lowest_index = idx;
        }

        // fill in the slot damage and damager-index
        if (found_slot) { sp->damage_ship[lowest_index] += actual_damage; }
        else {
            sp->damage_ship[lowest_index] = actual_damage;
        }
        sp->damage_ship_id[lowest_index] = signature;
    }
}

char Scoring_debug_text[4096];

// evaluate a kill on a ship
int scoring_eval_kill (object* ship_objp) {
    float max_damage_pct; // the pct% of total damage the max damage object did
    int max_damage_index; // the index into the dying ship's damage_ship[]
                          // array corresponding the greatest amount of damage
    int killer_sig; // signature of the guy getting credit for the kill (or -1
                    // if none)
    int idx;
    player* plr = 0; // pointer to a player struct if it was a player who got the kill
    ship* dead_ship = 0; // the ship which was killed

     // percentage to scale the killer's score by if we score based on the
     // amount of damage caused
    float scoring_scale_by_damage = 1;

    int kill_score;

    // we don't evaluate kills on anything except ships
    if (ship_objp->type != OBJ_SHIP) { return -1; }

    if ((ship_objp->instance < 0) || (ship_objp->instance >= MAX_SHIPS)) {
        return -1;
    }

    // assign the dead ship
    dead_ship = &Ships[ship_objp->instance];

    if (ship_objp == Player_obj) { Player->stats.m_player_deaths++; }

    // clear out invalid damager ships
    for (idx = 0; idx < MAX_DAMAGE_SLOTS; idx++) {
        if ((dead_ship->damage_ship_id[idx] >= 0) &&
            (ship_get_by_signature (dead_ship->damage_ship_id[idx]) < 0)) {
            dead_ship->damage_ship[idx] = 0.0f;
            dead_ship->damage_ship_id[idx] = -1;
        }
    }

    // determine which object did the most damage to the dying object, and how
    // much damage that was
    max_damage_index = -1;
    for (idx = 0; idx < MAX_DAMAGE_SLOTS; idx++) {
        // bogus ship
        if (dead_ship->damage_ship_id[idx] < 0) { continue; }

        // if this slot did more damage then the next highest slot
        if ((max_damage_index == -1) ||
            (dead_ship->damage_ship[idx] >
             dead_ship->damage_ship[max_damage_index])) {
            max_damage_index = idx;
        }
    }

    // doh
    if ((max_damage_index < 0) || (max_damage_index >= MAX_DAMAGE_SLOTS)) {
        return -1;
    }

    // the pct of total damage applied to this ship
    max_damage_pct = dead_ship->damage_ship[max_damage_index] /
                     dead_ship->total_damage_received;

    CLAMP (max_damage_pct, 0.0f, 1.0f);

    // only evaluate if the max damage % is high enough to record a kill and it
    // was done by a valid object
    if (max_damage_pct >= Kill_percentage &&
        dead_ship->damage_ship_id[max_damage_index] >= 0) {
        // set killer_sig for this ship to the signature of the guy who gets
        // credit for the kill
        killer_sig = dead_ship->damage_ship_id[max_damage_index];

        // set the scale value if we only award 100% score for 100% damage
        if (The_mission.ai_profile
                ->flags[AI::Profile_Flags::Kill_scoring_scales_with_damage]) {
            scoring_scale_by_damage = max_damage_pct;
        }

        // null this out for now
        plr = NULL;

        if (Objects[Player->objnum].signature == killer_sig) {
            plr = Player;
        }

        // if we found a valid player, evaluate some kill details
        if (plr) {
            int si_index;

            // bogus
            if ((plr->objnum < 0) || (plr->objnum >= MAX_OBJECTS)) {
                return -1;
            }

            // get the ship info index of the ship type of this kill.  we need
            // to take ship copies into account here.
            si_index = dead_ship->ship_info_index;

            if (Ship_info[si_index].flags[Ship::Info_Flags::Ship_copy]) {
                char temp[NAME_LENGTH];
                strcpy (temp, Ship_info[si_index].name);
                end_string_at_first_hash_symbol (temp);

                // Goober5000 - previous error checking guarantees that this
                // will be >= 0
                si_index = ship_info_lookup (temp);
            }

            // if he killed a guy on his own team increment his bonehead kills
            if (Ships[Objects[plr->objnum].instance].team == dead_ship->team) {
                if (!(The_mission.flags[Mission::Mission_Flags::No_traitor])) {
                    plr->stats.m_bonehead_kills++;

                    kill_score = -(int)(
                        dead_ship->score * scoring_get_scale_factor ());

                    plr->stats.m_score += kill_score;
                }
            }
            // otherwise increment his valid kill count and score
            else {
                plr->stats.m_okKills[si_index]++;
                plr->stats.m_kill_count_ok++;

                kill_score = (int)(
                    dead_ship->score * scoring_get_scale_factor () *
                    scoring_scale_by_damage);

                plr->stats.m_score += kill_score;
                hud_gauge_popup_start (HUD_KILLS_GAUGE);
            }

            // increment his all-encompassing kills
            plr->stats.m_kills[si_index]++;
            plr->stats.m_kill_count++;
        }
    }
    else {
        // set killer_sig for this ship to -1, indicating no one got the kill
        // for it
        killer_sig = -1;
    }

    // pass in the guy who got the credit for the kill (if any), so that he
    // doesn't also get credit for an assist
    scoring_eval_assists (dead_ship, killer_sig);

    return max_damage_index;
}

// evaluate a kill on a weapon, right now this is only called on bombs.
// -Halleck
int scoring_eval_kill_on_weapon (object* weapon_obj, object* other_obj) {
    int killer_sig; // signature of the guy getting credit for the kill (or -1
                    // if none)

    // pointer to a player struct if it was a player who got the kill
    player* plr = 0;

    int kill_score;

    // we don't evaluate kills on anything except weapons
    // also make sure there was a killer, and that it was a ship
    if ((weapon_obj->type != OBJ_WEAPON) || (weapon_obj->instance < 0) ||
        (weapon_obj->instance >= MAX_WEAPONS) || (other_obj == nullptr) ||
        (other_obj->type != OBJ_WEAPON) || (other_obj->instance < 0) ||
        (other_obj->instance >= MAX_WEAPONS) || (other_obj->parent == -1) ||
        (Objects[other_obj->parent].type != OBJ_SHIP)) {
        return -1;
    }

    weapon* dead_wp =
        &Weapons[weapon_obj->instance]; // the weapon that was killed

    weapon_info* dead_wip =
        &Weapon_info[dead_wp->weapon_info_index]; // info on the weapon that
                                                  // was killed

    // we don't evaluate kills on anything except bombs, currently. -Halleck
    if (!(dead_wip->wi_flags[Weapon::Info_Flags::Bomb])) { return -1; }

    // set killer_sig for this weapon to the signature of the guy who gets
    // credit for the kill
    killer_sig = Objects[other_obj->parent].signature;

    // only evaluate if the kill was done by a valid object
    if (killer_sig >= 0) {
        // null this out for now
        plr = NULL;

        if (Objects[Player->objnum].signature == killer_sig) {
            plr = Player;
        }

        // if we found a valid player, evaluate some kill details
        if (plr != NULL) {
            // bogus
            if ((plr->objnum < 0) || (plr->objnum >= MAX_OBJECTS)) {
                return -1;
            }

            // if he killed a bomb on his own team increment his bonehead kills
            if (Ships[Objects[plr->objnum].instance].team == dead_wp->team) {
                if (!(The_mission.flags[Mission::Mission_Flags::No_traitor])) {
                    plr->stats.m_bonehead_kills++;

                    kill_score = -(int)(
                        dead_wip->score * scoring_get_scale_factor ());

                    plr->stats.m_score += kill_score;
                }
            }
            // otherwise increment his valid kill count and score
            else {
                // bombs don't scale with difficulty at the moment. If we
                // change this we want to *scoring_get_scale_factor()
                kill_score = dead_wip->score;

                plr->stats.m_score += kill_score;
                hud_gauge_popup_start (HUD_KILLS_GAUGE);
            }
        }
    }

    return killer_sig;
}

// kill_id is the object signature of the guy who got the credit for the kill
// (may be -1, if no one got it) this is to ensure that you don't also get an
// assist if you get the kill.
void scoring_eval_assists (ship* sp, int killer_sig) {
    int idx;
    player* plr;

    // percentage to scale the score by if we score based on the amount
    // of damage caused
    float scoring_scale_by_damage = 1;

    int assist_score;
    float scoring_scale_factor;

    // evaluate each damage slot to see if it did enough to give the assis
    for (idx = 0; idx < MAX_DAMAGE_SLOTS; idx++) {
        // if this slot did enough damage to get an assist
        if (((sp->damage_ship[idx] / sp->total_damage_received) >=
             Assist_percentage) ||
            (The_mission.ai_profile->flags
                 [AI::Profile_Flags::Assist_scoring_scales_with_damage])) {
            // get the player which did this damage (if any)
            plr = NULL;

            if (Objects[Player->objnum].signature ==
                sp->damage_ship_id[idx]) {
                plr = Player;
            }

            // if we found a player, give him the assist if he attacks it
            if ((plr != NULL) &&
                (iff_x_attacks_y (
                    Ships[Objects[plr->objnum].instance].team, sp->team)) &&
                (killer_sig != Objects[plr->objnum].signature)) {
                // player has to equal the threshold to get an assist
                if ((sp->damage_ship[idx] / sp->total_damage_received) >=
                    Assist_percentage) {
                    plr->stats.m_assists++;
                    WARNINGF (LOCATION,"-==============GAVE PLAYER %s AN ASSIST=====================-",plr->callsign);
                }

                scoring_scale_factor = scoring_get_scale_factor ();

                // maybe award assist points based on damage
                if (The_mission.ai_profile
                        ->flags[AI::Profile_Flags::
                                    Assist_scoring_scales_with_damage]) {
                    scoring_scale_by_damage =
                        (sp->damage_ship[idx] / sp->total_damage_received);

                    assist_score =
                        (int)(sp->score * scoring_scale_factor * scoring_scale_by_damage);
                    
                    plr->stats.m_score += assist_score;
                }
                // otherwise give the points based on the percentage in the
                // mission file
                else {
                    assist_score =
                        (int)(sp->score * sp->assist_score_pct * scoring_scale_factor);
                    plr->stats.m_score += assist_score;
                }
            }
        }
    }
}

// eval a hit on an object (for primary and secondary hit purposes)
void scoring_eval_hit (object* hit_obj, object* other_obj, int from_blast) {
    // only evaluate hits on ships, asteroids, and weapons! -Halleck
    if ((hit_obj->type != OBJ_SHIP) && (hit_obj->type != OBJ_ASTEROID) &&
        (hit_obj->type != OBJ_WEAPON)) {
        return;
    }

    // if the other_obj == NULL, we can't evaluate where it came from, so bail
    // here
    if (other_obj == NULL) { return; }

    // other bogus situtations
    if (other_obj->instance < 0) { return; }

    if ((other_obj->type == OBJ_WEAPON) &&
        !(Weapons[other_obj->instance]
              .weapon_flags[Weapon::Weapon_Flags::Already_applied_stats])) {
        // bogus weapon
        if (other_obj->instance >= MAX_WEAPONS) { return; }

        // bogus parent
        if (other_obj->parent < 0) { return; }
        if (other_obj->parent >= MAX_OBJECTS) { return; }
        if (Objects[other_obj->parent].type != OBJ_SHIP) { return; }
        if ((Objects[other_obj->parent].instance < 0) ||
            (Objects[other_obj->parent].instance >= MAX_SHIPS)) {
            return;
        }

        // Only evaluate hits on bomb weapons. -Halleck
        bool hit_obj_is_bomb = false;

        if (hit_obj->type == OBJ_WEAPON) {
            // Hit weapon is bogus
            if (hit_obj->instance >= MAX_WEAPONS) { return; }

            hit_obj_is_bomb =
                (Weapon_info[Weapons[hit_obj->instance].weapon_info_index]
                     .wi_flags[Weapon::Info_Flags::Bomb])
                    ? true
                    : false;

            // If it's not a bomb but just a regular weapon, we don't care
            // about it (for now, at least.) -Halleck
            if (!hit_obj_is_bomb) { return; }
        }

        int is_bonehead = 0;
        int sub_type =
            Weapon_info[Weapons[other_obj->instance].weapon_info_index]
                .subtype;

        // determine if this was a bonehead hit or not
        if (hit_obj->type == OBJ_SHIP) {
            is_bonehead =
                Ships[hit_obj->instance].team ==
                        Ships[Objects[other_obj->parent].instance].team
                    ? 1
                    : 0;
        }
        else if (hit_obj_is_bomb) {
            is_bonehead =
                Weapons[hit_obj->instance].team ==
                        Ships[Objects[other_obj->parent].instance].team
                    ? 1
                    : 0;
        }
        // can't have a bonehead hit on an asteroid
        else {
            is_bonehead = 0;
        }

        // set the flag indicating that we've already applied a "stats" hit for
        // this weapon Weapons[other_obj->instance].weapon_flags |=
        // WF_ALREADY_APPLIED_STATS;

        if (Player_obj == &(Objects[other_obj->parent])) {
            switch (sub_type) {
            case WP_LASER:
                if (is_bonehead) { Player->stats.mp_bonehead_hits++; }
                else {
                    Player->stats.mp_shots_hit++;
                }
                break;
            case WP_MISSILE:
                // friendly hit, once it hits a friendly, its done
                if (is_bonehead) {
                    if (!from_blast) { Player->stats.ms_bonehead_hits++; }
                }
                // hostile hit
                else {
                    // if its a bomb, count every bit of damage it does
                    if (Weapon_info[Weapons[other_obj->instance]
                                    .weapon_info_index]
                        .wi_flags[Weapon::Info_Flags::Bomb]) {
                        // once we get impact damage, stop keeping track of
                        // it
                        Player->stats.ms_shots_hit++;
                    }
                    // if its not a bomb, only count impact damage
                    else {
                        if (!from_blast) { Player->stats.ms_shots_hit++; }
                    }
                }
                break;
            default: break;
            }
        }
    }
}

// get a scaling factor for adding/subtracting from mission score
float scoring_get_scale_factor () {
    // check for bogus Skill_level values
    ASSERT ((Game_skill_level >= 0) && (Game_skill_level < NUM_SKILL_LEVELS));
    
    if ((Game_skill_level < 0) || (Game_skill_level > NUM_SKILL_LEVELS - 1)) {
        return Scoring_scale_factors[0];
    }

    // return the correct scale value
    return Scoring_scale_factors[Game_skill_level];
}

// ----------------------------------------------------------------------------------------
// DCF functions
//

// bash the passed player to the specified rank
void scoring_bash_rank (player* pl, int rank) {
    // if this is an invalid rank, do nothing
    if ((rank < RANK_ENSIGN) || (rank > RANK_ADMIRAL)) {
        WARNINGF (LOCATION, "Could not bash player rank - invalid value!!!");
        return;
    }

    // set the player's score and rank
    pl->stats.score = Ranks[rank].points + 1;
    pl->stats.rank = rank;
}

DCF (rank, "changes player rank") {
    int rank;

    if (dc_optional_string_either ("help", "--help")) {
        dc_printf ("Usage: rank <index>\n");
        dc_printf (
            " <index> The rank index you wish to have. For retail ranks, "
            "these correspond to:\n");
        dc_printf ("\t0 : Ensign\n");
        dc_printf ("\t1 : Lieutenant Junior Grade\n");
        dc_printf ("\t2 : Lietenant\n");
        dc_printf ("\t3 : Lieutenant Commander\n");
        dc_printf ("\t4 : Commander\n");
        dc_printf ("\t5 : Captain\n");
        dc_printf ("\t6 : Commodore\n");
        dc_printf ("\t7 : Rear Admiral\n");
        dc_printf ("\t8 : Vice Admiral\n");
        dc_printf ("\t9 : Admiral\n\n");
        return;
    }

    if (dc_optional_string_either ("status", "--status") ||
        dc_optional_string_either ("?", "--?")) {
        if (Player != NULL) {
            dc_printf ("Current rank is %i\n", Player->stats.rank);
        }
        else {
            dc_printf ("Error! Current Player not active or loaded\n");
        }
    }

    dc_stuff_int (&rank);

    // parse the argument and change things around accordingly
    if (Player != NULL) { scoring_bash_rank (Player, rank); }
    else {
        dc_printf ("Error! Current Player not active or loaded\n");
    }
}

void scoreing_close () {
    std::map< int, char* >::iterator it;
    for (int i = 0; i < NUM_RANKS; i++) { Ranks[i].promotion_text.clear (); }
}
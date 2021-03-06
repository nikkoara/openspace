// -*- mode: c++; -*-

#include "defs.hh"

#include "ai/ai.hh"
#include "assert/assert.hh"
#include "fireball/fireballs.hh"
#include "gamesnd/gamesnd.hh"
#include "hud/hudartillery.hh"
#include "hud/hudmessage.hh"
#include "io/timer.hh"
#include "log/log.hh"
#include "math/prng.hh"
#include "math/vecmat.hh"
#include "object/object.hh"
#include "parse/parselo.hh"
#include "shared/alphacolors.hh"
#include "sound/sound.hh"
#include "util/list.hh"
#include "weapon/beam.hh"
#include "weapon/weapon.hh"

// -----------------------------------------------------------------------------------------------------------------------
// ARTILLERY DEFINES/VARS
//
// Goober5000 - moved to hudartillery.h

// -----------------------------------------------------------------------------------------------------------------------
// ARTILLERY FUNCTIONS
//

// test code for subspace missile strike
// -------------------------------------------

// ssm_info, like ship_info etc.
std::vector< ssm_info > Ssm_info;

// list of active strikes
std::list< ssm_strike > Ssm_strikes;

// Goober5000
int ssm_info_lookup (const char* name) {
    if (name == NULL) return -1;

    for (auto it = Ssm_info.cbegin (); it != Ssm_info.cend (); ++it)
        if (!strcasecmp (name, it->name))
            return (int)std::distance (Ssm_info.cbegin (), it);

    return -1;
}

void parse_ssm (const char* filename) {
    char weapon_name[NAME_LENGTH];

    try {
        read_file_text (filename, CF_TYPE_TABLES);
        reset_parse ();

        // parse the table
        while (required_string_either ("#end", "$SSM:")) {
            required_string ("$SSM:");
            ssm_info s;
            int string_index;

            // name
            stuff_string (s.name, F_NAME, NAME_LENGTH);
            if (*s.name == 0) {
                sprintf (s.name, "SSM %zu", Ssm_info.size ());
                WARNINGF (LOCATION,"Found an SSM entry without a name.  Assigning \"%s\".",s.name);
            }

            // stuff data
            required_string ("+Weapon:");
            stuff_string (weapon_name, F_NAME, NAME_LENGTH);

            string_index = optional_string_either ("+Count:", "+Min Count:");
            if (string_index == 0) {
                stuff_int (&s.count);
                s.max_count = -1;
            }
            else if (string_index == 1) {
                stuff_int (&s.count);
                required_string ("+Max Count:");
                stuff_int (&s.max_count);
            }
            else {
                s.count = 1;
                s.max_count = -1;
            }

            required_string ("+WarpRadius:");
            stuff_float (&s.warp_radius);

            if (optional_string ("+WarpTime:")) {
                stuff_float (&s.warp_time);
                // According to fireballs.cpp, "Warp lifetime must be at least
                // 4 seconds!"
                if ((s.warp_time) < 4.0f) {
                    // So let's warn them before they try to use it, shall we?
                    WARNINGF (LOCATION,"Expected a '+WarpTime:' value equal or greater than 4.0, found '%f' in weapon '%s'.\n Setting to 4.0, please check and set to a number 4.0 or greater!",s.warp_time, weapon_name);
                    // And then make the Assert obsolete -- Zacam
                    s.warp_time = 4.0f;
                }
            }
            else {
                s.warp_time = 4.0f;
            }

            string_index = required_string_either ("+Radius:", "+Min Radius:");
            if (string_index == 0) {
                required_string ("+Radius:");
                stuff_float (&s.radius);
                s.max_radius = -1.0f;
            }
            else {
                required_string ("+Min Radius:");
                stuff_float (&s.radius);
                required_string ("+Max Radius:");
                stuff_float (&s.max_radius);
            }

            string_index = optional_string_either ("+Offset:", "+Min Offset:");
            if (string_index == 0) {
                stuff_float (&s.offset);
                s.max_offset = -1.0f;
            }
            else if (string_index == 1) {
                stuff_float (&s.offset);
                required_string ("+Max Offset:");
                stuff_float (&s.max_offset);
            }
            else {
                s.offset = 0.0f;
                s.max_offset = -1.0f;
            }

            if (optional_string ("+Shape:")) {
                switch (
                    required_string_one_of (3, "Point", "Circle", "Sphere")) {
                case 0:
                    required_string ("Point");
                    s.shape = SSM_SHAPE_POINT;
                    break;
                case 1: required_string ("Circle");
                case -1: // If we're ignoring parse errors and can't identify
                         // the shape, go with a circle.
                    s.shape = SSM_SHAPE_CIRCLE;
                    break;
                case 2:
                    required_string ("Sphere");
                    s.shape = SSM_SHAPE_SPHERE;
                    break;
                default: ASSERT (0);
                }
            }
            else {
                s.shape = SSM_SHAPE_CIRCLE;
            }

            if (optional_string ("+HUD Message:"))
                stuff_boolean (&s.send_message);
            else
                s.send_message = true;

            if (optional_string ("+Custom Message:")) {
                stuff_string (s.message, F_NAME, NAME_LENGTH);
                s.use_custom_message = true;
            }

            s.sound_index = gamesnd_id ();
            parse_game_sound ("+Alarm Sound:", &s.sound_index);

            // see if we have a valid weapon
            s.weapon_info_index = weapon_info_lookup (weapon_name);
            if (s.weapon_info_index >= 0) {
                // valid
                int existing = ssm_info_lookup (s.name);
                if (existing >= 0) { // Redefined the existing entry instead of
                                     // adding a duplicate.
                    Ssm_info[existing] = s;
                }
                else {
                    Ssm_info.push_back (s);
                }
            }
        }
    }
    catch (const parse::ParseException& e) {
        ERRORF (
            LOCATION, "parse failed '%s'!  Error message = %s.\n", filename,
            e.what ());
        return;
    }
}

// game init
void ssm_init () {
    if (cf_exists_full ("ssm.tbl", CF_TYPE_TABLES)) {
        WARNINGF (LOCATION, "TABLES => Starting parse of 'ssm.tbl'...");
        parse_ssm ("ssm.tbl");
    }
    parse_modular_table (NOX ("*-ssm.tbm"), parse_ssm);

    // Now that we've populated Ssm_info, let's validate weapon $SSM: entries.
    validate_SSM_entries ();
}

void ssm_get_random_start_pos (
    vec3d* out, vec3d* start, matrix* orient, size_t ssm_index) {
    vec3d temp;
    ssm_info* s = &Ssm_info[ssm_index];
    float radius, offset;

    if (s->max_radius == -1.0f)
        radius = s->radius;
    else
        radius = fs2::prng::randf (0, s->radius, s->max_radius);

    if (s->max_offset == -1.0f)
        offset = s->offset;
    else
        offset = fs2::prng::randf (0, s->offset, s->max_offset);

    switch (s->shape) {
    case SSM_SHAPE_SPHERE:
        // get a random vector in a sphere around the target
        vm_vec_random_in_sphere (&temp, start, radius, 1);
        break;
    case SSM_SHAPE_CIRCLE:
        // get a random vector in the circle of the firing plane
        vm_vec_random_in_circle (&temp, start, orient, radius, 1);
        break;
    case SSM_SHAPE_POINT:
        // boooring
        vm_vec_scale_add (&temp, start, &orient->vec.fvec, radius);
        break;
    default: ASSERT (0); break;
    }

    // offset it a bit
    vm_vec_scale_add (out, &temp, &orient->vec.fvec, offset);
}

// level init
void ssm_level_init () {}

// start a subspace missile effect
void ssm_create (
    object* target, vec3d* start, size_t ssm_index, ssm_firing_info* override,
    int team) {
    ssm_strike ssm;
    matrix dir;
    int idx, count;

    // sanity
    ASSERT (target != NULL);
    if (target == NULL) { return; }
    ASSERT (start != NULL);
    if (start == NULL) { return; }
    if (ssm_index >= Ssm_info.size ()) { return; }

    // Init the ssm data

    count = Ssm_info[ssm_index].count;
    if (Ssm_info[ssm_index].max_count != -1) {
        // To get a range of values between min and max, inclusive:
        // random value = min + randon number % (max - min + 1)
        count += rand32 () % (Ssm_info[ssm_index].max_count - count + 1);
    }

    // override in multiplayer
    if (override != NULL) { ssm.sinfo = *override; }
    // single player or the server
    else {
        // forward orientation
        vec3d temp;

        vm_vec_sub (&temp, &target->pos, start);
        vm_vec_normalize (&temp);

        vm_vector_2_matrix (&dir, &temp, NULL, NULL);

        // stuff info
        ssm.sinfo.count = count;
        ssm.sinfo.ssm_index = ssm_index;
        ssm.sinfo.target = target;
        ssm.sinfo.ssm_team = team;

        // Instead of pushing them on one at a time, let's just grab all the
        // memory we'll need at once (as a side effect, don't need to change
        // the logic from the old array-based code)
        ssm.sinfo.delay_stamp.resize (count);
        ssm.sinfo.start_pos.resize (count);

        for (idx = 0; idx < count; idx++) {
            ssm.sinfo.delay_stamp[idx] =
                timestamp (200 + (int)fs2::prng::randf (0, -199.0f, 1000.0f));
            ssm_get_random_start_pos (
                &ssm.sinfo.start_pos[idx], start, &dir, ssm_index);
        }

        ssm_info* si = &Ssm_info[ssm_index];

        weapon_info* wip = &Weapon_info[si->weapon_info_index];
        if (wip->wi_flags[Weapon::Info_Flags::Beam]) {
            ssm.sinfo.duration =
                ((si->warp_time - ((wip->b_info.beam_warmup / 1000.0f) +
                                   wip->b_info.beam_life +
                                   (wip->b_info.beam_warmdown / 1000.0f))) /
                 2.0f) /
                si->warp_time;
        }
        else {
            ssm.sinfo.duration = 0.5f;
        }
    }

    ssm.done_flags.clear ();
    ssm.done_flags.resize (count);
    ssm.fireballs.clear ();
    ssm.fireballs.resize (count, -1);

    if (Ssm_info[ssm_index].send_message) {
        if (!Ssm_info[ssm_index].use_custom_message)
            HUD_printf ("%s", XSTR ("Firing artillery", 1570));
        else
            HUD_printf ("%s", Ssm_info[ssm_index].message);
    }
    if (Ssm_info[ssm_index].sound_index.isValid ()) {
        snd_play (gamesnd_get_game_sound (Ssm_info[ssm_index].sound_index));
    }

    Ssm_strikes.push_back (ssm);
}

// delete a finished ssm effect
void ssm_delete (std::list< ssm_strike >::iterator ssm) {
    Ssm_strikes.erase (ssm);
}

// process subspace missile stuff
void ssm_process () {
    int idx, finished;
    std::list< ssm_strike >::iterator moveup, eraser;
    ssm_info* si;
    int weapon_objnum;

    // process all strikes
    moveup = Ssm_strikes.begin ();
    while (moveup != Ssm_strikes.end ()) {
        // get the type
        ASSERTX (
            moveup->sinfo.ssm_index < Ssm_info.size (),
            "Invalid SSM index detected!");
        si = &Ssm_info[moveup->sinfo.ssm_index];

        // check all the individual missiles
        finished = 1;
        for (idx = 0; idx < moveup->sinfo.count; idx++) {
            // if this guy is not marked as done
            if (!moveup->done_flags[idx]) {
                finished = 0;

                // if he already has the fireball effect
                if (moveup->fireballs[idx] >= 0) {
                    if ((1.0f - fireball_lifeleft_percent (
                                    &Objects[moveup->fireballs[idx]])) >=
                        moveup->sinfo.duration) {
                        weapon_info* wip = &Weapon_info[si->weapon_info_index];
                        // are we a beam? -MageKing17
                        if (wip->wi_flags[Weapon::Info_Flags::Beam]) {
                            beam_fire_info fire_info;
                            memset (&fire_info, 0, sizeof (beam_fire_info));

                            fire_info.accuracy =
                                0.000001f; // this will guarantee a hit
                            fire_info.shooter = NULL;
                            fire_info.turret = NULL;
                            fire_info.target = moveup->sinfo.target;
                            fire_info.target_subsys = NULL;
                            fire_info.bfi_flags |= BFIF_FLOATING_BEAM;
                            fire_info.starting_pos =
                                moveup->sinfo.start_pos[idx];
                            fire_info.beam_info_index = si->weapon_info_index;
                            fire_info.team =
                                static_cast< char > (moveup->sinfo.ssm_team);

                            // fire the beam
                            beam_fire (&fire_info);

                            moveup->done_flags[idx] = true;
                        }
                        else {
                            // get an orientation
                            vec3d temp;
                            matrix orient;

                            vm_vec_sub (
                                &temp, &moveup->sinfo.target->pos,
                                &moveup->sinfo.start_pos[idx]);
                            vm_vec_normalize (&temp);
                            vm_vector_2_matrix (&orient, &temp, NULL, NULL);

                            // fire the missile and flash the screen
                            weapon_objnum = weapon_create (
                                &moveup->sinfo.start_pos[idx], &orient,
                                si->weapon_info_index, -1, -1, 1);

                            if (weapon_objnum >= 0) {
                                Weapons[Objects[weapon_objnum].instance].team =
                                    moveup->sinfo.ssm_team;
                                Weapons[Objects[weapon_objnum].instance]
                                    .homing_object = moveup->sinfo.target;
                                Weapons[Objects[weapon_objnum].instance]
                                    .target_sig =
                                    moveup->sinfo.target->signature;
                            }

                            // this makes this particular missile done
                            moveup->done_flags[idx] = true;
                        }
                    }
                }
                // maybe create his warpin effect
                else if (
                    (moveup->sinfo.delay_stamp[idx] >= 0) &&
                    timestamp_elapsed (moveup->sinfo.delay_stamp[idx])) {
                    // get an orientation
                    vec3d temp;
                    matrix orient;

                    vm_vec_sub (
                        &temp, &moveup->sinfo.target->pos,
                        &moveup->sinfo.start_pos[idx]);
                    vm_vec_normalize (&temp);
                    vm_vector_2_matrix (&orient, &temp, NULL, NULL);
                    moveup->fireballs[idx] = fireball_create (
                        &moveup->sinfo.start_pos[idx], FIREBALL_WARP,
                        FIREBALL_WARP_EFFECT, -1, si->warp_radius, 0,
                        &vmd_zero_vector, si->warp_time, 0, &orient);
                }
            }
        }
        if (finished) {
            eraser = moveup;
            ++moveup;
            ssm_delete (eraser);
            continue;
        }

        ++moveup;
    }
}

// test code for subspace missile strike
// -------------------------------------------

// level init
void hud_init_artillery () {}

// update all hud artillery related stuff
void hud_artillery_update () {}

// render all hud artillery related stuff
void hud_artillery_render () {
    // render how long the player has been painting his target
    if ((Player_ai != NULL) && (Player_ai->artillery_objnum >= 0)) {
        gr_set_color_fast (&Color_bright_blue);
        gr_printf_no_resize (
            gr_screen.center_offset_x + 10, gr_screen.center_offset_y + 50,
            "%f", Player_ai->artillery_lock_time);
    }
}

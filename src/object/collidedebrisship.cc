// -*- mode: c++; -*-

#include "defs.hh"

#include "asteroid/asteroid.hh"
#include "debris/debris.hh"
#include "hud/hud.hh"
#include "io/timer.hh"
#include "log/log.hh"
#include "math/prng.hh"
#include "object/objcollide.hh"
#include "object/object.hh"
#include "playerman/player.hh"
#include "ship/ship.hh"
#include "ship/shiphit.hh"

void calculate_ship_ship_collision_physics (
    collision_info_struct* ship_ship_hit_info);

/**
 * Checks debris-ship collisions.
 * @param pair obj_pair pointer to the two objects. pair->a is debris and
 * pair->b is ship.
 * @return 1 if all future collisions between these can be ignored
 */
int collide_debris_ship (obj_pair* pair) {
    float dist;
    object* pdebris = pair->a;
    object* pship = pair->b;

    // Don't check collisions for warping out player
    if (Player->control_mode != PCM_NORMAL) {
        if (pship == Player_obj) return 0;
    }

    ASSERT (pdebris->type == OBJ_DEBRIS);
    ASSERT (pship->type == OBJ_SHIP);

    // don't check collision if it's our own debris and we are dying
    if ((pdebris->parent == OBJ_INDEX (pship)) &&
        (Ships[pship->instance].flags[Ship::Ship_Flags::Dying]))
        return 0;

    dist = vm_vec_dist (&pdebris->pos, &pship->pos);
    if (dist < pdebris->radius + pship->radius) {
        int hit;
        vec3d hitpos;
        // create and initialize ship_ship_hit_info struct
        collision_info_struct debris_hit_info;
        init_collision_info_struct (&debris_hit_info);

        if (pdebris->phys_info.mass > pship->phys_info.mass) {
            debris_hit_info.heavy = pdebris;
            debris_hit_info.light = pship;
        }
        else {
            debris_hit_info.heavy = pship;
            debris_hit_info.light = pdebris;
        }

        hit =
            debris_check_collision (pdebris, pship, &hitpos, &debris_hit_info);

        if (hit) {
            float ship_damage;
            float debris_damage;

            // do collision physics
            calculate_ship_ship_collision_physics (&debris_hit_info);

            if (debris_hit_info.impulse < 0.5f) return 0;

            // calculate ship damage
            ship_damage =
                0.005f *
                debris_hit_info.impulse; // Cut collision-based damage in half.

            // Decrease heavy damage by 2x.
            if (ship_damage > 5.0f)
                ship_damage = 5.0f + (ship_damage - 5.0f) / 2.0f;

            // calculate debris damage and set debris damage to greater or
            // debris and ship debris damage is needed since we can really
            // whack some small debris with afterburner and not do
            // significant damage to ship but the debris goes off faster
            // than afterburner speed.
            debris_damage =
                debris_hit_info.impulse /
                pdebris->phys_info.mass; // ie, delta velocity of debris

            debris_damage =
                (debris_damage > ship_damage) ? debris_damage : ship_damage;

            // modify ship damage by debris damage multiplier
            ship_damage *= Debris[pdebris->instance].damage_mult;

            // supercaps cap damage at 10-20% max hull ship damage
            if (Ship_info[Ships[pship->instance].ship_info_index]
                    .flags[Ship::Info_Flags::Supercap]) {
                float cap_percent_damage = fs2::prng::randf (0, 0.1f, 0.2f);
                ship_damage = MIN (
                    ship_damage,
                    cap_percent_damage *
                        Ships[pship->instance].ship_max_hull_strength);
            }

            // apply damage to debris
            debris_hit (
                pdebris, pship, &hitpos, debris_damage); // speed => damage
            int quadrant_num, apply_ship_damage;

            // apply damage to ship unless 1) debris is from ship
            apply_ship_damage = !(pship->signature == pdebris->parent_sig);

            if (debris_hit_info.heavy == pship) {
                quadrant_num = get_ship_quadrant_from_global (&hitpos, pship);
                if ((pship->flags[Object::Object_Flags::No_shields]) ||
                    !ship_is_shield_up (pship, quadrant_num)) {
                    quadrant_num = -1;
                }
                if (apply_ship_damage) {
                    ship_apply_local_damage (
                        debris_hit_info.heavy, debris_hit_info.light, &hitpos,
                        ship_damage, quadrant_num, CREATE_SPARKS,
                        debris_hit_info.submodel_num);
                }
            }
            else {
                // don't draw sparks using sphere hit position
                if (apply_ship_damage) {
                    ship_apply_local_damage (
                        debris_hit_info.light, debris_hit_info.heavy, &hitpos,
                        ship_damage, MISS_SHIELDS, NO_SPARKS);
                }
            }

            // maybe print Collision on HUD
            if (pship == Player_obj) {
                hud_start_text_flash (XSTR ("Collision", 1431), 2000);
            }

            collide_ship_ship_do_sound (
                &hitpos, pship, pdebris, pship == Player_obj);
        }
        return 0;
    }
    else { // Bounding spheres don't intersect, set timestamp for next
           // collision check.
        float ship_max_speed, debris_speed;
        float time;
        ship* shipp;

        shipp = &Ships[pship->instance];

        if (ship_is_beginning_warpout_speedup (pship)) {
            ship_max_speed = MAX (
                ship_get_max_speed (shipp), ship_get_warpout_speed (pship));
        }
        else {
            ship_max_speed = ship_get_max_speed (shipp);
        }
        ship_max_speed = MAX (ship_max_speed, 10.0f);
        ship_max_speed = MAX (ship_max_speed, pship->phys_info.vel.xyz.z);

        debris_speed = pdebris->phys_info.speed;

        time = 1000.0f * (dist - pship->radius - pdebris->radius - 10.0f) /
               (ship_max_speed + debris_speed); // 10.0f is a safety factor
        time -= 200.0f; // allow one frame slow frame at ~5 fps

        if (time > 100) { pair->next_check_time = timestamp (int (time)); }
        else {
            pair->next_check_time = timestamp (0); // check next time
        }
    }

    return 0;
}

/**
 * Checks asteroid-ship collisions.
 * @param pair obj_pair pointer to the two objects. pair->a is asteroid and
 * pair->b is ship.
 * @return 1 if all future collisions between these can be ignored
 */
int collide_asteroid_ship (obj_pair* pair) {
    if (!Asteroids_enabled) return 0;

    float dist;
    object* pasteroid = pair->a;
    object* pship = pair->b;

    // Don't check collisions for warping out player
    if (Player->control_mode != PCM_NORMAL) {
        if (pship == Player_obj) return 0;
    }

    if (pasteroid->hull_strength < 0.0f) return 0;

    ASSERT (pasteroid->type == OBJ_ASTEROID);
    ASSERT (pship->type == OBJ_SHIP);

    dist = vm_vec_dist (&pasteroid->pos, &pship->pos);

    if (dist < pasteroid->radius + pship->radius) {
        int hit;
        vec3d hitpos;
        // create and initialize ship_ship_hit_info struct
        collision_info_struct asteroid_hit_info;
        init_collision_info_struct (&asteroid_hit_info);

        if (pasteroid->phys_info.mass > pship->phys_info.mass) {
            asteroid_hit_info.heavy = pasteroid;
            asteroid_hit_info.light = pship;
        }
        else {
            asteroid_hit_info.heavy = pship;
            asteroid_hit_info.light = pasteroid;
        }

        hit = asteroid_check_collision (
            pasteroid, pship, &hitpos, &asteroid_hit_info);

        if (hit) {
            float ship_damage;
            float asteroid_damage;

            vec3d asteroid_vel = pasteroid->phys_info.vel;

            // do collision physics
            calculate_ship_ship_collision_physics (&asteroid_hit_info);

            if (asteroid_hit_info.impulse < 0.5f) return 0;

            // limit damage from impulse by making max impulse (for damage)
            // 2*m*v_max_relative
            float max_ship_impulse =
                (2.0f * pship->phys_info.max_vel.xyz.z +
                 vm_vec_mag_quick (&asteroid_vel)) *
                (pship->phys_info.mass * pasteroid->phys_info.mass) /
                (pship->phys_info.mass + pasteroid->phys_info.mass);

            if (asteroid_hit_info.impulse > max_ship_impulse) {
                ship_damage = 0.001f * max_ship_impulse;
            }
            else {
                ship_damage =
                    0.001f *
                    asteroid_hit_info
                        .impulse; // Cut collision-based damage in half.
            }

            // Decrease heavy damage by 2x.
            if (ship_damage > 5.0f)
                ship_damage = 5.0f + (ship_damage - 5.0f) / 2.0f;

            if ((ship_damage > 500.0f) &&
                (ship_damage >
                 Ships[pship->instance].ship_max_hull_strength / 8.0f)) {
                ship_damage =
                    Ships[pship->instance].ship_max_hull_strength / 8.0f;
                WARNINGF (LOCATION,"Pinning damage to %s from asteroid at %7.3f (%7.3f percent)",Ships[pship->instance].ship_name, ship_damage,100.0f * ship_damage /Ships[pship->instance].ship_max_hull_strength);
            }

            // Decrease damage during warp out because it's annoying when
            // your escoree dies during warp out.
            if (Ai_info[Ships[pship->instance].ai_index].mode == AIM_WARP_OUT)
                ship_damage /= 3.0f;

            // calculate asteroid damage and set asteroid damage to greater
            // or asteroid and ship asteroid damage is needed since we can
            // really whack some small asteroid with afterburner and not do
            // significant damage to ship but the asteroid goes off faster
            // than afterburner speed.
            asteroid_damage =
                asteroid_hit_info.impulse /
                pasteroid->phys_info.mass; // ie, delta velocity of asteroid
            asteroid_damage = (asteroid_damage > ship_damage) ? asteroid_damage
                                                              : ship_damage;

            // apply damage to asteroid
            asteroid_hit (
                pasteroid, pship, &hitpos,
                asteroid_damage); // speed => damage

            // extern fix Missiontime;

            int quadrant_num;
            if (asteroid_hit_info.heavy == pship) {
                quadrant_num = get_ship_quadrant_from_global (&hitpos, pship);
                if ((pship->flags[Object::Object_Flags::No_shields]) ||
                    !ship_is_shield_up (pship, quadrant_num)) {
                    quadrant_num = -1;
                }
                ship_apply_local_damage (
                    asteroid_hit_info.heavy, asteroid_hit_info.light, &hitpos,
                    ship_damage, quadrant_num, CREATE_SPARKS,
                    asteroid_hit_info.submodel_num);
            }
            else {
                // don't draw sparks (using sphere hitpos)
                ship_apply_local_damage (
                    asteroid_hit_info.light, asteroid_hit_info.heavy, &hitpos,
                    ship_damage, MISS_SHIELDS, NO_SPARKS);
            }

            // maybe print Collision on HUD
            if (pship == Player_obj) {
                hud_start_text_flash (XSTR ("Collision", 1431), 2000);
            }

            collide_ship_ship_do_sound (
                &hitpos, pship, pasteroid, pship == Player_obj);
        }

        return 0;
    }
    else {
        // estimate earliest time at which pair can hit
        float asteroid_max_speed, ship_max_speed, time;
        ship* shipp = &Ships[pship->instance];

        asteroid_max_speed = vm_vec_mag (
            &pasteroid->phys_info
                 .vel); // Asteroid... vel gets reset, not max vel.z
        asteroid_max_speed = MAX (asteroid_max_speed, 10.0f);

        if (ship_is_beginning_warpout_speedup (pship)) {
            ship_max_speed = MAX (
                ship_get_max_speed (shipp), ship_get_warpout_speed (pship));
        }
        else {
            ship_max_speed = ship_get_max_speed (shipp);
        }
        ship_max_speed = MAX (ship_max_speed, 10.0f);
        ship_max_speed = MAX (ship_max_speed, pship->phys_info.vel.xyz.z);

        time =
            1000.0f * (dist - pship->radius - pasteroid->radius - 10.0f) /
            (asteroid_max_speed + ship_max_speed); // 10.0f is a safety factor
        time -= 200.0f; // allow one frame slow frame at ~5 fps

        if (time > 100) { pair->next_check_time = timestamp (int (time)); }
        else {
            pair->next_check_time = timestamp (0); // check next time
        }
        return 0;
    }
}

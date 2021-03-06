// -*- mode: c++; -*-

#ifndef FREESPACE2_GRAPHICS_SHADOWS_HH
#define FREESPACE2_GRAPHICS_SHADOWS_HH

#include "defs.hh"

#include "object/object.hh"

#define MAX_SHADOW_CASCADES 4

struct light_frustum_info {
    matrix4 proj_matrix;

    vec3d min;
    vec3d max;

    float start_dist;
};

extern matrix4 Shadow_view_matrix;
extern matrix4 Shadow_proj_matrix[MAX_SHADOW_CASCADES];
extern float Shadow_cascade_distances[MAX_SHADOW_CASCADES];

void shadows_construct_light_frustum (
    vec3d* min_out, vec3d* max_out, vec3d light_vec, matrix* orient,
    vec3d* pos, float fov, float aspect, float z_near, float z_far);
bool shadows_obj_in_frustum (
    object* objp, vec3d* min, vec3d* max, matrix* light_orient);
void shadows_render_all (float fov, matrix* eye_orient, vec3d* eye_pos);

matrix shadows_start_render (
    matrix* eye_orient, vec3d* eye_pos, float fov, float aspect,
    float veryneardist, float neardist, float middist, float fardist);
void shadows_end_render ();

#endif // FREESPACE2_GRAPHICS_SHADOWS_HH

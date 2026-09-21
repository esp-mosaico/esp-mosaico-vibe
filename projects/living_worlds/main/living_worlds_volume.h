// SPDX-License-Identifier: Apache-2.0
#pragma once
#include "living_volume.h"
#include "mosaico_game_2d.h"

typedef struct {
    float x,y,z;
    float m[9];
} living_camera_t;

living_camera_t living_camera_orbit(float yaw_deg,float pitch_deg,float focus);
bool living_project_xyz(const living_camera_t *camera,float x,float y,float z,Vector2 *out);
void living_draw_volume(const living_camera_t *camera,
                        const living_volume_vertex_t *vertices,int vertex_count,
                        const living_volume_face_t *faces,int face_count,
                        MosaicoAtlas atlas,float authored_width,float authored_height,
                        int part);
void living_draw_volume_uv(const living_camera_t *camera,
                           const living_volume_vertex_t *vertices,int vertex_count,
                           const living_volume_face_t *faces,int face_count,
                           MosaicoAtlas atlas,float src_w,float src_h,
                           float dst_u0,float dst_v0,float dst_u1,float dst_v1,
                           int part);
void living_cover_reset(void);
void living_cover_add_triangle(Vector2 a,Vector2 b,Vector2 c);
void living_cover_volume(const living_camera_t *camera,
                         const living_volume_vertex_t *vertices,int vertex_count,
                         const living_volume_face_t *faces,int face_count,int part);
void living_cover_seal(void);
int living_cover_quad(Vector2 a,Vector2 b,Vector2 c,Vector2 d);

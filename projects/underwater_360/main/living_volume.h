// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <stdint.h>
typedef struct { int16_t x,y,z,u,v; } living_volume_vertex_t;
typedef struct { uint16_t a,b,c; int8_t nx,ny,nz; uint16_t light; } living_volume_face_t;

// SPDX-License-Identifier: Apache-2.0
#include "underwater_volume.h"
#include <math.h>
#include "mosaico_raylib_fast.h"

#define LIVING_FOCAL (480.0f*1.055f)
#define LIVING_VERTEX_CAP 400

static Vector2 living_screen[LIVING_VERTEX_CAP];
static uint8_t living_valid[LIVING_VERTEX_CAP];

living_camera_t living_camera_orbit(float yaw_deg,float pitch_deg,float focus)
{
    float yaw=yaw_deg*.01745329252f,pitch=pitch_deg*.01745329252f;
    float cp=cosf(pitch),radius=focus;
    living_camera_t camera={.x=radius*sinf(yaw)*cp,
        .y=radius*sinf(pitch),.z=focus-radius*cosf(yaw)*cp};
    float dx=-camera.x,dy=-camera.y,dz=focus-camera.z;
    float length=sqrtf(dx*dx+dy*dy+dz*dz);
    float fx=dx/length,fy=dy/length,fz=dz/length;
    float right_length=sqrtf(fz*fz+fx*fx),rx=fz/right_length,rz=-fx/right_length;
    camera.m[0]=rx;camera.m[1]=0;camera.m[2]=rz;
    camera.m[3]=fy*rz;camera.m[4]=fz*rx-fx*rz;camera.m[5]=-fy*rx;
    camera.m[6]=fx;camera.m[7]=fy;camera.m[8]=fz;
    return camera;
}

bool living_project_xyz(const living_camera_t *camera,float x,float y,float z,Vector2 *out)
{
    float dx=x-camera->x,dy=y-camera->y,dz=z-camera->z;
    float view_z=dx*camera->m[6]+dy*camera->m[7]+dz*camera->m[8];
    if(view_z<.2f)return false;
    float inv=LIVING_FOCAL/view_z;
    /* Orbit cameras keep m[1]=0. */
    out->x=240.0f+inv*(dx*camera->m[0]+dz*camera->m[2]);
    out->y=240.0f-inv*(dx*camera->m[3]+dy*camera->m[4]+dz*camera->m[5]);
    return true;
}

void living_draw_volume_uv(const living_camera_t *camera,
                           const living_volume_vertex_t *vertices,int vertex_count,
                           const living_volume_face_t *faces,int face_count,
                           MosaicoAtlas atlas,float src_w,float src_h,
                           float dst_u0,float dst_v0,float dst_u1,float dst_v1,
                           int part)
{
    if(!atlas.texture.id||vertex_count>LIVING_VERTEX_CAP||src_w<=0||src_h<=0)return;
    for(int i=0;i<vertex_count;++i){
        const living_volume_vertex_t *v=&vertices[i];
        living_valid[i]=(uint8_t)living_project_xyz(camera,v->x*.001f,v->y*.001f,
            v->z*.001f,&living_screen[i]);
    }
    float span_u=dst_u1-dst_u0,span_v=dst_v1-dst_v0;
    for(int i=0;i<face_count;++i){
        unsigned ia=faces[i].a,ib=faces[i].b,ic=faces[i].c;
        if(!living_valid[ia]||!living_valid[ib]||!living_valid[ic])continue;
        const living_volume_vertex_t *a=&vertices[ia],*b=&vertices[ib],*c=&vertices[ic];
        if(part==3){
            float cx=(a->x+b->x+c->x)*.000333333f;
            float cy=(a->y+b->y+c->y)*.000333333f;
            float cz=(a->z+b->z+c->z)*.000333333f;
            float facing=faces[i].nx*(camera->x-cx)+faces[i].ny*(camera->y-cy)+
                         faces[i].nz*(camera->z-cz);
            if(facing<=0)continue;
        }
        Vector2 pa=living_screen[ia],pb=living_screen[ib],pc=living_screen[ic];
        float area=(pb.x-pa.x)*(pc.y-pa.y)-(pb.y-pa.y)*(pc.x-pa.x);
        if(fabsf(area)<=.25f)continue;
        if(part==1&&area<0)continue;
        if(part==2&&area>0)continue;
        float min_x=fminf(pa.x,fminf(pb.x,pc.x));
        float max_x=fmaxf(pa.x,fmaxf(pb.x,pc.x));
        float min_y=fminf(pa.y,fminf(pb.y,pc.y));
        float max_y=fmaxf(pa.y,fmaxf(pb.y,pc.y));
        if(max_x<0||min_x>=480||max_y<0||min_y>=480)continue;
        mosaico_textured_vertex_t va={pa.x,pa.y,dst_u0+a->u/src_w*span_u,
            dst_v0+a->v/src_h*span_v};
        mosaico_textured_vertex_t vb={pb.x,pb.y,dst_u0+b->u/src_w*span_u,
            dst_v0+b->v/src_h*span_v};
        mosaico_textured_vertex_t vc={pc.x,pc.y,dst_u0+c->u/src_w*span_u,
            dst_v0+c->v/src_h*span_v};
        Mosaico2DDrawTexturedTriangle(atlas.texture,va,vb,vc,faces[i].light);
    }
}

void living_draw_volume(const living_camera_t *camera,
                        const living_volume_vertex_t *vertices,int vertex_count,
                        const living_volume_face_t *faces,int face_count,
                        MosaicoAtlas atlas,float authored_width,float authored_height,
                        int part)
{
    living_draw_volume_uv(camera,vertices,vertex_count,faces,face_count,atlas,
        authored_width,authored_height,0.0f,0.0f,(float)atlas.texture.width,
        (float)atlas.texture.height,part);
}

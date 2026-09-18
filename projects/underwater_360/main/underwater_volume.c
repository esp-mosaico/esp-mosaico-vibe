// SPDX-License-Identifier: Apache-2.0
#include "underwater_volume.h"
#include <math.h>
#include <string.h>
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
    float su=(dst_u1-dst_u0)/src_w,sv=(dst_v1-dst_v0)/src_h;
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
        mosaico_textured_vertex_t va={pa.x,pa.y,dst_u0+a->u*su,dst_v0+a->v*sv};
        mosaico_textured_vertex_t vb={pb.x,pb.y,dst_u0+b->u*su,dst_v0+b->v*sv};
        mosaico_textured_vertex_t vc={pc.x,pc.y,dst_u0+c->u*su,dst_v0+c->v*sv};
        Mosaico2DDrawTexturedTriangle(atlas.texture,va,vb,vc,faces[i].light);
    }
}

#define LIVING_COVER_N 32
static uint8_t living_cover[LIVING_COVER_N*LIVING_COVER_N];
static uint8_t living_cover_tmp[LIVING_COVER_N*LIVING_COVER_N];

static int living_point_in_tri(float px,float py,Vector2 a,Vector2 b,Vector2 c)
{
    float v0x=c.x-a.x,v0y=c.y-a.y,v1x=b.x-a.x,v1y=b.y-a.y,v2x=px-a.x,v2y=py-a.y;
    float den=v0x*v1y-v1x*v0y;
    if(den>-1e-4f&&den<1e-4f)return 0;
    float inv=1.0f/den;
    float u=(v2x*v1y-v1x*v2y)*inv,v=(v0x*v2y-v2x*v0y)*inv;
    return u>=0.0f&&v>=0.0f&&u+v<=1.0f;
}

void living_cover_reset(void)
{
    memset(living_cover,0,sizeof living_cover);
}

void living_cover_add_triangle(Vector2 a,Vector2 b,Vector2 c)
{
    float min_x=fminf(a.x,fminf(b.x,c.x)),max_x=fmaxf(a.x,fmaxf(b.x,c.x));
    float min_y=fminf(a.y,fminf(b.y,c.y)),max_y=fmaxf(a.y,fmaxf(b.y,c.y));
    if(max_x<0||min_x>=480||max_y<0||min_y>=480)return;
    const float to_cell=LIVING_COVER_N/480.0f,to_px=480.0f/LIVING_COVER_N;
    int x0=(int)(min_x*to_cell),x1=(int)(max_x*to_cell);
    int y0=(int)(min_y*to_cell),y1=(int)(max_y*to_cell);
    if(x0<0)x0=0;
    if(y0<0)y0=0;
    if(x1>=LIVING_COVER_N)x1=LIVING_COVER_N-1;
    if(y1>=LIVING_COVER_N)y1=LIVING_COVER_N-1;
    for(int cy=y0;cy<=y1;++cy)
    for(int cx=x0;cx<=x1;++cx){
        if(living_point_in_tri((cx+.5f)*to_px,(cy+.5f)*to_px,a,b,c))
            living_cover[cy*LIVING_COVER_N+cx]=1;
    }
}

static int living_cover_face(const living_camera_t *camera,
    const living_volume_vertex_t *vertices,const living_volume_face_t *face,int part)
{
    unsigned ia=face->a,ib=face->b,ic=face->c;
    if(!living_valid[ia]||!living_valid[ib]||!living_valid[ic])return 0;
    const living_volume_vertex_t *a=&vertices[ia],*b=&vertices[ib],*c=&vertices[ic];
    if(part==3){
        float cx=(a->x+b->x+c->x)*.000333333f;
        float cy=(a->y+b->y+c->y)*.000333333f;
        float cz=(a->z+b->z+c->z)*.000333333f;
        float facing=face->nx*(camera->x-cx)+face->ny*(camera->y-cy)+
                     face->nz*(camera->z-cz);
        if(facing<=0)return 0;
    }
    Vector2 pa=living_screen[ia],pb=living_screen[ib],pc=living_screen[ic];
    float area=(pb.x-pa.x)*(pc.y-pa.y)-(pb.y-pa.y)*(pc.x-pa.x);
    if(fabsf(area)<=.25f)return 0;
    if(part==1&&area<0)return 0;
    if(part==2&&area>0)return 0;
    living_cover_add_triangle(pa,pb,pc);
    return 1;
}

void living_cover_volume(const living_camera_t *camera,
                         const living_volume_vertex_t *vertices,int vertex_count,
                         const living_volume_face_t *faces,int face_count,int part)
{
    if(!camera||vertex_count>LIVING_VERTEX_CAP)return;
    for(int i=0;i<vertex_count;++i){
        const living_volume_vertex_t *v=&vertices[i];
        living_valid[i]=(uint8_t)living_project_xyz(camera,v->x*.001f,v->y*.001f,
            v->z*.001f,&living_screen[i]);
    }
    for(int i=0;i<face_count;++i)
        living_cover_face(camera,vertices,&faces[i],part);
}

void living_cover_seal(void)
{
    memcpy(living_cover_tmp,living_cover,sizeof living_cover);
    memset(living_cover,0,sizeof living_cover);
    for(int y=1;y<LIVING_COVER_N-1;++y)
    for(int x=1;x<LIVING_COVER_N-1;++x){
        int i=y*LIVING_COVER_N+x;
        if(living_cover_tmp[i]&&living_cover_tmp[i-1]&&living_cover_tmp[i+1]&&
           living_cover_tmp[i-LIVING_COVER_N]&&living_cover_tmp[i+LIVING_COVER_N])
            living_cover[i]=1;
    }
}

int living_cover_quad(Vector2 a,Vector2 b,Vector2 c,Vector2 d)
{
    const float to_cell=LIVING_COVER_N/480.0f;
    int cells[4][2]={{(int)(a.x*to_cell),(int)(a.y*to_cell)},
                     {(int)(b.x*to_cell),(int)(b.y*to_cell)},
                     {(int)(c.x*to_cell),(int)(c.y*to_cell)},
                     {(int)(d.x*to_cell),(int)(d.y*to_cell)}};
    for(int i=0;i<4;++i){
        int cx=cells[i][0],cy=cells[i][1];
        if((unsigned)cx>=LIVING_COVER_N||(unsigned)cy>=LIVING_COVER_N)return 0;
        if(!living_cover[cy*LIVING_COVER_N+cx])return 0;
    }
    return 1;
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

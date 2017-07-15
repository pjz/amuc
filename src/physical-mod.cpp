/* physical-mod.cpp
 * Derived from api-wrapper.c (from PsIndustrializer), modified by W.Boeke
 * Copyright (c) 2000 David A. Bartold
 * Copyright (c) 2004 Yury G. Aliaev
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "amuc-headers.h"

static const int sample_rate=44100; // same as for wave files

PhmBuffer phm_buf;

struct vector2 {
  float x, z;
};

struct Node {
  bool anchor;
  vector2  pos;
  vector2  vel;
  Node *neighbors[2];
};

struct Nodes {
  int num_nodes;
  Node *nodes;
};

void eval_model (Nodes *obj, float speed, float damp) {
  int i, j;
  vector2 sum,
          dif;
  Node *node;
  float temp;

  for (i = 0; i < obj->num_nodes; i++) {
      node = obj->nodes+i;
      if (!node->anchor) {
          sum.x = sum.z = 0.0;

          for (j = 0; j < 2; j++) {
              dif.x = node->pos.x - node->neighbors[j]->pos.x;
              dif.z = node->pos.z - node->neighbors[j]->pos.z;
              temp = hypot(dif.x, dif.z);
              sum.x -= dif.x * temp;
              sum.z -= dif.z * temp;
          }
          node->vel.x = node->vel.x * damp + sum.x * speed;
          node->vel.z = node->vel.z * damp + sum.z * speed;
        }
    }

  for (i = 0; i < obj->num_nodes; i++) {
      node = obj->nodes+i;

      if (!node->anchor) {
          node->pos.x += node->vel.x * speed;
          node->pos.z += node->vel.z * speed;
      }
  }
}

static float non_linear(float x) {
  const float limit=0.5;
  if (x>0.) {
    if (x>limit) return limit;
    return 2*(x-x*x/2/limit);
  }
  else {
    if (x<-limit) return -limit;
    return 2*(x+x*x/2/limit);
  }
}

uint obj_render(Nodes * obj, int outnode, float speed, 
                         float damp_time, int maxlen, float *samples, bool add_dist, bool timing_only) {
    int i,
        len;
    float sample,
          damp;
    Node *out=obj->nodes+outnode;
    const float stasis = hypot(out->pos.x,out->pos.z);

    len = min(lround(damp_time * sample_rate * 6.0), maxlen); // good value for big damp_time value
    if (timing_only)
      return len;
    damp = pow(0.5, 1.0 / damp_time / sample_rate);

    if (debug)
       printf("obj_render: speed=%.2f damp=%.5f len=%d\n",speed,damp,len);
    for (i = 0; i < len; i++) {
       eval_model(obj, speed, damp);
       sample = hypot(out->pos.x, out->pos.z) - stasis;
       if (i > len-100) sample*=(len-i)/100.;
       if (add_dist)   // distorsion added
         samples[i] = non_linear(sample);
       else
         samples[i] = sample;
    }
    return len;
}

uint obj_render_rod(int height, float tension, float speed,
   			     float damp, int len, float *samples, bool add_dist, bool timing_only) {
  static Nodes *obj;
  Node *node;
  int i;
  if (!obj) {
    obj = new Nodes;
    obj->nodes=new Node[height];
    obj->num_nodes = height;
    for (i = 0; i < height; i++) {
      node = obj->nodes+i;
  
      if (i == 0 || i == height - 1) {
        node->neighbors[0] = node->neighbors[1] = 0;
        node->anchor = true;
      }
      else {
        node->neighbors[0] = obj->nodes+(i - 1);
        node->neighbors[1] = obj->nodes+(i + 1);
        node->anchor = false;
      }
    }
  }
  for (i = 0; i < height; i++) {
    node = obj->nodes+i;
    node->pos.x = 0.;
    node->pos.z = i * tension;  // empirically, this sounds best
    node->vel.x=node->vel.z=0.;
  }
  obj->nodes[1].pos.x = 2.0; // input node

  return obj_render(obj, height-2, speed, damp, len, samples, add_dist, timing_only);
}

static void set_phm_data(ShortBuffer *pmd,PhmCtrl *ctrl,bool timing_only,const int ampl_scale) {
  static float
    tension[6] =   { 0, 0.3,  0.5,  1,    1.5,  2   },  // all indexes: 1 - 5
    speed[6] =     { 0, 0.015,0.02, 0.03, 0.04, 0.05},
    damp[6] =      { 0, 0.01, 0.02, 0.04, 0.06, 0.1 };
  const int max_data_len=sample_rate; // enough for 1 sec
  float data[max_data_len];
  pmd->size=obj_render_rod(
              6,
              tension[ctrl->speed_tension->value.y],
              speed[ctrl->speed_tension->value.x],
              damp[ctrl->decay->value()],
              max_data_len,data,ctrl->add_noise->value,timing_only);
  if (timing_only) return;
  for (int i=0;i<pmd->size;++i)
    pmd->buf[i]=minmax(-30000,int(data[i]*ampl_scale),30000);
}

bool PhmBuffer::set_phys_model(int col,ShortBuffer *pmdat) {
  ShortBuffer *pmd=pmdat ? pmdat : const_data + col;
  var_data[col]=pmd;
  pmd->reset();

  set_phm_data(pmd,col2phm_ctrl(col),true,0); // sets pmd->size
  pmd->buf=new short[pmd->size];
  pmd->alloced=true;
  for (int i=0;i<pmd->size;++i) pmd->buf[i]=0;

  set_phm_data(pmd,col2phm_ctrl(col),false,ampl_scale);
  return true;
}

bool PhmBuffer::init(const int ampl) {
  ampl_scale=ampl;
  for (int n = 0; n<colors_max; ++n) {
    if (!set_phys_model(n,0)) return false;
  }
  return true;
}

void PhmBuffer::reset() {
  for (int n = 0; n<colors_max; ++n)
    var_data[n]=const_data+n;
}

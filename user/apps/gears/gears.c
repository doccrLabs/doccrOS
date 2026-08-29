/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: sulfurOS
 * FILE: gears.c
 * CREDITS: Offihito (for whole gears + tinygl port btw)
 *
 */

#include "libdesktop.h"
#include "ui16.h"
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <GL/gl.h>
#include <zbuffer.h>

static GLfloat view_rotx = 20.0, view_roty = 30.0;
static GLint gear1, gear2, gear3;
static GLfloat angle = 0.0;

static void gear(GLfloat inner_radius, GLfloat outer_radius, GLfloat width,
                 GLint teeth, GLfloat tooth_depth) {
  GLint i;
  GLfloat r0, r1, r2;
  GLfloat angle, da;
  GLfloat u, v, len;

  r0 = inner_radius;
  r1 = outer_radius - tooth_depth / 2.0;
  r2 = outer_radius + tooth_depth / 2.0;
  da = 2.0 * M_PI / teeth / 4.0;

  glNormal3f(0.0, 0.0, 1.0);
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = i * 2.0 * M_PI / teeth;
    glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
    glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
  }
  glEnd();

  glBegin(GL_QUADS);
  for (i = 0; i < teeth; i++) {
    angle = i * 2.0 * M_PI / teeth;
    glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
  }
  glEnd();

  glNormal3f(0.0, 0.0, -1.0);
  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = i * 2.0 * M_PI / teeth;
    glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
               -width * 0.5);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
  }
  glEnd();

  glBegin(GL_QUADS);
  for (i = 0; i < teeth; i++) {
    angle = i * 2.0 * M_PI / teeth;
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
               -width * 0.5);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
               -width * 0.5);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
    glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
  }
  glEnd();

  glBegin(GL_QUAD_STRIP);
  for (i = 0; i < teeth; i++) {
    angle = i * 2.0 * M_PI / teeth;
    glVertex3f(r1 * cos(angle), r1 * sin(angle), width * 0.5);
    glVertex3f(r1 * cos(angle), r1 * sin(angle), -width * 0.5);
    u = r2 * cos(angle + da) - r1 * cos(angle);
    v = r2 * sin(angle + da) - r1 * sin(angle);
    len = sqrt(u * u + v * v);
    u /= len;
    v /= len;
    glNormal3f(v, -u, 0.0);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), width * 0.5);
    glVertex3f(r2 * cos(angle + da), r2 * sin(angle + da), -width * 0.5);
    glNormal3f(cos(angle), sin(angle), 0.0);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da), width * 0.5);
    glVertex3f(r2 * cos(angle + 2 * da), r2 * sin(angle + 2 * da),
               -width * 0.5);
    u = r1 * cos(angle + 3 * da) - r2 * cos(angle + 2 * da);
    v = r1 * sin(angle + 3 * da) - r2 * sin(angle + 2 * da);
    glNormal3f(v, -u, 0.0);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da), width * 0.5);
    glVertex3f(r1 * cos(angle + 3 * da), r1 * sin(angle + 3 * da),
               -width * 0.5);
    glNormal3f(cos(angle), sin(angle), 0.0);
  }
  glVertex3f(r1 * cos(0), r1 * sin(0), width * 0.5);
  glVertex3f(r1 * cos(0), r1 * sin(0), -width * 0.5);
  glEnd();

  glBegin(GL_QUAD_STRIP);
  for (i = 0; i <= teeth; i++) {
    angle = i * 2.0 * M_PI / teeth;
    glNormal3f(-cos(angle), -sin(angle), 0.0);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), -width * 0.5);
    glVertex3f(r0 * cos(angle), r0 * sin(angle), width * 0.5);
  }
  glEnd();
}

void draw_scene() {
  angle += 2.0;
  glPushMatrix();
  glRotatef(view_rotx, 1.0, 0.0, 0.0);
  glRotatef(view_roty, 0.0, 1.0, 0.0);

  glPushMatrix();
  glTranslatef(-3.0, -2.0, 0.0);
  glRotatef(angle, 0.0, 0.0, 1.0);
  glCallList(gear1);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(3.1, -2.0, 0.0);
  glRotatef(-2.0 * angle - 9.0, 0.0, 0.0, 1.0);
  glCallList(gear2);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(-3.1, 4.2, 0.0);
  glRotatef(-2.0 * angle - 25.0, 0.0, 0.0, 1.0);
  glCallList(gear3);
  glPopMatrix();

  glPopMatrix();
}

void init_scene() {
  static GLfloat pos[4] = {5, 5, 10, 0.0};
  static GLfloat red[4] = {1.0, 0.0, 0.0, 0.0};
  static GLfloat green[4] = {0.0, 1.0, 0.0, 0.0};
  static GLfloat blue[4] = {0.0, 0.0, 1.0, 0.0};
  static GLfloat white[4] = {1.0, 1.0, 1.0, 0.0};
  static GLfloat shininess = 5;

  glLightfv(GL_LIGHT0, GL_POSITION, pos);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, white);
  glLightfv(GL_LIGHT0, GL_SPECULAR, white);
  glEnable(GL_CULL_FACE);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHTING);
  glEnable(GL_DEPTH_TEST);

  gear1 = glGenLists(1);
  glNewList(gear1, GL_COMPILE);
  glMaterialfv(GL_FRONT, GL_DIFFUSE, blue);
  glMaterialfv(GL_FRONT, GL_SPECULAR, white);
  glMaterialfv(GL_FRONT, GL_SHININESS, &shininess);
  glColor3fv(blue);
  gear(1.0, 4.0, 1.0, 20, 0.7);
  glEndList();

  gear2 = glGenLists(1);
  glNewList(gear2, GL_COMPILE);
  glMaterialfv(GL_FRONT, GL_DIFFUSE, red);
  glMaterialfv(GL_FRONT, GL_SPECULAR, white);
  glColor3fv(red);
  gear(0.5, 2.0, 2.0, 10, 0.7);
  glEndList();

  gear3 = glGenLists(1);
  glNewList(gear3, GL_COMPILE);
  glMaterialfv(GL_FRONT, GL_DIFFUSE, green);
  glMaterialfv(GL_FRONT, GL_SPECULAR, white);
  glColor3fv(green);
  gear(1.3, 2.0, 0.5, 10, 0.7);
  glEndList();
}

#include <time.h>

int main() {
    int content_w = 450, content_h = 350;
    int window_width, window_height;

    desktopWindowSizeForContent(content_w, content_h, DT_WIN, &window_width, &window_height);

    int scr_w = 1280, scr_h = 800; // Expected resolution
    int win_x = (scr_w - window_width) / 2;
    int win_y = (scr_h - window_height) / 2;

    if (desktop.createWindow("TinyGL Gears", win_x, win_y, window_width, window_height, DT_WIN) < 0) {
        printf("Failed to create window\n");
        return 1;
    }

    int w = content_w;
    int h = content_h;

    ZBuffer *frameBuffer = ZB_open(w, h, ZB_MODE_RGBA, 0);
    if (!frameBuffer) {
        printf("ZB_open failed\n");
        return 1;
    }

    glInit(frameBuffer);

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glViewport(0, 0, w, h);
    glShadeModel(GL_SMOOTH);

    GLfloat aspect = (GLfloat)h / (GLfloat)w;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.0, 1.0, -aspect, aspect, 5.0, 60.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0, 0.0, -45.0);

    init_scene();
    glSetEnableSpecular(GL_TRUE);

    unsigned int *pixel_buffer = desktop.allocFramebuffer(w, h);
    if (!pixel_buffer) {
        printf("Failed to allocate desktop framebuffer\n");
        return 1;
    }

    int frame_count = 0;
    char fps_buf[32] = "FPS: 00";
    struct timespec last_time;
    clock_gettime(CLOCK_MONOTONIC, &last_time);

    while (1) {
    dt_event_t incoming_events[8];
    int event_count = desktop.pollEvents(incoming_events, 8);
    for (int i = 0; i < event_count; i++)
    {
        dt_event_t *ev = &incoming_events[i];
        if (ev->type == DT_EV_KEY && ev->keycode == 27) { // ESC
            goto cleanup;
        }
        if (ev->type == DT_EV_MOUSE) {
            ui16_input(ev->mx, ev->my, (ev->buttons & DT_BTN_LEFT) != 0);
        }
        if (ev->type == DT_EV_RESIZE && ev->width > 40 && ev->height > 40)
        {
            int nw = ev->width;
            int nh = ev->height;
            unsigned int *new_pb = desktop.resizeFramebuffer(nw, nh);

            if (new_pb)
            {
                pixel_buffer = new_pb;
                w = nw;
                h = nh;
                ZB_close(frameBuffer);
                frameBuffer = ZB_open(w, h, ZB_MODE_RGBA, 0);

                if (frameBuffer)
                {
                    glInit(frameBuffer);
                    glClearColor(0.0, 0.0, 0.0, 0.0);
                    glViewport(0, 0, w, h);
                    glShadeModel(GL_SMOOTH);
                    GLfloat aspect = (GLfloat)h / (GLfloat)w;
                    glMatrixMode(GL_PROJECTION);
                    glLoadIdentity();
                    glFrustum(-1.0, 1.0, -aspect, aspect, 5.0, 60.0);
                    glMatrixMode(GL_MODELVIEW);
                    glLoadIdentity();
                    glTranslatef(0.0, 0.0, -45.0);
                }
            }
        }
    }

    frame_count++;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    long elapsed_ms = (now.tv_sec - last_time.tv_sec) * 1000 + (now.tv_nsec - last_time.tv_nsec) / 1000000;

    if (elapsed_ms >= 500) {
        int fps = (int)((frame_count * 1000L) / elapsed_ms);
        snprintf(fps_buf, sizeof(fps_buf), "FPS: %d", fps);
        frame_count = 0;
        last_time = now;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_scene();
    ZB_copyFrameBuffer(frameBuffer, pixel_buffer, w * 4);

    for (int i = 0; i < w * h; i++) {
        pixel_buffer[i] |= 0xFF000000;
    }
    ui16_setRoot(style(width(fill), height(fill), bg(0)), pixel_buffer, w, h);

    ui16_container(
        style(
            width(fill),
            height(fill),
            padding(8),
            justify_content(spaceBetween)
        )
    ) {
        ui16_label(style(color(rgb(255, 255, 255))), "TinyGL Gears (ui16)");
        ui16_label(style(color(rgb(0, 255, 128)), font(fontBold)), fps_buf);
    }

    ui16_frame();

    desktop.presentFrame();

    yield();
    }

cleanup:
    ZB_close(frameBuffer);
    desktop.closeWindow();
    return 0;
}
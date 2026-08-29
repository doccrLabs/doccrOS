/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (c) 2026 sulfurLabs
 *
 * PROJECT: s4
 * FILE: ui16.h
 *
 */

#pragma once

typedef enum
{
    UI16_SIZE_PIXELS,
    UI16_SIZE_PERCENT,
    UI16_SIZE_FILL,
    UI16_SIZE_AUTOSIZE
} ui16_size_kind_t;

typedef struct
{
    ui16_size_kind_t kind;
    int  value;
} ui16_size_t;

typedef enum
{
    UI16_LAYOUT_ROW,
    UI16_LAYOUT_COLUMN
} ui16_layout_kind_t;

typedef enum
{
    UI16_FONT_INHERIT,
    UI16_FONT_REGULAR,
    UI16_FONT_BOLD,
    UI16_FONT_PSF
} ui16_font_kind_t;

typedef struct
{
    ui16_font_kind_t kind;
    const char *path;
} ui16_font_t;

typedef enum
{
    UI16_ALIGN_START,
    UI16_ALIGN_CENTER,
    UI16_ALIGN_END,
    UI16_ALIGN_STRETCH
} ui16_align_t;

typedef enum
{
    UI16_JUSTIFY_START,
    UI16_JUSTIFY_CENTER,
    UI16_JUSTIFY_END,
    UI16_JUSTIFY_SPACE_BETWEEN,
    UI16_JUSTIFY_SPACE_AROUND,
    UI16_JUSTIFY_SPACE_EVENLY
} ui16_justify_t;

typedef enum
{
    UI16_POSITION_STATIC,
    UI16_POSITION_ABSOLUTE
} ui16_position_t;

typedef struct
{
    ui16_size_t width;
    ui16_size_t height;

    int min_width;
    int max_width;
    int min_height;
    int max_height;

    unsigned int background;
    unsigned int color;

    int  padding;
    int  margin;
    int  gap;
    int  radius;

    int border_width;
    unsigned int border_color;

    ui16_layout_kind_t layout;
    ui16_font_t font;

    ui16_align_t align_items;
    ui16_justify_t justify_content;
    int wrap;

    ui16_position_t position;
    int left;
    int top;
    int layer;
} ui16_style_t;

typedef enum
{
    UI16_NODE_CONTAINER,
    UI16_NODE_BUTTON,
    UI16_NODE_LABEL,
    UI16_NODE_IMAGE
} ui16_node_kind_t;

typedef struct ui16_node_s
{
    ui16_node_kind_t kind;
    ui16_style_t style;
    const char *text;

    struct ui16_node_s *parent;
    struct ui16_node_s *first_child;
    struct ui16_node_s *last_child;
    struct ui16_node_s *next_sibling;

    int  box_x;
    int  box_y;
    int  box_width;
    int  box_height;

    int natural_width;
    int natural_height;

    int line_index;
} ui16_node_t;

typedef enum
{
    UI16_MOD_WIDTH,
    UI16_MOD_HEIGHT,
    UI16_MOD_MIN_WIDTH,
    UI16_MOD_MAX_WIDTH,
    UI16_MOD_MIN_HEIGHT,
    UI16_MOD_MAX_HEIGHT,
    UI16_MOD_BACKGROUND,
    UI16_MOD_COLOR,
    UI16_MOD_PADDING,
    UI16_MOD_MARGIN,
    UI16_MOD_GAP,
    UI16_MOD_RADIUS,
    UI16_MOD_BORDER_WIDTH,
    UI16_MOD_BORDER_COLOR,
    UI16_MOD_LAYOUT,
    UI16_MOD_FONT,
    UI16_MOD_ALIGN_ITEMS,
    UI16_MOD_JUSTIFY_CONTENT,
    UI16_MOD_WRAP,
    UI16_MOD_POSITION,
    UI16_MOD_LEFT,
    UI16_MOD_TOP,
    UI16_MOD_LAYER,

    UI16_MOD_END
} ui16_style_mod_kind_t;

typedef struct
{
    ui16_style_mod_kind_t kind;
    union
    {
        ui16_size_t size_value;
        unsigned int color_value;
        int  int_value;

        ui16_layout_kind_t layout_value;
        ui16_font_t font_value;
        ui16_align_t align_value;
        ui16_justify_t  justify_value;
        ui16_position_t position_value;

    } data;
} ui16_style_mod_t;

ui16_style_t ui16__defaultStyle(void);
ui16_style_t ui16__applyMods(ui16_style_t base_style, const ui16_style_mod_t *mod_list);

#define style(...) \
    ui16__applyMods(ui16__defaultStyle(), \
        (const ui16_style_mod_t[])        \
        {                                 \
            __VA_ARGS__,{                 \
                UI16_MOD_END, { 0 }       \
            }                             \
        }                                 \
    )

#define width(size_arg)     ((ui16_style_mod_t){ UI16_MOD_WIDTH,      .data.size_value = (size_arg) })
#define height(size_arg)    ((ui16_style_mod_t){ UI16_MOD_HEIGHT,     .data.size_value = (size_arg) })
#define min_width(px_arg)   ((ui16_style_mod_t){ UI16_MOD_MIN_WIDTH,  .data.int_value  = (px_arg) })
#define max_width(px_arg)   ((ui16_style_mod_t){ UI16_MOD_MAX_WIDTH,  .data.int_value  = (px_arg) })
#define min_height(px_arg)  ((ui16_style_mod_t){ UI16_MOD_MIN_HEIGHT, .data.int_value  = (px_arg) })
#define max_height(px_arg)  ((ui16_style_mod_t){ UI16_MOD_MAX_HEIGHT, .data.int_value  = (px_arg) })
#define bg(color_arg)       ((ui16_style_mod_t){ UI16_MOD_BACKGROUND, .data.color_value = (color_arg) })
#define color(color_arg)    ((ui16_style_mod_t){ UI16_MOD_COLOR,      .data.color_value = (color_arg) })
#define padding(int_arg)    ((ui16_style_mod_t){ UI16_MOD_PADDING,    .data.int_value = (int_arg) })
#define margin(int_arg)     ((ui16_style_mod_t){ UI16_MOD_MARGIN,     .data.int_value = (int_arg) })
#define gap(int_arg)        ((ui16_style_mod_t){ UI16_MOD_GAP,        .data.int_value = (int_arg) })
#define radius(int_arg)     ((ui16_style_mod_t){ UI16_MOD_RADIUS,     .data.int_value = (int_arg) })

#define border_width(px_arg) ((ui16_style_mod_t){ UI16_MOD_BORDER_WIDTH, .data.int_value = (px_arg) })
#define border_color(color_arg) ((ui16_style_mod_t){ UI16_MOD_BORDER_COLOR, .data.color_value = (color_arg) })
#define border(width_arg, color_arg) border_width(width_arg), border_color(color_arg)

#define layout(layout_arg) ((ui16_style_mod_t){ UI16_MOD_LAYOUT, .data.layout_value = (layout_arg) })
#define font(v) ((ui16_style_mod_t){ UI16_MOD_FONT, .data.font_value = (v) })

// font modes like psf (in future maybe ttf ._. )
#define fontPsf(path_arg) ((ui16_font_t){ UI16_FONT_PSF, (path_arg) })

#define align_items(v) ((ui16_style_mod_t){ UI16_MOD_ALIGN_ITEMS, .data.align_value = (v) })
#define justify_content(v) ((ui16_style_mod_t){ UI16_MOD_JUSTIFY_CONTENT, .data.justify_value = (v) })
#define wrap(v) ((ui16_style_mod_t){ UI16_MOD_WRAP, .data.int_value = (v) })

/* position/overlay */
#define position(v) ((ui16_style_mod_t){ UI16_MOD_POSITION, .data.position_value = (v) })
#define left(px_arg) ((ui16_style_mod_t){ UI16_MOD_LEFT, .data.int_value = (px_arg) })
#define top(px_arg) ((ui16_style_mod_t){ UI16_MOD_TOP, .data.int_value = (px_arg) })
#define layer(int_arg) ((ui16_style_mod_t){ UI16_MOD_LAYER, .data.int_value = (int_arg) })

/* sizes */
#define px(pixel_amount) ((ui16_size_t){ UI16_SIZE_PIXELS, (pixel_amount) })
#define percent(percent_amount) ((ui16_size_t){ UI16_SIZE_PERCENT, (percent_amount) })

#define fill     ((ui16_size_t){ UI16_SIZE_FILL, 0 })
#define autosize ((ui16_size_t){ UI16_SIZE_AUTOSIZE, 0 })

#define row    UI16_LAYOUT_ROW
#define column UI16_LAYOUT_COLUMN

#define fontInherit ((ui16_font_t){ UI16_FONT_INHERIT, 0 })
#define fontRegular ((ui16_font_t){ UI16_FONT_REGULAR, 0 })
#define fontBold    ((ui16_font_t){ UI16_FONT_BOLD, 0 })

#define alignStart   UI16_ALIGN_START
#define alignCenter  UI16_ALIGN_CENTER
#define alignEnd     UI16_ALIGN_END
#define alignStretch UI16_ALIGN_STRETCH

#define justifyStart UI16_JUSTIFY_START
#define justifyCenter UI16_JUSTIFY_CENTER
#define justifyEnd UI16_JUSTIFY_END

#define spaceBetween UI16_JUSTIFY_SPACE_BETWEEN
#define spaceAround UI16_JUSTIFY_SPACE_AROUND
#define spaceEvenly UI16_JUSTIFY_SPACE_EVENLY

#define positionStatic  UI16_POSITION_STATIC
#define positionAbsolute UI16_POSITION_ABSOLUTE

#define wrapEnabled 1
#define wrapDisabled 0

#define rgb(             \
            red_value,   \
            green_value, \
            blue_value   \
        ) (\
            0xFF000000u |\
            ((unsigned int)(red_value) << 16)  |\
            ((unsigned int)(green_value) << 8) |\
            (unsigned int)(blue_value)          \
        )

ui16_node_t *ui16__setRootStyled(ui16_style_t root_style, unsigned int *target_buffer, int buffer_width, int buffer_height);
ui16_node_t *ui16__setRootDefault(unsigned int *target_buffer, int buffer_width, int buffer_height);

#define UI16_ARG4(first_arg, second_arg, third_arg, fourth_arg, name_arg, ...) name_arg

#define ui16_setRoot(...) \
    UI16_ARG4(\
        __VA_ARGS__,                        \
        ui16__setRootStyled,                \
        ui16__setRootDefault)(__VA_ARGS__)

void ui16_frame(void);

ui16_node_t *ui16__containerBegin(ui16_style_t container_style);
void ui16__containerEnd(void);

#define ui16_container(style_arg) \
    for (ui16_node_t *ui16_loop_node = ui16__containerBegin(style_arg); \
         ui16_loop_node;     \
         ui16_loop_node =    \
         (ui16__containerEnd(), (ui16_node_t *)0))

ui16_node_t *ui16__labelSimple(const char *label_text);
ui16_node_t *ui16__labelStyled(ui16_style_t label_style, const char *label_text);

#define UI16_ARG2(first_arg, second_arg, name_arg, ...) name_arg

#define ui16_label(...) UI16_ARG2(\
                                __VA_ARGS__, \
                            ui16__labelStyled, \
                            ui16__labelSimple) \
                        (__VA_ARGS__)

void ui16_input(int mouse_x, int mouse_y, int mouse_down);

typedef struct bmp_image_s bmp_image_t;

void ui16_drawBmp(const char *path, int x, int y);
void ui16_drawBmpScaled(const char *path, int x, int y, int w, int h);

ui16_node_t *ui16__imageSimple(const char *path);
ui16_node_t *ui16__imageStyled(ui16_style_t image_style, const char *path);

#define ui16_image(...) UI16_ARG2(\
                            __VA_ARGS__, \
                            ui16__imageStyled, \
                            ui16__imageSimple) \
                            (__VA_ARGS__)

int ui16_hovered(const ui16_node_t *node);
int ui16_pressed(const ui16_node_t *node);
int ui16_clicked(const ui16_node_t *node);

char ui16_keyToChar(unsigned int keycode, int shift);
#include <ui16.h>
#include <ui16buttons.h>
#include <libdesktop.h>
#include <unistd.h>
#include <stdio.h>

#define UI16DEMO_CONTENT_WIDTH  420
#define UI16DEMO_CONTENT_HEIGHT 260

static int click_count = 0;
static char click_label[32] = "clicks: 0";

static void update_click_label(void)
{
    snprintf(click_label, sizeof(click_label), "clicks: %d", click_count);
}

int main(void)
{
    int window_width;
    int window_height;
    #define APP_TITLE "ui16 demo"

    desktopWindowSizeForContent(
        UI16DEMO_CONTENT_WIDTH,
        UI16DEMO_CONTENT_HEIGHT,
        DT_WIN,

        &window_width,
        &window_height
    );

    desktop.createWindow(
        APP_TITLE,

        150,   150,
        window_width,
        window_height,

        DT_WIN
    );

    int content_w = UI16DEMO_CONTENT_WIDTH;
    int content_h = UI16DEMO_CONTENT_HEIGHT;

    unsigned int *window_buffer = desktop.allocFramebuffer(content_w, content_h);

    update_click_label();

    for (;;)
    {
        ui16_setRoot(
            style(
                width(fill),
                height(fill),
                bg(rgb(64, 64, 64))
            ),
            window_buffer,
            content_w,
            content_h
        );

        ui16_node_t *save_btn = 0;
        ui16_node_t *reset_btn = 0;

        ui16_container(
            style(
                layout(row),
                width(fill),
                height(fill)
            )
        ) {
            // sidebar sizes itself to its content, but not smaller/bigger than this
            ui16_container(
                style(
                    layout(column),
                    width(autosize),
                    min_width(90),
                    max_width(160),
                    height(fill),
                    bg(rgb(30, 30, 30)),
                    padding(8),
                    gap(6),
                    font(fontBold)
                )
            ) {
                ui16_button("Settings");
                ui16_button("Save");
                ui16_label(style(font(fontBold)), "font test");
            };

            ui16_container(
                style(
                    layout(column),
                    width(fill),
                    height(fill),
                    padding(8),
                    gap(6)
                )
            ) {
                ui16_label(style(font(fontBold)), "Hello from ui16");

                // shrink the window to see this wrap onto more than one row
                ui16_container(
                    style(
                        layout(row),
                        width(fill),
                        height(autosize),
                        gap(4),
                        wrap(wrapEnabled)
                    )
                ) {
                    ui16_label(style(margin(2), padding(4), bg(rgb(60, 60, 60))), "tag-a");
                    ui16_label(style(margin(2), padding(4), bg(rgb(60, 60, 60))), "tag-b");
                    ui16_label(style(margin(2), padding(4), bg(rgb(60, 60, 60))), "tag-c");
                    ui16_label(style(margin(2), padding(4), bg(rgb(60, 60, 60))), "tag-d");
                    ui16_label(style(margin(2), padding(4), bg(rgb(60, 60, 60))), "tag-e");
                };

                ui16_container(
                    style(
                        layout(row),
                        width(fill),
                        height(px(40)),
                        justify_content(spaceBetween),
                        align_items(alignCenter),
                        bg(rgb(45, 45, 45)),
                        padding(4)
                    )
                ) {
                    save_btn = ui16_button(style(margin(2)), "Save");
                    reset_btn = ui16_button(style(margin(2)), "Reset");
                    ui16_label(style(color(rgb(200, 200, 200))), click_label);
                };
            };
        }

        // absolute overlay, floats on top of everything else, its own layer
        ui16_container(
            style(
                position(positionAbsolute),
                left(content_w - 70),
                top(6),
                width(px(60)),
                height(px(18)),
                bg(rgb(180, 60, 60)),
                radius(3),
                layer(10)
            )
        ) {
            ui16_label(style(color(rgb(255, 255, 255))), "overlay");
        };

        ui16_frame();

        desktop.presentFrame();

        dt_event_t incoming_events[8];
        int event_count = desktop.pollEvents(incoming_events, 8);

        for (int i = 0; i < event_count; i++)
        {
            dt_event_t *ev = &incoming_events[i];
            if (ev->type == DT_EV_MOUSE)
            {
                ui16_input(ev->mx, ev->my, (ev->buttons & DT_BTN_LEFT) != 0);
            }
            if (ev->type == DT_EV_RESIZE && ev->width > 40 && ev->height > 40)
            {
                content_w = ev->width;
                content_h = ev->height;
                unsigned int *new_buf = desktop.resizeFramebuffer(content_w, content_h);
                if (new_buf) window_buffer = new_buf;
            }
        }

        if (save_btn && ui16_clicked(save_btn))
        {
            click_count++;
            update_click_label();
            printf("[ui16demo] save clicked (%d)\n", click_count);
        }

        if (reset_btn && ui16_clicked(reset_btn))
        {
            click_count = 0;
            update_click_label();
            printf("[ui16demo] reset clicked\n");
        }

        yield();
    }

    return 0;
}
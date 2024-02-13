use macroquad::color::Color;
use macroquad::prelude::*;

use crate::graphics::Viewport2D;

pub async fn run() {
    let clear_color = Color::from_hex(0x333333);
    let map_viewport = Viewport2D::new(Vec2::ZERO, vec2(screen_width(), screen_height()), 1.);
    loop {
        let dt = get_frame_time();

        //draw
        {
            clear_background(clear_color);

            map_viewport.draw_rectangle(vec2(0., 0.), map_viewport.size, BLACK);

            let rect_size = 50.;
            map_viewport.draw_rectangle(
                vec2(
                    -map_viewport.size.x * 0.5 + rect_size * 0.5,
                    map_viewport.size.y * 0.5 - rect_size * 0.5,
                ),
                vec2(rect_size, rect_size),
                BLUE,
            );
            map_viewport.draw_rectangle(vec2(0., 0.), vec2(rect_size, rect_size), RED);
            map_viewport.draw_rectangle(
                vec2(
                    map_viewport.size.x * 0.5 - rect_size * 0.5,
                    -map_viewport.size.y * 0.5 + rect_size * 0.5,
                ),
                vec2(rect_size, rect_size),
                GREEN,
            );
        }
        next_frame().await;
    }
}

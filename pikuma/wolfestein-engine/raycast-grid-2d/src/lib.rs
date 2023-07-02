pub mod math {
    use macroquad::prelude::*;

    pub fn polar_to_cartesian(r: f32, theta_radians: f32) -> Vec2 {
        vec2(theta_radians.cos() * r, theta_radians.sin() * r)
    }
}

pub mod graphics {
    use macroquad::prelude::{vec2, Vec2};

    pub struct Viewport2D {
        pub size: Vec2,
    }

    impl Viewport2D {
        pub fn world_to_viewport(&self, pos: Vec2) -> Vec2 {
            vec2(pos.x + self.size.x * 0.5, -pos.y + self.size.y * 0.5)
        }
    }
}

pub mod game {
    use std::fs::read_to_string;

    use macroquad::{miniquad::gl::WGL_CONTEXT_MAJOR_VERSION_ARB, prelude::*};

    use crate::{graphics::Viewport2D, math};

    pub struct Map {
        pub grid: Vec<Vec<u8>>,
        pub row_count: usize,
        pub column_count: usize,
        pub tile_size: usize,
    }

    impl Map {
        pub fn width(&self) -> u16 {
            (self.column_count * self.tile_size) as u16
        }

        pub fn height(&self) -> u16 {
            (self.row_count * self.tile_size) as u16
        }

        pub fn tile_center(&self, x: usize, y: usize) -> Vec2 {
            let map_pos = Vec2::ZERO;
            let tile_size = self.tile_size as f32;
            let extents = vec2(self.width() as f32 * 0.5, self.height() as f32 * 0.5);

            let tile_x = map_pos.x + tile_size * x as f32 - extents.x + tile_size * 0.5;

            let tile_y = map_pos.y + tile_size * y as f32 - extents.y + tile_size * 0.5;

            vec2(tile_x, tile_y)
        }
    }

    pub struct Player {
        pub radius: f32,
        pub color: Color,

        pub position: Vec2,
        pub rotation_radians: f32,

        pub move_speed: f32,
        pub rotation_speed_radians: f32,
    }

    pub async fn run() {
        let viewport = Viewport2D {
            size: vec2(screen_width(), screen_height()),
        };
        let map = create_map();

        let mut player = Player {
            radius: 10.0,
            color: RED,
            position: Vec2::ZERO,
            rotation_radians: 15.0_f32.to_radians(),
            move_speed: 100.0,
            rotation_speed_radians: 80.0_f32.to_radians(),
        };

        loop {
            clear_background(GRAY);
            update_player(&mut player, get_frame_time());

            draw_map(&map, &viewport);
            draw_player(&player, &viewport);

            next_frame().await;
        }
    }

    fn update_player(player: &mut Player, dt: f32) {
        //move
        {
            let mut move_input = 0.0;
            if is_key_down(KeyCode::W) {
                move_input += 1.0;
            }
            if is_key_down(KeyCode::S) {
                move_input -= 1.0;
            }

            let move_amount = player.move_speed * move_input * dt;
            let move_vector = math::polar_to_cartesian(move_amount, player.rotation_radians);

            player.position += move_vector;
        }

        //rotate
        {
            let mut rot_input = 0.0;
            if is_key_down(KeyCode::A) {
                rot_input += 1.0;
            }
            if is_key_down(KeyCode::D) {
                rot_input -= 1.0;
            }

            let rot_amount = player.rotation_speed_radians * rot_input * dt;
            player.rotation_radians += rot_amount;
        }
    }

    fn draw_player(player: &Player, viewport: &Viewport2D) {
        let player_draw_pos = viewport.world_to_viewport(player.position);
        let line_end = player.position + math::polar_to_cartesian(50.0, player.rotation_radians);
        let line_end_draw = viewport.world_to_viewport(line_end);
        draw_line(
            player_draw_pos.x,
            player_draw_pos.y,
            line_end_draw.x,
            line_end_draw.y,
            2.0,
            BLUE,
        );

        draw_circle(
            player_draw_pos.x,
            player_draw_pos.y,
            player.radius,
            player.color,
        );
    }

    fn create_map() -> Map {
        let grid: Vec<Vec<u8>> = vec![
            vec![1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
            vec![1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1],
            vec![1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1],
            vec![1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1],
            vec![1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1],
            vec![1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1],
            vec![1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
            vec![1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
            vec![1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1],
            vec![1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
            vec![1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
        ];

        let row_count = grid.len();
        let column_count = grid[0].len();

        Map {
            grid,
            row_count,
            column_count,
            tile_size: 32,
        }
    }

    fn draw_map(map: &Map, viewport: &Viewport2D) {
        for (y, line) in map.grid.iter().enumerate() {
            for (x, cell) in line.iter().enumerate() {
                let tile_size: f32 = map.tile_size as f32;

                let tile_center = map.tile_center(x, y);
                let tile_top_left_viewport =
                    viewport.world_to_viewport(tile_center + vec2(-tile_size, tile_size) * 0.5);

                match cell {
                    0 => {
                        draw_rectangle(
                            tile_top_left_viewport.x,
                            tile_top_left_viewport.y,
                            tile_size,
                            tile_size,
                            WHITE,
                        );
                        draw_rectangle_lines(
                            tile_top_left_viewport.x,
                            tile_top_left_viewport.y,
                            tile_size,
                            tile_size,
                            1.0,
                            BLACK,
                        );
                    }
                    1 => {
                        draw_rectangle(
                            tile_top_left_viewport.x,
                            tile_top_left_viewport.y,
                            tile_size,
                            tile_size,
                            BLACK,
                        );
                    }
                    _ => (),
                };
            }
        }
    }
}

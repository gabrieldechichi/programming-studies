pub mod math {
    use macroquad::prelude::*;

    pub fn polar_to_cartesian(r: f32, theta_radians: f32) -> Vec2 {
        vec2(theta_radians.cos() * r, theta_radians.sin() * r)
    }
}

pub mod graphics {
    use macroquad::prelude::*;

    pub struct Viewport2D {
        pub size: Vec2,
    }

    impl Viewport2D {
        pub fn world_to_viewport(&self, pos: Vec2) -> Vec2 {
            vec2(pos.x + self.size.x * 0.5, -pos.y + self.size.y * 0.5)
        }

        pub fn draw_rectangle(&self, center: Vec2, size: Vec2, color: Color) {
            let draw_pos = self.world_to_viewport(Viewport2D::rect_top_left(center, size));
            draw_rectangle(draw_pos.x, draw_pos.y, size.x, size.y, color);
        }

        pub fn draw_rectangle_lines(&self, center: Vec2, size: Vec2, thickness: f32, color: Color) {
            let draw_pos = self.world_to_viewport(Viewport2D::rect_top_left(center, size));
            draw_rectangle_lines(draw_pos.x, draw_pos.y, size.x, size.y, thickness, color);
        }

        pub fn draw_line(&self, from: Vec2, to: Vec2, thickness: f32, color: Color) {
            let from_draw = self.world_to_viewport(from);
            let to_draw = self.world_to_viewport(to);
            draw_line(
                from_draw.x,
                from_draw.y,
                to_draw.x,
                to_draw.y,
                thickness,
                color,
            );
        }

        pub fn draw_circle(&self, center: Vec2, r: f32, color: Color) {
            let center_draw = self.world_to_viewport(center);
            draw_circle(center_draw.x, center_draw.y, r, color);
        }

        fn rect_top_left(center: Vec2, size: Vec2) -> Vec2 {
            center + vec2(-size.x, size.y) * 0.5
        }
    }
}

pub mod game {

    use macroquad::prelude::*;

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

        pub fn get_tile(&self, x: usize, y: usize) -> u8 {
            self.grid[y][x]
        }

        pub fn tile_size(&self) -> Vec2 {
            vec2(self.tile_size as f32, self.tile_size as f32)
        }

        pub fn tile_center(&self, x: usize, y: usize) -> Vec2 {
            let map_pos = Vec2::ZERO;
            let tile_size = self.tile_size as f32;
            let extents = vec2(self.width() as f32 * 0.5, self.height() as f32 * 0.5);

            let tile_x = map_pos.x + tile_size * x as f32 - extents.x + tile_size * 0.5;

            let tile_y = map_pos.y + tile_size * y as f32 - extents.y + tile_size * 0.5;

            vec2(tile_x, tile_y)
        }

        pub fn position_to_coords(&self, pos: Vec2) -> (usize, usize) {
            let map_pos = Vec2::ZERO;
            let extents = vec2(self.width() as f32 * 0.5, self.height() as f32 * 0.5);
            let tile_size = self.tile_size as f32;
            let x = ((pos.x - map_pos.x + extents.x - tile_size * 0.5) / tile_size).round() as i32;
            let y = ((pos.y - map_pos.y + extents.y - tile_size * 0.5) / tile_size).round() as i32;

            (x as usize, y as usize)
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
            let dt = get_frame_time();

            draw_map(&map, &viewport);

            draw_player(&player, &viewport);

            update_player(&mut player, &map, dt);

            next_frame().await;
        }
    }

    fn update_player(player: &mut Player, map: &Map, dt: f32) {
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

            let new_pos = player.position + move_vector;

            let mut can_move = true;
            const SKIN_WIDTH: f32 = 3.0;
            for deg in [45.0, 180.0 - 45.0, 180.0 + 45.0, -45.0] {
                let radians = (deg as f32).to_radians();
                let point = new_pos + math::polar_to_cartesian(player.radius + SKIN_WIDTH, radians);

                let tile_coors = map.position_to_coords(point);
                let mut c = GREEN;
                c.a = 0.5;

                let tile = map.get_tile(tile_coors.0, tile_coors.1);
                if tile == 1 {
                    can_move = false;
                    break;
                }
            }

            if can_move {
                player.position = new_pos;
            }
        }
    }

    fn draw_player(player: &Player, viewport: &Viewport2D) {
        let line_end = player.position + math::polar_to_cartesian(50.0, player.rotation_radians);

        viewport.draw_line(player.position, line_end, 2.0, BLUE);

        viewport.draw_circle(player.position, player.radius, player.color);
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
                let tile_size = vec2(map.tile_size as f32, map.tile_size as f32);

                let tile_center = map.tile_center(x, y);
                match cell {
                    0 => {
                        viewport.draw_rectangle(tile_center, tile_size, WHITE);
                        viewport.draw_rectangle_lines(tile_center, tile_size, 1.0, BLACK);
                    }
                    1 => {
                        viewport.draw_rectangle(tile_center, tile_size, BLACK);
                    }
                    _ => (),
                };
            }
        }
    }
}

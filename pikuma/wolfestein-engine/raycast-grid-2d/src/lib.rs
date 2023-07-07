pub mod math;

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

pub mod grid {
    use macroquad::prelude::*;

    #[derive(Debug, Clone, Copy)]
    pub struct GridCoords {
        pub x: i32,
        pub y: i32,
    }

    impl std::ops::Add<GridCoords> for GridCoords {
        type Output = GridCoords;

        fn add(self, rhs: GridCoords) -> Self::Output {
            GridCoords {
                x: self.x + rhs.x,
                y: self.y + rhs.y,
            }
        }
    }

    pub fn coords(x: i32, y: i32) -> GridCoords {
        GridCoords { x, y }
    }

    pub fn tile_center(x: i32, y: i32, tile_size: f32) -> Vec2 {
        tile_bl(x, y, tile_size) + tile_size * 0.5
    }

    pub fn tile_bl(x: i32, y: i32, tile_size: f32) -> Vec2 {
        let tile_x = tile_size * x as f32;
        let tile_y = tile_size * y as f32;
        vec2(tile_x, tile_y)
    }

    pub fn position_to_coords(pos: Vec2, tile_size: f32) -> GridCoords {
        let x = (pos.x / tile_size).floor() as i32;
        let y = (pos.y / tile_size).floor() as i32;
        GridCoords { x, y }
    }
}

pub mod game {

    use crate::{graphics::Viewport2D, grid::*, math};
    use macroquad::prelude::*;

    #[derive(Debug, Clone)]
    pub struct Level {
        pub grid: Vec<Vec<u8>>,
        pub pivot: Vec2,
        pub row_count: usize,
        pub column_count: usize,
        pub tile_size: usize,
    }

    impl Level {
        pub fn width(&self) -> u16 {
            (self.column_count * self.tile_size) as u16
        }

        pub fn height(&self) -> u16 {
            (self.row_count * self.tile_size) as u16
        }

        pub fn extents(&self) -> Vec2 {
            vec2(self.width() as f32 * 0.5, self.height() as f32 * 0.5)
        }

        pub fn get_tile(&self, x: usize, y: usize) -> Option<u8> {
            if x >= self.column_count || y >= self.row_count {
                None
            } else {
                Some(self.grid[y][x])
            }
        }

        pub fn is_wall(&self, x: usize, y: usize) -> bool {
            if let Some(t) = self.get_tile(x, y) {
                t == 1
            } else {
                false
            }
        }

        pub fn tile_size(&self) -> Vec2 {
            vec2(self.tile_size as f32, self.tile_size as f32)
        }

        pub fn tile_center(&self, x: usize, y: usize) -> Vec2 {
            tile_center(x as i32, y as i32, self.tile_size as f32) + self.pivot - self.extents()
        }

        pub fn tile_bl(&self, x: usize, y: usize) -> Vec2 {
            tile_bl(x as i32, y as i32, self.tile_size as f32) + self.pivot - self.extents()
        }

        pub fn position_to_coords(&self, pos: Vec2) -> GridCoords {
            let adjusted_pos = pos + self.extents() - self.pivot;
            position_to_coords(adjusted_pos, self.tile_size as f32)
        }
    }

    pub struct Player {
        pub radius: f32,
        pub color: Color,

        pub position: Vec2,
        pub rotation_radians: f32,
        pub fov: PlayerFov,

        pub move_speed: f32,
        pub rotation_speed_radians: f32,
    }

    pub struct PlayerFov {
        pub ray_count: usize,
        pub half_fov_radians: f32,
    }

    impl PlayerFov {
        pub fn dtheta(&self) -> f32 {
            self.half_fov_radians * 2 as f32 / self.ray_count as f32
        }
    }

    pub async fn run() {
        let viewport = Viewport2D {
            size: vec2(screen_width(), screen_height()),
        };
        let level = create_level();

        let mut player = Player {
            radius: 10.0,
            color: RED,
            position: Vec2::ZERO,
            fov: PlayerFov {
                ray_count: 100,
                half_fov_radians: 30.0_f32.to_radians(),
            },
            rotation_radians: (180.0_f32 - 30.0_f32).to_radians(),
            move_speed: 100.0,
            rotation_speed_radians: 80.0_f32.to_radians(),
        };

        loop {
            clear_background(GRAY);
            let dt = get_frame_time();

            draw_map(&level, &viewport);

            draw_player_fov(&player, &level, &viewport);
            draw_player(&player, &viewport);

            update_player(&mut player, dt);
            player_map_collision(&mut player, &level, &viewport);

            next_frame().await;
        }
    }

    fn draw_player_fov(player: &Player, level: &Level, viewport: &Viewport2D) {
        let dtheta = player.fov.dtheta();
        let mut color = PURPLE;
        color.a = 0.5;

        for i in 0..player.fov.ray_count {
            let theta = player.rotation_radians - player.fov.half_fov_radians + dtheta * i as f32;
            let distance = raycast_grid(level, player.position, theta);

            let hit_point = player.position + math::polar_to_cartesian(distance, theta);
            viewport.draw_line(player.position, hit_point, 2.0, color);
        }
    }

    fn raycast_grid(level: &Level, origin: Vec2, theta: f32) -> f32 {
        let origin_coords = level.position_to_coords(origin);

        let theta_tan = theta.tan();
        //horizontal
        let h_intersect_dist = {
            let y_sign = math::normalize_angle(theta) < math::PI;

            let start_coords = if y_sign {
                origin_coords + coords(0, 1)
            } else {
                origin_coords
            };

            let mut y_intersect = level
                .tile_bl(start_coords.x as usize, start_coords.y as usize)
                .y;
            let mut x_intersect = origin.x + (y_intersect - origin.y) / theta_tan;

            let y_step = if y_sign {
                level.tile_size as f32
            } else {
                -(level.tile_size as f32)
            };

            let x_step = y_step / theta_tan;

            let p_extra = if y_sign {
                vec2(0.001, 0.001)
            } else {
                -vec2(0.001, 0.001)
            };

            loop {
                let coords = level.position_to_coords(vec2(x_intersect, y_intersect) + p_extra);

                if let Some(t) = level.get_tile(coords.x as usize, coords.y as usize) {
                    if t == 1 {
                        break;
                    }
                    x_intersect += x_step;
                    y_intersect += y_step;
                } else {
                    break;
                }
            }

            vec2(x_intersect, y_intersect).distance(origin)
        };

        //vertical
        let v_intersect_dist = {
            let x_sign = !(math::normalize_angle(theta) > math::PI * 0.5
                && math::normalize_angle(theta) < math::PI * 1.5);

            let start_coords = if x_sign {
                origin_coords + coords(1, 0)
            } else {
                origin_coords
            };

            let mut x_intersect = level
                .tile_bl(start_coords.x as usize, start_coords.y as usize)
                .x;

            let mut y_intersect = origin.y + (x_intersect - origin.x) * theta_tan;

            let x_step = if x_sign {
                level.tile_size as f32
            } else {
                -(level.tile_size as f32)
            };

            let y_step = x_step * theta_tan;

            let p_extra = if x_sign {
                vec2(0.001, 0.001)
            } else {
                -vec2(0.001, 0.001)
            };

            loop {
                let coords = level.position_to_coords(vec2(x_intersect, y_intersect) + p_extra);

                if let Some(t) = level.get_tile(coords.x as usize, coords.y as usize) {
                    if t == 1 {
                        break;
                    }
                    x_intersect += x_step;
                    y_intersect += y_step;
                } else {
                    break;
                }
            }

            vec2(x_intersect, y_intersect).distance(origin)
        };

        if h_intersect_dist > v_intersect_dist {
            v_intersect_dist
        } else {
            h_intersect_dist
        }
    }

    fn update_player(player: &mut Player, dt: f32) {
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

            player.position = player.position + move_vector;
        }
    }

    fn player_map_collision(player: &mut Player, map: &Level, viewport: &Viewport2D) {
        let current_tile_coords = map.position_to_coords(player.position);

        for coords in [
            coords(
                current_tile_coords.x as i32 + 1,
                current_tile_coords.y as i32 + 1,
            ),
            coords(
                current_tile_coords.x as i32 + 1,
                current_tile_coords.y as i32,
            ),
            coords(
                current_tile_coords.x as i32,
                current_tile_coords.y as i32 + 1,
            ),
            coords(
                current_tile_coords.x as i32 - 1,
                current_tile_coords.y as i32,
            ),
            coords(
                current_tile_coords.x as i32,
                current_tile_coords.y as i32 - 1,
            ),
            coords(
                current_tile_coords.x as i32 - 1,
                current_tile_coords.y as i32 - 1,
            ),
            coords(
                current_tile_coords.x as i32 + 1,
                current_tile_coords.y as i32 - 1,
            ),
            coords(
                current_tile_coords.x as i32 - 1,
                current_tile_coords.y as i32 + 1,
            ),
        ] {
            let mut c = GREEN;
            c.a = 0.5;
            let tile_center = map.tile_center(coords.x as usize, coords.y as usize);
            viewport.draw_rectangle(tile_center, map.tile_size(), c);

            if map.is_wall(coords.x as usize, coords.y as usize) {
                let penetration = math::circle_aabb_penetration(
                    player.position,
                    player.radius,
                    tile_center,
                    map.tile_size() * 0.5,
                );

                player.position -= penetration;
            }
        }
    }

    fn draw_player(player: &Player, viewport: &Viewport2D) {
        viewport.draw_circle(player.position, player.radius, player.color);
    }

    fn create_level() -> Level {
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

        Level {
            grid,
            pivot: Vec2::ZERO,
            row_count,
            column_count,
            tile_size: 32,
        }
    }

    fn draw_map(map: &Level, viewport: &Viewport2D) {
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

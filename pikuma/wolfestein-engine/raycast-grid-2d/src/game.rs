use crate::{graphics::Viewport2D, grid::*, math};
use macroquad::prelude::*;

const LB_FREE: u8 = 0;
const LB_WALL_RED: u8 = 1;
const LB_WALL_BLUE: u8 = 2;
const LB_WALL_GREEN: u8 = 3;

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

    pub fn tile_to_color(tile: u8) -> Color {
        match tile {
            LB_FREE => WHITE,
            LB_WALL_BLUE => BLUE,
            LB_WALL_GREEN => GREEN,
            LB_WALL_RED => RED,
            _ => BLACK,
        }
    }
    pub fn get_tile_color(&self, x: usize, y: usize) -> Color {
        let t = match self.get_tile(x, y) {
            Some(t) => t,
            None => 0,
        };
        Level::tile_to_color(t)
    }

    pub fn is_wall(&self, x: usize, y: usize) -> bool {
        if let Some(t) = self.get_tile(x, y) {
            t > 0
        } else {
            true
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

    fn raycast_grid(&self, origin: Vec2, theta: f32) -> RaycastResult {
        let adjusted_pos = origin + self.extents() - self.pivot;
        raycast_grid(adjusted_pos, theta, self.tile_size as f32, |x, y| {
            self.is_wall(x as usize, y as usize)
        })
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
    let level = create_level();
    let minimap_viewport = Viewport2D {
        size: vec2(screen_width(), screen_height()),
        pivot: -vec2(screen_width(), screen_height()) * 0.5
            + vec2(level.width() as f32, level.height() as f32) * 0.5 * 0.4,
        scale: 0.4,
    };

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

    let clear_color = Color::from_hex(0x333333);

    loop {
        clear_background(clear_color);
        let dt = get_frame_time();

        //simulation
        {
            update_player(&mut player, dt);
            player_map_collision(&mut player, &level);
        }

        //render 3d level
        {
            draw_3d_level(&player, &level);
        }

        //render minimap
        {
            draw_map(&level, &minimap_viewport);
            draw_player_fov(&player, &level, &minimap_viewport);
            draw_player(&player, &minimap_viewport);
        }

        next_frame().await;
    }
}

fn draw_3d_level(player: &Player, level: &Level) {
    let screen_width = screen_width();
    let screen_height = screen_height();
    let wall_width_proj = 1.0; //1 pixel

    let half_fov = player.fov.half_fov_radians;
    let ray_count = (screen_width / wall_width_proj) as i32;
    let dtheta = half_fov * 2.0 / ray_count as f32;
    let wall_height = level.tile_size as f32;
    let dist_proj_plane = screen_width * 0.5 / half_fov.tan();
    let shadow_decay_factor = 120.0;

    for i in 0..ray_count {
        let theta = player.rotation_radians + player.fov.half_fov_radians - dtheta * i as f32;

        let hit = level.raycast_grid(player.position, theta);

        // this corrects or fish-eye lens effect, by projecting the ray into a circle
        // no need to use cos().abs() here as we are always in the cos+ range
        let corrected_dist = hit.distance * (player.rotation_radians - theta).cos();

        let wall_heigh_proj = dist_proj_plane * wall_height / corrected_dist;

        let mut color_factor = (shadow_decay_factor / corrected_dist).min(1.0);
        // let mut color_factor = 1.0;
        color_factor *= if hit.is_horizontal_hit { 0.8 } else { 1.0 };

        let tile_color = level.get_tile_color(hit.coords.x as usize, hit.coords.y as usize);

        // let col = (tile_color * color_factor) as u8;
        let col = Color::from_vec(tile_color.to_vec() * color_factor);

        draw_rectangle(
            i as f32 * wall_width_proj,
            (screen_height - wall_heigh_proj) * 0.5,
            wall_width_proj,
            wall_heigh_proj,
            col,
        );
    }
}

fn draw_player_fov(player: &Player, level: &Level, viewport: &Viewport2D) {
    let dtheta = player.fov.dtheta();
    let mut color = PURPLE;
    color.a = 0.5;

    for i in 0..player.fov.ray_count {
        let theta = player.rotation_radians - player.fov.half_fov_radians + dtheta * i as f32;
        let hit = level.raycast_grid(player.position, theta);

        let hit_point = player.position + math::polar_to_cartesian(hit.distance, theta);
        viewport.draw_line(player.position, hit_point, 2.0, color);
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

fn player_map_collision(player: &mut Player, map: &Level) {
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
        vec![1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
        vec![1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
        vec![1, 0, 0, 1, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 1],
        vec![1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1],
        vec![1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 1],
        vec![1, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 1],
        vec![1, 0, 3, 1, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 1],
        vec![1, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 3, 3, 1],
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
            if *cell == 0 {
                viewport.draw_rectangle(tile_center, tile_size, WHITE);
                viewport.draw_rectangle_lines(tile_center, tile_size, 1.0, BLACK);
            } else {
                let c = Level::tile_to_color(*cell);
                viewport.draw_rectangle(tile_center, tile_size, c);
            }
        }
    }
}

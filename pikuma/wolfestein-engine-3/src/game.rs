use crate::*;
use macroquad::color::Color;
use macroquad::prelude::*;

mod grid {
    use macroquad::prelude::*;
    use std::ops::Add;

    pub struct GridCoords {
        pub x: i32,
        pub y: i32,
    }

    impl Add<GridCoords> for GridCoords {
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

    pub fn tile_bl(x: i32, y: i32, tile_size: f32) -> Vec2 {
        vec2(x as f32 * tile_size, y as f32 * tile_size)
    }

    pub fn tile_center(x: i32, y: i32, tile_size: f32) -> Vec2 {
        tile_bl(x, y, tile_size) + vec2(tile_size, tile_size) * 0.5
    }

    pub fn pos_to_coords(pos: Vec2, tile_size: f32) -> GridCoords {
        let x = (pos.x / tile_size).floor() as i32;
        let y = (pos.y / tile_size).floor() as i32;
        coords(x, y)
    }
}

mod level {
    use crate::graphics::Viewport2D;
    use macroquad::prelude::*;

    use super::grid;

    #[derive(PartialEq, Clone, Copy, Debug)]
    pub enum LevelBlock {
        Free = 0,
        Red = 1,
        Blue = 2,
        Green = 3,
        MAX,
    }

    impl LevelBlock {
        pub fn color(&self) -> Color {
            match self {
                LevelBlock::Free => WHITE,
                LevelBlock::Red => RED,
                LevelBlock::Blue => BLUE,
                LevelBlock::Green => GREEN,
                LevelBlock::MAX => RED,
            }
        }
    }

    impl From<&u8> for LevelBlock {
        fn from(value: &u8) -> Self {
            match value {
                0 => LevelBlock::Free,
                1 => LevelBlock::Red,
                2 => LevelBlock::Blue,
                3 => LevelBlock::Green,
                _ => LevelBlock::MAX,
            }
        }
    }

    pub struct Level {
        pub grid: Vec<Vec<LevelBlock>>,
        pub tile_size: f32,
    }

    impl Level {
        pub fn row_count(&self) -> usize {
            self.grid.len()
        }

        pub fn column_count(&self) -> usize {
            self.grid[0].len()
        }

        pub fn extents(&self) -> Vec2 {
            vec2(
                self.column_count() as f32 * self.tile_size,
                self.row_count() as f32 * self.tile_size,
            ) * 0.5
        }

        pub fn tile_center(&self, x: i32, y: i32) -> Vec2 {
            grid::tile_center(x, y, self.tile_size) - self.extents()
        }
    }

    pub fn create_level() -> Level {
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
        let mut level_grid = Vec::new();
        for row in grid {
            let level_row = row.iter().map(|i| LevelBlock::from(i)).collect();
            level_grid.push(level_row);
        }

        Level {
            grid: level_grid,
            tile_size: 32.,
        }
    }

    pub fn draw_mini_map(viewport: &Viewport2D, level: &Level) {
        let tile_size = vec2(level.tile_size, level.tile_size);
        for (y, row) in level.grid.iter().enumerate() {
            for (x, cell) in row.iter().enumerate() {
                let pos = level.tile_center(x as i32, y as i32);
                viewport.draw_rectangle(pos, tile_size, cell.color());
                match cell {
                    LevelBlock::Free => {
                        viewport.draw_rectangle_lines(pos, tile_size, 2.0, BLACK);
                    }
                    _ => {}
                }
            }
        }
    }
}

mod math {
    use macroquad::prelude::*;

    pub fn polar_to_cartesian(r: f32, theta_radians: f32) -> Vec2 {
        vec2(theta_radians.cos() * r, theta_radians.sin() * r)
    }
}

mod player {
    use macroquad::{math, prelude::*};

    use crate::graphics::Viewport2D;

    pub struct PlayerBundle {
        pub transform: PlayerTransform,
        pub movement: PlayerMovement,
        pub minimap_graphics: PlayerMinimapGraphics,
    }

    pub struct PlayerMovement {
        pub v: f32,
        pub w: f32,
    }

    pub struct PlayerTransform {
        pub pos: Vec2,
        pub rot: f32,
    }

    pub struct PlayerMinimapGraphics {
        pub color: Color,
        pub radius: f32,
        pub arrow_length: f32,
    }

    pub fn create_player() -> PlayerBundle {
        PlayerBundle {
            transform: PlayerTransform {
                pos: Vec2::ZERO,
                rot: 0.0,
            },
            movement: PlayerMovement {
                v: 60.0,
                w: 90_f32.to_radians(),
            },
            minimap_graphics: PlayerMinimapGraphics {
                color: PURPLE,
                radius: 10.,
                arrow_length: 20.,
            },
        }
    }

    pub fn update_player(player: &mut PlayerBundle, dt: f32) {
        let mut rot_input = 0.;
        if is_key_down(KeyCode::A) {
            rot_input += 1.;
        }
        if is_key_down(KeyCode::D) {
            rot_input -= 1.;
        }

        let mut move_input = 0.;
        if is_key_down(KeyCode::W) {
            move_input += 1.;
        }
        if is_key_down(KeyCode::S) {
            move_input -= 1.;
        }

        let new_rot = player.transform.rot + rot_input * player.movement.w * dt;
        let ds = math::polar_to_cartesian(move_input, new_rot) * player.movement.v * dt;

        player.transform.pos += ds;
        player.transform.rot = new_rot;
    }

    pub fn draw_player_minimap(player: &PlayerBundle, viewport: &Viewport2D) {
        viewport.draw_line(
            player.transform.pos,
            player.transform.pos
                + math::polar_to_cartesian(
                    player.minimap_graphics.arrow_length,
                    player.transform.rot,
                ),
            2.,
            BLACK,
        );
        viewport.draw_circle(
            player.transform.pos,
            player.minimap_graphics.radius,
            player.minimap_graphics.color,
        );
    }
}

pub async fn run() {
    let level = level::create_level();
    let mut player = player::create_player();
    let clear_color = Color::from_hex(0x333333);
    let map_viewport =
        graphics::Viewport2D::new(Vec2::ZERO, vec2(screen_width(), screen_height()), 1.);

    loop {
        let dt = get_frame_time();

        //simulate
        {
            player::update_player(&mut player, dt);
        }
        //draw
        {
            clear_background(clear_color);
            level::draw_mini_map(&map_viewport, &level);
            player::draw_player_minimap(&player, &map_viewport);
        }
        next_frame().await;
    }
}

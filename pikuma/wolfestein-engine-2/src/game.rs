use crate::engine::Engine;
use glam::Vec2;
use sdl2::{keyboard::Keycode, pixels::Color};

use self::{
    editor::{editor_update, enter_editor_mode, LevelEditor},
    level::*,
    player::*,
    playmode::{enter_playmode, play_mode_update},
};

pub mod player {
    use crate::{
        engine::Engine,
        math::{collision, polar_to_cartesian},
    };
    use glam::{vec2, Vec2};
    use sdl2::{keyboard::Keycode, pixels::Color};

    use super::level::Level;

    pub struct Player {
        pub position: Vec2,
        pub rotation: f32,

        pub move_speed: f32,
        pub rotation_speed: f32,

        pub half_fov: f32,

        pub size: f32,
        pub color: Color,
    }

    pub fn draw_player(player: &Player, level: &Level, engine: &mut Engine) {
        {
            let window_size = engine.renderer.window_size();
            let d_theta = player.half_fov * 2.0 / window_size.x;

            let mut theta = player.rotation - player.half_fov;
            let end_theta = player.rotation + player.half_fov;

            while theta <= end_theta {
                let hit_result = level.raycast_level(player.position, theta);
                engine.renderer.draw_line(
                    player.position,
                    player.position + polar_to_cartesian(hit_result.dist, theta),
                    Color::GRAY,
                );
                theta += d_theta;
            }
        }

        engine
            .renderer
            .draw_circle(player.position, player.size, player.color);
    }

    pub fn player_level_collision(player: &mut Player, level: &Level) {
        let tile_coords = level.position_to_coords(player.position);
        let tile_extents = vec2(level.tile_size, level.tile_size) * 0.5;

        let neighbors = [
            (-1, 0),
            (1, 0),
            (0, -1),
            (0, 1),
            (-1, -1),
            (-1, 1),
            (1, -1),
            (1, 1),
        ];

        for (dx, dy) in &neighbors {
            let x = tile_coords.x + dx;
            let y = tile_coords.y + dy;
            if level.is_wall(x, y) {
                player.position -= collision::circle_aabb_penetration(
                    player.position,
                    player.size,
                    level.tile_center(x, y),
                    tile_extents,
                );
            }
        }
    }

    pub fn update_player(player: &mut Player, level: &Level, engine: &Engine) {
        let dt = engine.time.dt as f32 / 1000.0;

        let mut dtheta = 0.0;
        if engine.input.is_key_held_down(Keycode::D) {
            dtheta += player.rotation_speed * dt;
        }
        if engine.input.is_key_held_down(Keycode::A) {
            dtheta -= player.rotation_speed * dt;
        }

        player.rotation += dtheta;

        let mut ds = 0.0;
        if engine.input.is_key_held_down(Keycode::W) {
            ds += player.move_speed * dt;
        }
        if engine.input.is_key_held_down(Keycode::S) {
            ds -= player.move_speed * dt;
        }

        let move_vector = polar_to_cartesian(ds, player.rotation);
        player.position += move_vector;

        player_level_collision(player, level);
    }

    // pub fn player_level_collision(pos : Vec2, radius: f32, level: &Level){
    //     let player_tile = level.tile_center(x, y)
    // }
}

mod level {
    use crate::engine::Engine;
    use crate::math::grid::{self, GridRaycastHitResult};
    use crate::math::{color_to_vec4, vec4_to_color};
    use crate::{engine::Renderer2D, math::grid::GridCoords};
    use glam::{vec2, Vec2};
    use sdl2::pixels::Color;
    use std::fs::File;
    use std::io::{BufRead, BufReader};
    use std::io::{BufWriter, Write};

    use super::player::Player;

    #[derive(PartialEq, Clone, Copy)]
    pub enum LevelBlock {
        Free = 0,
        Red = 1,
        Blue = 2,
        Green = 3,
        MAX,
    }

    impl From<u8> for LevelBlock {
        fn from(item: u8) -> Self {
            match item {
                0 => LevelBlock::Free,
                1 => LevelBlock::Red,
                2 => LevelBlock::Blue,
                3 => LevelBlock::Green,
                _ => LevelBlock::Red,
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
        pub fn colum_count(&self) -> usize {
            self.grid[0].len()
        }

        pub fn extents(&self) -> Vec2 {
            vec2(self.colum_count() as f32, self.row_count() as f32) * self.tile_size * 0.5
        }

        pub fn tile_center(&self, x: i32, y: i32) -> Vec2 {
            grid::tile_center(x, y, self.tile_size) - self.extents()
        }

        pub fn tile_extents(&self) -> Vec2 {
            vec2(self.tile_size, self.tile_size) * 0.5
        }

        pub fn position_to_coords(&self, pos: Vec2) -> GridCoords {
            grid::position_to_coords(pos + self.extents(), self.tile_size)
        }

        pub fn try_get_tile(&self, x: i32, y: i32) -> Option<LevelBlock> {
            if x >= 0 && x < (self.colum_count() as i32) && y >= 0 && y < (self.row_count() as i32)
            {
                return Some(self.grid[y as usize][x as usize]);
            }
            None
        }

        pub fn try_get_tile_mut(&mut self, x: i32, y: i32) -> Option<&mut LevelBlock> {
            if x >= 0 && x < (self.colum_count() as i32) && y >= 0 && y < (self.row_count() as i32)
            {
                return Some(&mut self.grid[y as usize][x as usize]);
            }
            None
        }

        pub fn is_wall(&self, x: i32, y: i32) -> bool {
            if let Some(t) = self.try_get_tile(x, y) {
                return match t {
                    LevelBlock::Free => false,
                    _ => true,
                };
            }
            true
        }

        pub fn tile_color(t: LevelBlock) -> Color {
            match t {
                LevelBlock::Free => Color::WHITE,
                LevelBlock::Red => Color::RED,
                LevelBlock::Blue => Color::CYAN,
                LevelBlock::Green => Color::GREEN,
                LevelBlock::MAX => Color::BLACK,
            }
        }

        pub fn raycast_level(&self, origin: Vec2, theta: f32) -> GridRaycastHitResult {
            let is_wall = |x, y| self.is_wall(x, y);
            grid::raycast_grid(origin + self.extents(), theta, self.tile_size, is_wall)
        }
    }

    pub fn draw_2d_map(level: &Level, minimap_renderer: &mut Renderer2D) {
        let tile_extents = vec2(level.tile_size, level.tile_size) * 0.5;
        for (y, row) in level.grid.iter().enumerate() {
            for (x, cell) in row.iter().enumerate() {
                let tile_center = level.tile_center(x as i32, y as i32);
                match *cell {
                    LevelBlock::Free => {
                        minimap_renderer.draw_wire_rect(tile_center, tile_extents, Color::BLACK)
                    }
                    _ => minimap_renderer.draw_fill_rect(
                        tile_center,
                        tile_extents,
                        Level::tile_color(*cell),
                    ),
                }
            }
        }
    }

    pub fn draw_3d_map(player: &Player, level: &Level, engine: &mut Engine) {
        let window_size = engine.renderer.window_size();
        let wall_width = 1.0;
        let d_theta = wall_width * player.half_fov * 2.0 / window_size.x;

        let mut theta = player.rotation - player.half_fov;
        let end_theta = player.rotation + player.half_fov;

        let dist_to_proj_plane = window_size.x * 0.5 / player.half_fov.tan();
        let wall_height = level.tile_size;
        let mut start_x = (wall_width - window_size.x) * 0.5;

        let shadow_decay_factor = 120.0;

        while theta <= end_theta {
            let hit_result = level.raycast_level(player.position, theta);
            let corrected_hit_dist = hit_result.dist * (theta - player.rotation).cos();
            let proj_height = (wall_height / corrected_hit_dist) * dist_to_proj_plane;

            let mut color_factor = (shadow_decay_factor / corrected_hit_dist).min(1.0);
            if hit_result.is_horizontal {
                color_factor *= 0.8;
            }

            // let color = Level::tile_color(
            //     level
            //         .try_get_tile(hit_result.coords.x, hit_result.coords.y)
            //         .unwrap(),
            // );

            let color = Color::WHITE;

            let color = vec4_to_color(color_to_vec4(color) * color_factor);

            engine.renderer.draw_fill_rect(
                vec2(start_x, 0.0),
                vec2(wall_width, proj_height) * 0.5,
                color,
            );
            theta += d_theta;
            start_x += wall_width;
        }
    }

    pub fn read_level_from_file(filename: &str) -> std::io::Result<Level> {
        let file = File::open(filename)?;
        let reader = BufReader::new(file);

        let mut grid = Vec::new();
        for line in reader.lines() {
            let row = line?
                .chars()
                .map(|ch| LevelBlock::from(ch.to_digit(10).unwrap() as u8))
                .collect();
            grid.push(row);
        }

        Ok(Level {
            grid,
            tile_size: 32.0,
        })
    }

    pub fn write_level_to_file(level: &Level, filename: &str) -> std::io::Result<()> {
        let file = File::create(filename)?;
        let mut writer = BufWriter::new(file);

        for row in level.grid.iter() {
            let line: String = row
                .iter()
                .map(|&num| char::from_digit(num as u32, 10).unwrap())
                .collect();
            writeln!(writer, "{}", line)?;
        }

        Ok(())
    }
}

pub fn create_player() -> Player {
    Player {
        position: Vec2::ZERO,
        rotation: 0.0_f32.to_radians(),
        move_speed: 100.0,
        rotation_speed: 85.0_f32.to_radians(),
        half_fov: 30.0_f32.to_radians(),
        size: 10.0,
        color: Color::RED,
    }
}

pub enum GameState {
    PlayMode,
    Editor,
}

pub mod editor {
    use sdl2::{
        keyboard::Keycode::{self, Space},
        pixels::Color,
    };

    use crate::{
        engine::{Engine, MouseButtonState, SdlMouseButton},
        math::grid::{FromToCoordsIterator, GridCoords},
    };

    use super::level::*;

    pub struct LevelEditor {
        pub tile_brush: LevelBlock,
    }

    pub fn enter_editor_mode(engine: &Engine) {
        engine.input.show_cursor(true);
    }

    pub fn editor_update(level_editor: &mut LevelEditor, level: &mut Level, engine: &mut Engine) {
        let mut shift_drag_tile: Option<GridCoords> = None;
        //tile painting
        {
            if engine.input.is_key_pressed(Space) {
                level_editor.tile_brush =
                    ((level_editor.tile_brush as u8 + 1) % LevelBlock::MAX as u8).into();
            }

            if let Some(lmb) = engine.input.mouse_button_state(SdlMouseButton::Left) {
                if engine.input.is_key_held_down(Keycode::LShift) {
                    match lmb {
                        MouseButtonState::Down { down_pos, .. }
                        | MouseButtonState::HeldDown { down_pos, .. } => {
                            shift_drag_tile = Some(level.position_to_coords(down_pos));
                        }
                        MouseButtonState::Up {
                            down_pos, up_pos, ..
                        } => {
                            let down_coords = level.position_to_coords(down_pos);
                            let up_coords = level.position_to_coords(up_pos);

                            for coords in FromToCoordsIterator::new(down_coords, up_coords) {
                                if let Some(t) = level.try_get_tile_mut(coords.x, coords.y) {
                                    *t = level_editor.tile_brush
                                }
                            }
                        }
                    }
                } else {
                    match lmb {
                        MouseButtonState::Up {
                            down_pos, up_pos, ..
                        } => {
                            let down_coords = level.position_to_coords(down_pos);
                            let up_coords = level.position_to_coords(up_pos);

                            if down_coords == up_coords {
                                if let Some(t) =
                                    level.try_get_tile_mut(down_coords.x, down_coords.y)
                                {
                                    *t = level_editor.tile_brush;
                                }
                            }
                        }
                        _ => {}
                    }
                }
            }
        }

        //render
        {
            engine.renderer.clear(Color::GRAY);
            draw_2d_map(&level, &mut engine.renderer);

            let mouse_coords = level.position_to_coords(engine.input.mouse_position());
            let tile_center = level.tile_center(mouse_coords.x, mouse_coords.y);

            let mut tile_color = Level::tile_color(level_editor.tile_brush);
            tile_color.a = 255 / 2;
            let tile_extents = level.tile_extents();

            if let Some(down_tile) = shift_drag_tile {
                for coords in FromToCoordsIterator::new(down_tile, mouse_coords) {
                    engine.renderer.draw_fill_rect(
                        level.tile_center(coords.x, coords.y),
                        tile_extents,
                        tile_color,
                    );
                }
            }

            engine
                .renderer
                .draw_fill_rect(tile_center, tile_extents, tile_color);
            engine
                .renderer
                .draw_wire_rect(tile_center, tile_extents, Color::GREEN);
            engine.renderer.present();
        }
    }
}

pub mod playmode {
    use sdl2::pixels::Color;

    use crate::engine::Engine;

    use super::{level::*, player::*};

    pub fn enter_playmode(engine: &Engine) {
        engine.input.show_cursor(false);
    }

    pub fn play_mode_update(player: &mut Player, level: &Level, engine: &mut Engine) {
        //update
        {
            update_player(player, &level, &engine);
        }

        //render
        {
            engine.renderer.clear(Color::GRAY);
            draw_3d_map(player, level, engine);
            engine.renderer.present();
        }
    }
}

pub fn run() {
    let mut engine = Engine::new(800, 600);

    const LEVEL_FILE: &str = "data/lvl1.level";
    let mut level = read_level_from_file(LEVEL_FILE).unwrap();
    let mut player = create_player();

    let mut level_editor = LevelEditor {
        tile_brush: LevelBlock::Red,
    };

    let mut game_state = GameState::PlayMode;

    engine.input.show_cursor(false);
    loop {
        engine.pre_update();

        if engine.input.is_key_pressed(Keycode::Escape) {
            write_level_to_file(&level, LEVEL_FILE).unwrap();
            break;
        }

        if engine.input.is_key_pressed(Keycode::F11) {
            match game_state {
                GameState::PlayMode => {
                    game_state = GameState::Editor;
                    enter_editor_mode(&engine);
                }
                GameState::Editor => {
                    game_state = GameState::PlayMode;
                    enter_playmode(&engine);
                }
            }
        }

        match game_state {
            GameState::PlayMode => play_mode_update(&mut player, &level, &mut engine),
            GameState::Editor => editor_update(&mut level_editor, &mut level, &mut engine),
        }

        engine.post_update();
    }
}

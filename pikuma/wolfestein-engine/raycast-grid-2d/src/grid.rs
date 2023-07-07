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

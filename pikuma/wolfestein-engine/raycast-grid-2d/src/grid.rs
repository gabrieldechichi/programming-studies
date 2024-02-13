use crate::math;
use macroquad::prelude::*;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
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

pub struct RaycastResult {
    pub distance: f32,
    pub coords: GridCoords,
    pub is_horizontal_hit: bool,
}

pub fn raycast_grid<F>(origin: Vec2, theta: f32, tile_size: f32, is_wall: F) -> RaycastResult
where
    F: Fn(i32, i32) -> bool,
{
    let origin_coords = position_to_coords(origin, tile_size);

    let theta_tan = theta.tan();
    //horizontal
    let h_hit = {
        let y_sign = math::normalize_angle(theta) < math::PI;

        let start_coords = if y_sign {
            origin_coords + coords(0, 1)
        } else {
            origin_coords
        };

        let mut y_intersect = tile_bl(start_coords.x, start_coords.y, tile_size).y;
        let mut x_intersect = origin.x + (y_intersect - origin.y) / theta_tan;

        let y_step = if y_sign { tile_size } else { -tile_size };

        let x_step = y_step / theta_tan;

        let p_extra = if y_sign {
            vec2(0.001, 0.001)
        } else {
            -vec2(0.001, 0.001)
        };

        let mut coords: GridCoords;
        loop {
            coords = position_to_coords(vec2(x_intersect, y_intersect) + p_extra, tile_size);

            if is_wall(coords.x, coords.y) {
                break;
            } else {
                x_intersect += x_step;
                y_intersect += y_step;
            }
        }

        RaycastResult {
            distance: vec2(x_intersect, y_intersect).distance(origin),
            is_horizontal_hit: true,
            coords,
        }
    };

    //vertical
    let v_hit = {
        let x_sign = !(math::normalize_angle(theta) > math::PI * 0.5
            && math::normalize_angle(theta) < math::PI * 1.5);

        let start_coords = if x_sign {
            origin_coords + coords(1, 0)
        } else {
            origin_coords
        };

        let mut x_intersect = tile_bl(start_coords.x, start_coords.y, tile_size).x;

        let mut y_intersect = origin.y + (x_intersect - origin.x) * theta_tan;

        let x_step = if x_sign { tile_size } else { -tile_size };

        let y_step = x_step * theta_tan;

        let p_extra = if x_sign {
            vec2(0.001, 0.001)
        } else {
            -vec2(0.001, 0.001)
        };

        let mut coords: GridCoords;
        loop {
            coords = position_to_coords(vec2(x_intersect, y_intersect) + p_extra, tile_size);

            if is_wall(coords.x, coords.y) {
                break;
            } else {
                x_intersect += x_step;
                y_intersect += y_step;
            }
        }

        RaycastResult {
            distance: vec2(x_intersect, y_intersect).distance(origin),
            is_horizontal_hit: false,
            coords,
        }
    };

    if h_hit.distance > v_hit.distance {
        v_hit
    } else {
        h_hit
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use macroquad::math::Vec2;

    #[test]
    fn add_grid_coords_test() {
        let coord1 = GridCoords { x: 5, y: 3 };
        let coord2 = GridCoords { x: 2, y: 7 };
        let result = coord1 + coord2;
        assert_eq!(result.x, 7);
        assert_eq!(result.y, 10);
    }

    #[test]
    fn coords_function_test() {
        let result = coords(4, 7);
        assert_eq!(result.x, 4);
        assert_eq!(result.y, 7);
    }

    #[test]
    fn tile_center_test() {
        let result = tile_center(2, 3, 5.0);
        assert_eq!(result, vec2(12.5, 17.5));
    }

    #[test]
    fn tile_bl_test() {
        let result = tile_bl(2, 3, 5.0);
        assert_eq!(result, vec2(10.0, 15.0));
    }

    #[test]
    fn position_to_coords_test() {
        let pos = Vec2::new(12.0, 17.0);
        let result = position_to_coords(pos, 5.0);
        assert_eq!(result, GridCoords { x: 2, y: 3 });
    }

    #[test]
    fn raycast_grid_test() {
        //45 deg
        {
            let origin = Vec2::new(0.0, 0.0);
            let theta = std::f32::consts::FRAC_PI_4;

            let tile_size = 5.0;
            let expect_coord_wall = coords(3, 3);
            let result = raycast_grid(origin, theta, tile_size, |x, y| {
                x == expect_coord_wall.x && y == expect_coord_wall.y
            });

            let expected_dist =
                (vec2(expect_coord_wall.x as f32, expect_coord_wall.y as f32) * tile_size - origin)
                    .length();
            assert_eq!(result.distance, expected_dist);
        }

        // //-90 deg
        // {
        //     let origin = Vec2::new(0.0, 0.0);
        //     let theta = -std::f32::consts::FRAC_PI_2;

        //     let tile_size = 5.0;
        //     let expect_coord_wall = coords(0, -4);
        //     let result = raycast_grid(origin, theta, tile_size, |x, y| {
        //         x == expect_coord_wall.x && y == expect_coord_wall.y
        //     });

        //     let expected_dist =
        //         (vec2(expect_coord_wall.x as f32, expect_coord_wall.y as f32) * tile_size - origin)
        //             .length();
        //     assert_eq!(result, expected_dist);
        // }
    }
}

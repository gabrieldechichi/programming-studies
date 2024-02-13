use glam::{vec2, vec4, Vec2, Vec4};
use sdl2::pixels::Color;

pub const PI: f32 = std::f32::consts::PI;

pub fn polar_to_cartesian(r: f32, theta: f32) -> Vec2 {
    vec2(theta.cos() * r, theta.sin() * r)
}

pub fn normalize_angle(a: f32) -> f32 {
    a.rem_euclid(2.0 * std::f32::consts::PI)
}

pub fn color_to_vec4(c: Color) -> Vec4 {
    vec4(c.r as f32, c.g as f32, c.b as f32, c.a as f32)
}

pub fn vec4_to_color(v: Vec4) -> Color {
    Color::RGBA(v.x as u8, v.y as u8, v.z as u8, v.w as u8)
}

pub mod grid {
    use glam::{vec2, Vec2};

    use super::{normalize_angle, PI};

    #[derive(Default, Debug, Clone, Copy, PartialEq, Eq)]
    pub struct GridCoords {
        pub x: i32,
        pub y: i32,
    }

    pub struct FromToCoordsIterator {
        current: Option<GridCoords>,
        start: GridCoords,
        end: GridCoords,
        x_sign: i32,
        y_sign: i32,
    }

    impl FromToCoordsIterator {
        pub fn new(start: GridCoords, end: GridCoords) -> Self {
            let mut x_sign = (end.x - start.x).signum();
            let mut y_sign = (end.y - start.y).signum();
            if x_sign == 0 {
                x_sign = 1;
            }
            if y_sign == 0 {
                y_sign = 1;
            }
            Self {
                current: Some(start),
                start,
                end,
                x_sign,
                y_sign,
            }
        }
    }

    impl Iterator for FromToCoordsIterator {
        type Item = GridCoords;

        fn next(&mut self) -> Option<Self::Item> {
            let result = self.current;

            if let Some(current) = &mut self.current {
                current.y += self.y_sign;
                if self.y_sign * current.y > self.y_sign * self.end.y {
                    current.x += self.x_sign;
                    current.y = self.start.y;
                }

                if self.x_sign * current.x > self.x_sign * self.end.x {
                    self.current = None;
                    return Some(self.end);
                }
            }

            result
        }
    }

    pub fn tile_bottom_left(x: i32, y: i32, tile_size: f32) -> Vec2 {
        return vec2(tile_size * x as f32, tile_size * y as f32);
    }

    pub fn tile_center(x: i32, y: i32, tile_size: f32) -> Vec2 {
        return tile_bottom_left(x, y, tile_size) + vec2(tile_size, tile_size) * 0.5;
    }

    pub fn position_to_coords(pos: Vec2, tile_size: f32) -> GridCoords {
        GridCoords {
            x: (pos.x / tile_size).floor() as i32,
            y: (pos.y / tile_size).floor() as i32,
        }
    }

    pub struct GridRaycastHitResult {
        pub dist: f32,
        pub coords: GridCoords,
        pub is_horizontal: bool,
    }

    pub fn raycast_grid<F>(
        origin: Vec2,
        theta: f32,
        tile_size: f32,
        is_wall: F,
    ) -> GridRaycastHitResult
    where
        F: Fn(i32, i32) -> bool,
    {
        let origin_coords = position_to_coords(origin, tile_size);
        let normalized_theta = normalize_angle(theta);
        let theta_tan = normalized_theta.tan();
        let horizontal_hit = {
            let y_sign = normalized_theta < PI;
            let mut start_coords = origin_coords;

            if y_sign {
                start_coords.y += 1;
            }

            let mut y_intersect = tile_bottom_left(start_coords.x, start_coords.y, tile_size).y;
            let mut x_intersect = origin.x + (y_intersect - origin.y) / theta_tan;

            let y_increment = if y_sign { tile_size } else { -tile_size };
            let x_increment = y_increment / theta_tan;

            let extra_increment = if y_sign {
                vec2(0.0001, 0.0001)
            } else {
                -vec2(0.0001, 0.0001)
            };

            let mut coords: GridCoords;
            loop {
                coords =
                    position_to_coords(vec2(x_intersect, y_intersect) + extra_increment, tile_size);
                if is_wall(coords.x, coords.y) {
                    break;
                }
                y_intersect += y_increment;
                x_intersect += x_increment;
            }

            GridRaycastHitResult {
                dist: vec2(x_intersect, y_intersect).distance(origin),
                coords,
                is_horizontal: true,
            }
        };

        let vertical_hit = {
            let x_sign = !(normalized_theta > PI * 0.5 && normalized_theta < PI * 1.5);
            let mut start_coords = origin_coords;

            if x_sign {
                start_coords.x += 1;
            }

            let mut x_intersect = tile_bottom_left(start_coords.x, start_coords.y, tile_size).x;
            let mut y_intersect = origin.y + (x_intersect - origin.x) * theta_tan;

            let x_increment = if x_sign { tile_size } else { -tile_size };
            let y_increment = x_increment * theta_tan;

            let extra_increment = if x_sign {
                vec2(0.0001, 0.0001)
            } else {
                -vec2(0.0001, 0.0001)
            };

            let mut coords: GridCoords;
            loop {
                coords =
                    position_to_coords(vec2(x_intersect, y_intersect) + extra_increment, tile_size);
                if is_wall(coords.x, coords.y) {
                    break;
                }
                y_intersect += y_increment;
                x_intersect += x_increment;
            }

            GridRaycastHitResult {
                dist: vec2(x_intersect, y_intersect).distance(origin),
                coords,
                is_horizontal: false,
            }
        };

        if horizontal_hit.dist < vertical_hit.dist {
            horizontal_hit
        } else {
            vertical_hit
        }
    }
}

pub mod collision {
    use glam::Vec2;

    pub fn circle_aabb_penetration(
        circle_center: Vec2,
        circle_radius: f32,
        aabb_center: Vec2,
        aabb_extents: Vec2,
    ) -> Vec2 {
        let diff = aabb_center - circle_center;
        let clamped = diff.clamp(-aabb_extents, aabb_extents);

        let circle_to_edge = diff - clamped;
        let circle_to_edge_length = circle_to_edge.length();

        if circle_to_edge_length > circle_radius {
            Vec2::ZERO
        } else {
            circle_radius * circle_to_edge.normalize() - circle_to_edge
        }
    }
}

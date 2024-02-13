use macroquad::prelude::*;

pub const PI: f32 = std::f32::consts::PI;

pub fn polar_to_cartesian(r: f32, theta_radians: f32) -> Vec2 {
    vec2(theta_radians.cos() * r, theta_radians.sin() * r)
}

pub fn normalize_angle(a: f32) -> f32 {
    a.rem_euclid(2.0 * std::f32::consts::PI)
}

pub fn lerp(a: f32, b: f32, t: f32) -> f32 {
    a * (1.0 - t) + b * t
}

///Returns how much a line of `center` and `extents` is outside a given `bounds`, or 0.0 if inside
///The penetration returned can be subtracted from `center` in order to move it back inside `bounds`
pub fn axis_bounds_penetration(center: f32, extents: f32, bounds: f32) -> f32 {
    let sign = center.signum();
    let penetration = (sign * (center + sign * (extents - bounds * 0.5))).max(0.0);
    sign * penetration
}

pub fn circle_bounds_penetration(circle_pos: Vec2, circle_radius: f32, bounds: Vec2) -> Vec2 {
    Vec2::new(
        axis_bounds_penetration(circle_pos.x, circle_radius, bounds.x),
        axis_bounds_penetration(circle_pos.y, circle_radius, bounds.y),
    )
}

pub fn aabb_rect_bounds_penetration(center: Vec2, extents: Vec2, bounds: Vec2) -> Vec2 {
    Vec2::new(
        axis_bounds_penetration(center.x, extents.x, bounds.x),
        axis_bounds_penetration(center.y, extents.y, bounds.y),
    )
}

pub fn circle_aabb_penetration(
    circle_pos: Vec2,
    circle_radius: f32,
    aabb_center: Vec2,
    aabb_extents: Vec2,
) -> Vec2 {
    let diff = circle_pos - aabb_center;
    let clamped = Vec2::new(
        diff.x.clamp(-aabb_extents.x, aabb_extents.x),
        diff.y.clamp(-aabb_extents.y, aabb_extents.y),
    );

    let p = clamped - diff;

    let distance = p.length();

    if distance > 0.0 && distance < circle_radius {
        p.normalize() * (circle_radius - distance)
    } else {
        Vec2::ZERO
    }
}

#[cfg(test)]
mod tests {
    mod axis_bounds_penetration_tests {
        use crate::math::*;

        #[test]
        fn returns_zero_if_inside_bounds() {
            let center = 1.0;
            let size = 4.0;
            let bounds = 8.0;
            let p = axis_bounds_penetration(center, size * 0.5, bounds);
            assert_eq!(p, 0.0);
        }

        #[test]
        fn returns_zero_if_touching_bounds() {
            let center = 1.0;
            let size = 4.0;
            let bounds = 6.0;
            let p = axis_bounds_penetration(center, size * 0.5, bounds);
            assert_eq!(p, 0.0);

            //to be safe, check that just a bit further is outside bounds
            let p2 = axis_bounds_penetration(center + 0.0001, size * 0.5, bounds);
            assert!(p2 > 0.0);
        }

        #[test]
        fn returns_positive_if_outside_bounds_right_side() {
            let center = 1.0;
            let size = 4.0;
            let bounds = 4.0;
            let p = axis_bounds_penetration(center, size * 0.5, bounds);
            assert_eq!(p, 1.0);
        }

        #[test]
        fn returns_negative_if_outside_bounds_left_side() {
            let center = -1.0;
            let size = 4.0;
            let bounds = 4.0;
            let p = axis_bounds_penetration(center, size * 0.5, bounds);
            assert_eq!(p, -1.0);
        }
    }

    mod circle_rect_penetration_tests {
        use macroquad::prelude::{vec2, Vec2};

        use crate::math::*;

        #[test]
        fn returns_zero_if_not_overlapping() {
            let circle_pos = vec2(1.0, 1.0);
            let circle_radius = 5.0;
            let rect_center = vec2(20.0, 20.0);
            let rect_extents = vec2(5.0, 5.0);
            let p = circle_aabb_penetration(circle_pos, circle_radius, rect_center, rect_extents);
            assert_eq!(p, Vec2::ZERO);
        }

        #[test]
        fn returns_zero_if_touching_corners() {
            //we test both axis separately for clarity
            //x axis
            {
                let circle_pos = vec2(1.0, 0.0);
                let circle_radius = 5.0;
                let rect_extents_axis = 5.0;
                let rect_extents = vec2(rect_extents_axis, rect_extents_axis);
                let rect_center = circle_pos + vec2(circle_radius + rect_extents_axis, 0.0);
                let p =
                    circle_aabb_penetration(circle_pos, circle_radius, rect_center, rect_extents);
                assert_eq!(p, Vec2::ZERO);

                //check that just a bit further creates an overlap
                let p2 = circle_aabb_penetration(
                    circle_pos + vec2(0.0001, 0.0),
                    circle_radius,
                    rect_center,
                    rect_extents,
                );

                assert_ne!(p2, Vec2::ZERO);
            }

            //y axis
            {
                let circle_pos = vec2(0.0, 1.0);
                let circle_radius = 5.0;
                let rect_extents_axis = 5.0;
                let rect_extents = vec2(rect_extents_axis, rect_extents_axis);
                let rect_center = circle_pos + vec2(0.0, circle_radius + rect_extents_axis);
                let p =
                    circle_aabb_penetration(circle_pos, circle_radius, rect_center, rect_extents);
                assert_eq!(p, Vec2::ZERO);

                //check that just a bit further creates an overlap
                let p2 = circle_aabb_penetration(
                    circle_pos + vec2(0.0, 0.00001),
                    circle_radius,
                    rect_center,
                    rect_extents,
                );

                assert_ne!(p2, Vec2::ZERO);
            }
        }

        #[test]
        fn returns_positive_if_overlapping_right() {
            let circle_pos = vec2(1.0, 1.0);
            let circle_radius = 5.0;
            let rect_center = vec2(8.0, 8.0);
            let rect_extents = vec2(5.0, 5.0);
            let p = circle_aabb_penetration(circle_pos, circle_radius, rect_center, rect_extents);
            //hardcoded values here that match the correct calculation
            assert_eq!(p, vec2(1.5355339, 1.5355339));
        }

        #[test]
        fn returns_negative_if_overlapping_right() {
            let circle_pos = vec2(1.0, 1.0);
            let circle_radius = 5.0;
            let rect_center = vec2(-6.0, -6.0);
            let rect_extents = vec2(5.0, 5.0);
            let p = circle_aabb_penetration(circle_pos, circle_radius, rect_center, rect_extents);
            //hardcoded values here that match the correct calculation
            assert_eq!(p, vec2(-1.5355339, -1.5355339));
        }
    }
}

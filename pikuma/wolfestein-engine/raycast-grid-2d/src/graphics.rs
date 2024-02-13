use macroquad::prelude::*;

pub struct Viewport2D {
    pub size: Vec2,
    pub pivot: Vec2,
    pub scale: f32,
}

impl Viewport2D {
    pub fn world_to_viewport(&self, pos: Vec2) -> Vec2 {
        vec2(
            self.pivot.x + pos.x * self.scale + self.size.x * 0.5,
            self.pivot.y - pos.y * self.scale + self.size.y * 0.5,
        )
    }

    pub fn draw_rectangle(&self, center: Vec2, size: Vec2, color: Color) {
        let draw_pos = self.world_to_viewport(Viewport2D::rect_top_left(center, size));
        draw_rectangle(
            draw_pos.x,
            draw_pos.y,
            size.x * self.scale,
            size.y * self.scale,
            color,
        );
    }

    pub fn draw_rectangle_lines(&self, center: Vec2, size: Vec2, thickness: f32, color: Color) {
        let draw_pos = self.world_to_viewport(Viewport2D::rect_top_left(center, size));
        draw_rectangle_lines(
            draw_pos.x,
            draw_pos.y,
            size.x * self.scale,
            size.y * self.scale,
            thickness,
            color,
        );
    }

    pub fn draw_line(&self, from: Vec2, to: Vec2, thickness: f32, color: Color) {
        let from_draw = self.world_to_viewport(from);
        let to_draw = self.world_to_viewport(to);
        draw_line(
            from_draw.x,
            from_draw.y,
            to_draw.x,
            to_draw.y,
            thickness * self.scale,
            color,
        );
    }

    pub fn draw_circle(&self, center: Vec2, r: f32, color: Color) {
        let center_draw = self.world_to_viewport(center);
        draw_circle(center_draw.x, center_draw.y, r * self.scale, color);
    }

    fn rect_top_left(center: Vec2, size: Vec2) -> Vec2 {
        center + vec2(-size.x, size.y) * 0.5
    }
}

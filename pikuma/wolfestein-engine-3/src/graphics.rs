use macroquad::prelude::*;

pub struct ScreenBuffer {
    pub image: Image,
    pub tex: Texture2D,
}

impl ScreenBuffer {
    pub fn new(width: usize, height: usize) -> Self {
        let image = {
            let buffer_size = width * height * 4;
            let mut buf = Vec::with_capacity(buffer_size);
            buf.resize(buffer_size, 0);
            Image {
                bytes: buf,
                width: width as u16,
                height: height as u16,
            }
        };

        let tex = Texture2D::from_image(&image);
        Self { image, tex }
    }

    pub fn fill(&mut self, x_start: u16, y_start: u16, x_end: u16, y_end: u16, color: Color) {
        let start_i = self.xy_to_index_safe(x_start, y_start);
        let end_i = self.xy_to_index_safe(x_end, y_end);
        for i in (start_i..end_i).step_by(4) {
            self.image.bytes[i] = (color.r * 255.) as u8;
            self.image.bytes[i + 1] = (color.g * 255.) as u8;
            self.image.bytes[i + 2] = (color.b * 255.) as u8;
            self.image.bytes[i + 3] = (color.a * 255.) as u8;
        }
    }

    pub fn xy_to_index(&self, x: u16, y: u16) -> usize {
        x as usize + (y as usize * self.width() as usize * 4)
    }
    pub fn xy_to_index_safe(&self, x: u16, y: u16) -> usize {
        self.xy_to_index(x, y).min(self.image.bytes.len())
    }

    pub fn width(&self) -> u16 {
        self.image.width
    }
    pub fn height(&self) -> u16 {
        self.image.height
    }

    pub fn set_pixel(&mut self, x: u32, y: u32, color: Color) {
        self.image.set_pixel(x, y, color)
    }

    pub fn draw(&self) {
        self.tex.update(&self.image);
        draw_texture(&self.tex, 0., 0., WHITE);
    }
}

pub struct Viewport2D {
    pub size: Vec2,
    pub pivot: Vec2,
    pub scale: f32,
}

impl Viewport2D {
    pub fn new(pivot: Vec2, size: Vec2, scale: f32) -> Self {
        Self { pivot, size, scale }
    }
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

use glam::{vec2, Mat3, Vec2};
use sdl2::{
    event::Event,
    keyboard::Keycode,
    pixels::{Color, PixelFormatEnum},
    rect::Rect,
    render::{Texture, TextureCreator, WindowCanvas},
    video::WindowContext,
    EventPump, Sdl, TimerSubsystem,
};

pub type SdlMouseButton = sdl2::mouse::MouseButton;

pub struct Time {
    pub target_frame_time: u32,
    pub dt: u32,
    pub last_frame_elapsed_time: u32,

    timer: TimerSubsystem,
}

trait InputBuffer {
    fn index_of(&self, target: Keycode) -> Option<usize>;
}

impl InputBuffer for Vec<InputKey> {
    fn index_of(&self, target: Keycode) -> Option<usize> {
        self.iter().position(|key| key.keycode == target)
    }
}

trait MouseButtonBuffer {
    fn index_of(&self, target: SdlMouseButton) -> Option<usize>;
}

impl MouseButtonBuffer for Vec<MouseButton> {
    fn index_of(&self, target: SdlMouseButton) -> Option<usize> {
        self.iter().position(|key| key.button == target)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum KeyState {
    HeldDown,
    Released,
    Pressed,
}

#[derive(Debug, Clone, Copy)]
pub struct InputKey {
    pub keycode: Keycode,
    pub state: KeyState,
}

#[derive(Clone, Copy, PartialEq, Debug)]
pub enum MouseButtonState {
    Down {
        down_pos: Vec2,
        down_time: u32,
    },
    HeldDown {
        down_pos: Vec2,
        down_time: u32,
    },
    Up {
        down_pos: Vec2,
        down_time: u32,
        up_pos: Vec2,
        up_time: u32,
    },
}

#[derive(Debug, Clone, Copy)]
pub struct MouseButton {
    pub button: SdlMouseButton,
    pub state: MouseButtonState,
}

#[derive(Default, Debug, Clone)]
pub struct MouseState {
    pub pos: Vec2,
    pub buttons: Vec<MouseButton>,
}

pub struct Input {
    keys: Vec<InputKey>,
    mouse_state: MouseState,
    sdl: Sdl,
}

impl Input {
    pub fn is_key_pressed(&self, keycode: Keycode) -> bool {
        self.is_key_in_state(keycode, KeyState::Pressed)
    }
    pub fn is_key_released(&self, keycode: Keycode) -> bool {
        self.is_key_in_state(keycode, KeyState::Released)
    }
    pub fn is_key_held_down(&self, keycode: Keycode) -> bool {
        self.is_key_in_state(keycode, KeyState::HeldDown)
            || self.is_key_in_state(keycode, KeyState::Pressed)
    }

    fn is_key_in_state(&self, keycode: Keycode, key_state: KeyState) -> bool {
        match self.keys.index_of(keycode) {
            Some(idx) => self.keys[idx].state == key_state,
            None => false,
        }
    }

    pub fn show_cursor(&self, show: bool) {
        self.sdl.mouse().show_cursor(show);
    }

    pub fn mouse_position(&self) -> Vec2 {
        self.mouse_state.pos
    }

    pub fn mouse_button_state(&self, mouse_btn: SdlMouseButton) -> Option<MouseButtonState> {
        match self.mouse_state.buttons.index_of(mouse_btn) {
            Some(idx) => Some(self.mouse_state.buttons[idx].state),
            None => None,
        }
    }
}

pub struct Engine {
    pub renderer: Renderer2D,
    pub time: Time,
    pub input: Input,
    event_pump: EventPump,
}

impl Engine {
    pub fn new(window_width: u32, window_height: u32) -> Self {
        let sdl = sdl2::init().unwrap();
        let video = sdl.video().unwrap();
        let window = video
            .window("", window_width, window_height)
            .position_centered()
            .build()
            .unwrap();

        let canvas = window.into_canvas().build().unwrap();
        let event_pump = sdl.event_pump().unwrap();
        let renderer = Renderer2D::new(canvas);

        let timer = sdl.timer().unwrap();

        Self {
            renderer,
            event_pump,
            time: Time {
                target_frame_time: 1000 / 60,
                dt: 0,
                last_frame_elapsed_time: 0,
                timer,
            },
            input: Input {
                mouse_state: MouseState::default(),
                keys: Vec::new(),
                sdl,
            },
        }
    }

    pub fn pre_update(&mut self) {
        //delta time
        {
            let ticks = self.time.timer.ticks();
            self.time.dt = ticks - self.time.last_frame_elapsed_time;
            self.time.last_frame_elapsed_time = ticks;

            let remaining = self.time.target_frame_time as i32 - self.time.dt as i32;
            if remaining > 0 {
                self.time.timer.delay(remaining as u32)
            }
        }

        //input
        {
            for event in self.event_pump.poll_iter() {
                match event {
                    Event::KeyDown {
                        keycode: Some(keycode),
                        ..
                    } => {
                        if let Some(idx) = self.input.keys.index_of(keycode) {
                            let mut key = &mut self.input.keys[idx];
                            match key.state {
                                KeyState::HeldDown => (),
                                KeyState::Released => key.state = KeyState::Pressed,
                                KeyState::Pressed => key.state = KeyState::HeldDown,
                            }
                        } else {
                            self.input.keys.push(InputKey {
                                keycode,
                                state: KeyState::Pressed,
                            })
                        }
                    }
                    Event::KeyUp {
                        keycode: Some(keycode),
                        ..
                    } => {
                        if let Some(idx) = self.input.keys.index_of(keycode) {
                            let mut key = &mut self.input.keys[idx];
                            match key.state {
                                KeyState::HeldDown | KeyState::Pressed => {
                                    key.state = KeyState::Released
                                }
                                KeyState::Released => (),
                            }
                        } else {
                            self.input.keys.push(InputKey {
                                keycode,
                                state: KeyState::Released,
                            })
                        }
                    }
                    Event::MouseButtonDown {
                        mouse_btn, x, y, ..
                    } => {
                        if let None = self.input.mouse_state.buttons.index_of(mouse_btn) {
                            let down_pos = self
                                .renderer
                                .to_world
                                .transform_point2(vec2(x as f32, y as f32));
                            self.input.mouse_state.buttons.push(MouseButton {
                                button: mouse_btn,
                                state: MouseButtonState::Down {
                                    down_pos,
                                    down_time: self.time.last_frame_elapsed_time,
                                },
                            })
                        }
                    }
                    Event::MouseButtonUp {
                        mouse_btn, x, y, ..
                    } => {
                        if let Some(idx) = self.input.mouse_state.buttons.index_of(mouse_btn) {
                            let mut btn = &mut self.input.mouse_state.buttons[idx];
                            match btn.state {
                                MouseButtonState::Down {
                                    down_pos,
                                    down_time,
                                }
                                | MouseButtonState::HeldDown {
                                    down_pos,
                                    down_time,
                                } => {
                                    let up_pos = self
                                        .renderer
                                        .to_world
                                        .transform_point2(vec2(x as f32, y as f32));
                                    btn.state = MouseButtonState::Up {
                                        down_pos,
                                        down_time,
                                        up_pos,
                                        up_time: self.time.last_frame_elapsed_time,
                                    }
                                }
                                _ => {}
                            }
                        }
                    }
                    _ => {}
                }
            }

            let sdl_mouse_state = self.event_pump.mouse_state();
            self.input.mouse_state.pos = self
                .renderer
                .to_world
                .transform_point2(vec2(sdl_mouse_state.x() as f32, sdl_mouse_state.y() as f32));
        }
    }

    pub fn post_update(&mut self) {
        self.input
            .keys
            .retain(|key| key.state != KeyState::Released);
        self.input
            .keys
            .iter_mut()
            .filter(|key| key.state == KeyState::Pressed)
            .for_each(|key| key.state = KeyState::HeldDown);

        self.input
            .mouse_state
            .buttons
            .retain(|button| match button.state {
                MouseButtonState::Up { .. } => false,
                _ => true,
            });

        self.input
            .mouse_state
            .buttons
            .iter_mut()
            .for_each(|btn| match btn.state {
                MouseButtonState::Down {
                    down_pos,
                    down_time,
                } => {
                    btn.state = MouseButtonState::HeldDown {
                        down_pos,
                        down_time,
                    }
                }
                _ => {}
            });
    }
}

pub struct Renderer2D {
    canvas: WindowCanvas,
    width: u32,
    height: u32,
    color_buffer: Vec<u32>,
    texture_creator: TextureCreator<WindowContext>,
    texture: Texture,
    to_screen: Mat3,
    to_world: Mat3,
}

impl Renderer2D {
    pub fn new(canvas: WindowCanvas) -> Self {
        let size = canvas.window().size();
        let t_mat = Mat3::from_translation(vec2(size.0 as f32 * 0.5, size.1 as f32 * 0.5));
        let s_mat = Mat3::from_scale(vec2(1.0, -1.0));
        let to_screen = t_mat * s_mat;

        let color_buffer: Vec<u32> = vec![0; (size.0 * size.1) as usize];

        let texture_creator = canvas.texture_creator();
        let texture = texture_creator
            .create_texture_streaming(PixelFormatEnum::ARGB8888, size.0, size.1)
            .unwrap();
        Self {
            canvas,
            width: size.0,
            height: size.1,
            color_buffer,
            texture_creator,
            texture,
            to_screen: to_screen,
            to_world: to_screen.inverse(),
        }
    }

    pub fn clear(&mut self, color: Color) {
        self.canvas.clear();
        for p in &mut self.color_buffer {
            *p = Renderer2D::color_to_u32(&color);
        }
    }

    fn color_to_u32(color: &Color) -> u32 {
        ((color.a as u32) << 24)
            | ((color.r as u32) << 16)
            | ((color.g as u32) << 8)
            | (color.b as u32)
    }

    pub fn present(&mut self) {
        self.texture
            .with_lock(None, |buffer: &mut [u8], pitch: usize| {
                for y in 0..self.height {
                    for x in 0..self.width {
                        //pitch is the size of a row in bytes (width * pixel size if a pixel is u32)
                        //since we are offsetting in bytes, y * width + x turns into y * pitch + x * 4
                        let offset = y as usize * pitch + x as usize * 4;
                        let pixel = self.color_buffer[(y * self.width + x) as usize];
                        buffer[offset..offset + 4].copy_from_slice(&pixel.to_le_bytes());
                    }
                }
            })
            .unwrap();
        self.canvas.copy(&self.texture, None, None).unwrap();
        self.canvas.present();
    }

    pub fn window_size(&self) -> Vec2 {
        let s = self.canvas.window().size();
        vec2(s.0 as f32, s.1 as f32)
    }

    pub fn draw_line(&mut self, from: Vec2, to: Vec2, color: Color) {
        let screen_from = self.to_screen.transform_point2(from);
        let screen_to = self.to_screen.transform_point2(to);

        // self.canvas.set_draw_color(color);
        // self.canvas
        //     .draw_line(
        //         (screen_from.x as i32, screen_from.y as i32),
        //         (screen_to.x as i32, screen_to.y as i32),
        //     )
        //     .unwrap();
    }

    pub fn draw_fill_rect(&mut self, center: Vec2, extents: Vec2, color: Color) {
        let screen_pos = self.to_screen.transform_point2(center);
        let screen_extents = self.to_screen.transform_vector2(extents);
        let rect = Rect::new(
            (screen_pos.x - screen_extents.x) as i32,
            (screen_pos.y + screen_extents.y) as i32,
            (screen_extents.x * 2.0) as u32,
            (-screen_extents.y * 2.0) as u32,
        );
        // self.canvas.set_draw_color(color);
        // self.canvas.fill_rect(rect).unwrap();
    }

    pub fn draw_wire_rect(&mut self, center: Vec2, extents: Vec2, color: Color) {
        let screen_pos = self.to_screen.transform_point2(center);
        let screen_extents = self.to_screen.transform_vector2(extents);
        let rect = Rect::new(
            (screen_pos.x - screen_extents.x) as i32,
            (screen_pos.y + screen_extents.y) as i32,
            (screen_extents.x * 2.0) as u32,
            (-screen_extents.y * 2.0) as u32,
        );
        // self.canvas.set_draw_color(color);
        // self.canvas.draw_rect(rect).unwrap();
    }

    pub fn draw_circle(&mut self, center: Vec2, radius: f32, color: Color) {
        let screen_center = self.to_screen.transform_point2(center);
        let screen_radius = self.to_screen.transform_vector2(vec2(radius, radius)).x;
        self.draw_circle_internal(
            screen_center.x as i32,
            screen_center.y as i32,
            screen_radius as i32,
            color,
        );
    }

    fn draw_circle_internal(&mut self, x: i32, y: i32, radius: i32, color: Color) {
        // self.canvas.set_draw_color(color);
        // for w in 0..radius * 2 {
        //     for h in 0..radius * 2 {
        //         let dx = radius - w; // horizontal offset
        //         let dy = radius - h; // vertical offset
        //         if (dx * dx + dy * dy) <= (radius * radius) {
        //             self.canvas.draw_point((x + dx, y + dy)).unwrap();
        //         }
        //     }
        // }
    }
}

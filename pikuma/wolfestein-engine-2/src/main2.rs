extern crate sdl2;

use sdl2::event::Event;
use sdl2::keyboard::Keycode;
use sdl2::pixels::{Color, PixelFormatEnum};

const WINDOW_WIDTH: u32 = 800;
const WINDOW_HEIGHT: u32 = 600;

fn main() {
    let sdl_context = sdl2::init().unwrap();
    let video_subsystem = sdl_context.video().unwrap();

    let window = video_subsystem
        .window("Color Buffer Example", WINDOW_WIDTH, WINDOW_HEIGHT)
        .position_centered()
        .build()
        .unwrap();

    let mut canvas = window.into_canvas().software().build().unwrap();

    // Create a color buffer
    let mut color_buffer: Vec<u32> = vec![0; (WINDOW_WIDTH * WINDOW_HEIGHT) as usize];

    // Example: Fill the color buffer with a gradient pattern
    for y in 0..WINDOW_HEIGHT {
        for x in 0..WINDOW_WIDTH {
            let offset = (y * WINDOW_WIDTH + x) as usize;
            color_buffer[offset] = ((x as u32) << 24) | ((y as u32) << 16) | (128 << 8) | 255;
        }
    }

    let texture_creator = canvas.texture_creator();
    let mut color_buffer_texture = texture_creator
        .create_texture_streaming(PixelFormatEnum::ARGB8888, WINDOW_WIDTH, WINDOW_HEIGHT)
        .unwrap();

    color_buffer_texture
        .with_lock(None, |buffer: &mut [u8], pitch: usize| {
            for y in 0..WINDOW_HEIGHT {
                for x in 0..WINDOW_WIDTH {
                    let offset = y as usize * pitch + x as usize * 4;
                    let pixel = color_buffer[(y * WINDOW_WIDTH + x) as usize];
                    buffer[offset..offset + 4].copy_from_slice(&pixel.to_le_bytes());
                }
            }
        })
        .unwrap();

    let mut event_pump = sdl_context.event_pump().unwrap();

    'running: loop {
        for event in event_pump.poll_iter() {
            match event {
                Event::Quit { .. }
                | Event::KeyDown {
                    keycode: Some(Keycode::Escape),
                    ..
                } => break 'running,
                _ => {}
            }
        }

        canvas.set_draw_color(Color::BLACK);
        canvas.clear();

        canvas.copy(&color_buffer_texture, None, None).unwrap();

        canvas.present();
    }
}

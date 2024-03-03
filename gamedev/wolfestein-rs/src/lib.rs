pub mod game {
    use error_iter::ErrorIter as _;
    use log::{error, info};
    use pixels::{Pixels, SurfaceTexture};
    use winit::dpi::{LogicalPosition, LogicalSize};
    use winit::event::{Event, WindowEvent};
    use winit::event_loop::{ControlFlow, EventLoop};
    use winit::keyboard::KeyCode;
    use winit::window::{Fullscreen, WindowBuilder};
    use winit_input_helper::WinitInputHelper;

    pub fn run() {
        const WIDTH: f64 = 1920.*0.5;
        const HEIGHT: f64 = 1080. *0.5;
        let event_loop = EventLoop::new().unwrap();
        let mut input = WinitInputHelper::new();
        let window = {
            let size = LogicalSize::new(WIDTH as f64, HEIGHT as f64);
            WindowBuilder::new()
                // .with_fullscreen(Some(Fullscreen::Borderless(None)))
                .with_inner_size(size)
                .with_min_inner_size(size)
                .with_max_inner_size(size)
                .with_resizable(false)
                .with_title("Wolfestein-rs")
                .build(&event_loop)
                .unwrap()
        };

        if let Some(monitor) = window.current_monitor() {
            let monitor_size = monitor.size();
            let window_size = window.inner_size();

            info!("{:?}, {:?}", &monitor_size, &window_size);

            let position = LogicalPosition::new(
                (monitor_size.width - window_size.width) as f64 / 3.0,
                (monitor_size.height - window_size.height) as f64 / 3.0,
            );

            // Set the window's position
            window.set_outer_position(position);
        }

        let mut pixels = {
            let window_size = window.inner_size();
            let surface_texture =
                SurfaceTexture::new(window_size.width, window_size.height, &window);
            pixels::PixelsBuilder::new(window_size.width, window_size.height, surface_texture)
                .build()
                .unwrap()
        };

        event_loop.set_control_flow(ControlFlow::Poll);

        event_loop
            .run(move |event, elwt| {
                if let Event::WindowEvent {
                    event: WindowEvent::Resized(new_size),
                    ..
                } = event
                {
                    info!("RESIZING");
                    pixels
                        .resize_buffer(new_size.width, new_size.height)
                        .unwrap();
                    pixels
                        .resize_surface(new_size.width, new_size.height)
                        .unwrap();
                }

                if let Event::AboutToWait = event {
                    render(&mut pixels);
                    if let Err(e) = pixels.render() {
                        log_error("pixels.render", e);
                        elwt.exit();
                    }
                }

                if input.update(&event) {
                    if input.key_pressed(KeyCode::Escape) || input.close_requested() {
                        elwt.exit();
                        return;
                    }

                    if input.key_pressed(KeyCode::F11) {
                        match window.fullscreen() {
                            Some(_) => window.set_fullscreen(None),
                            None => window
                                .set_fullscreen(Some(winit::window::Fullscreen::Borderless(None))),
                        };
                    }
                }
            })
            .unwrap();
    }

    fn render(pixels: &mut Pixels) {
        for pixel in pixels.frame_mut().chunks_exact_mut(4) {
            pixel[0] = 0; // R
            pixel[1] = 0; // G
            pixel[2] = 255; // B
            pixel[3] = 255; // A
        }
    }

    fn log_error<E: std::error::Error + 'static>(method_name: &str, err: E) {
        error!("{method_name}() failed: {err}");
        for source in err.sources().skip(1) {
            error!("  Caused by: {source}");
        }
    }
}

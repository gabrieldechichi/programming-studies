pub mod game {
    use macroquad::prelude::*;

    pub struct Map {
        pub grid: Vec<Vec<u8>>,
        pub row_count: usize,
        pub column_count: usize,
        pub tile_size: usize,
    }

    impl Map {
        pub fn width(&self) -> u16 {
            (self.column_count * self.tile_size) as u16
        }

        pub fn height(&self) -> u16 {
            (self.row_count * self.tile_size) as u16
        }
    }

    pub async fn run() {
        let grid: Vec<Vec<u8>> = vec![
            vec![1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
            vec![1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1],
            vec![1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1],
            vec![1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1],
            vec![1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1],
            vec![1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1],
            vec![1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
            vec![1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
            vec![1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1],
            vec![1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
            vec![1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
        ];

        let row_count = grid.len();
        let column_count = grid[0].len();

        let map = Map {
            grid,
            row_count,
            column_count,
            tile_size: 32,
        };

        let delta = vec2(
            (screen_width() - map.width() as f32) * 0.5,
            (screen_height() - map.height() as f32) * 0.5,
        );

        loop {
            clear_background(WHITE);

            for (y, line) in map.grid.iter().enumerate() {
                for (x, cell) in line.iter().enumerate() {
                    let tile_size: f32 = map.tile_size as f32;
                    let tile_x: f32 = x as f32 * tile_size + delta.x;
                    let tile_y: f32 = y as f32 * tile_size + delta.y;

                    match cell {
                        0 => {
                            draw_rectangle_lines(tile_x, tile_y, tile_size, tile_size, 1.0, BLACK);
                        }
                        1 => {
                            draw_rectangle(tile_x, tile_y, tile_size, tile_size, BLACK);
                        }
                        _ => (),
                    };
                }
            }
            next_frame().await;
        }
    }
}

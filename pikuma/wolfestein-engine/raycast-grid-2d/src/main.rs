use raycast_grid_2d::game;

#[macroquad::main("2D Grid")]
async fn main() {
    game::run().await;
}

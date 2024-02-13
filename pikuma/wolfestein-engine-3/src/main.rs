use wolfestein_engine_3::game;

#[macroquad::main("Wolfestein v3")]
async fn main() {
    game::run().await;
}

use bevy::prelude::*;

#[derive(Component, Default, Reflect)]
#[reflect(Component)]
pub struct MainCamera;

#[derive(Component)]
pub struct ReloadableComponent;

#[derive(Reflect, Default, Component)]
#[reflect(Component)]
pub enum SpriteLayer {
    #[default]
    Background,
    Midground,
    Foreground,
    Player,
}

#[derive(States, Default, Debug, Clone, Eq, PartialEq, Hash)]
pub enum EngineState {
    #[default]
    Game,
    Editor,
}

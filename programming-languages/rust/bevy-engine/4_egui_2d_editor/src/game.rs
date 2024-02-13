use crate::common::*;
use crate::level_editor::*;
use bevy::math::*;
use bevy::prelude::*;
use dexterous_developer::*;

const PIXELS_PER_UNIT: u32 = 256;
const SCENE_FILE_PATH: &str = "scenes/level_1.scn.ron";

#[derive(Reflect, Default, Component)]
#[reflect(Component)]
struct AddComputedVisibility;

#[hot_bevy_main]
pub fn bevy_main(initial_plugins: impl InitialPlugins) {
    App::new()
        .add_plugins(DefaultPlugins)
        .register_type::<SpriteLayer>()
        .add_state::<EngineState>()
        .add_plugins(LevelEditorPlugin::new(SCENE_FILE_PATH, PIXELS_PER_UNIT))
        .add_systems(Startup, setup)
        .setup_reloadable_elements::<reloadable>()
        .run();
}

#[dexterous_developer_setup]
fn reloadable(app: &mut ReloadableAppContents) {
    println!("Setting up reloadabless");
    app.add_systems(Update, (update_sprite_layer, add_computed_visibility));
    println!("Done");
}

fn setup(mut commands: Commands, asset_server: Res<AssetServer>) {
    commands.spawn((
        DynamicSceneBundle {
            scene: asset_server.load(SCENE_FILE_PATH),
            ..default()
        },
        Name::from("level_1"),
    ));
    commands.spawn((
        Camera2dBundle {
            projection: OrthographicProjection {
                scale: 0.01,
                ..default()
            },
            ..default()
        },
        MainCamera,
        Name::from("Main Camera"),
    ));
}

fn add_computed_visibility(
    mut commands: Commands,
    mut query: Query<(Entity, With<Visibility>, Without<ComputedVisibility>)>,
) {
    for (entity, _, _) in &mut query {
        commands
            .entity(entity)
            .insert(ComputedVisibility::default());
    }
}

fn update_sprite_layer(
    mut sprite_layers: Query<(&mut Transform, &SpriteLayer), Changed<SpriteLayer>>,
) {
    for (mut transform, layer) in &mut sprite_layers {
        transform.translation.z = match layer {
            SpriteLayer::Background => -100.,
            SpriteLayer::Midground => -50.,
            SpriteLayer::Player => -25.,
            SpriteLayer::Foreground => -10.,
        }
    }
}

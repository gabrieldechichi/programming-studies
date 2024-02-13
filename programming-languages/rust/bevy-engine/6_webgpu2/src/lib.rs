//! Shows how to render simple primitive shapes with a single color.

use bevy::{prelude::*, window::close_on_esc};
use dexterous_developer::*;

#[hot_bevy_main]
pub fn bevy_main(initial_plugins: impl InitialPlugins) {
    App::new()
        .add_plugins(initial_plugins.initialize::<DefaultPlugins>())
        .insert_resource(ClearColor(Color::rgb_u8(255, 0, 255)))
        .add_systems(Startup, setup)
        .add_systems(Update, close_on_esc)
        .setup_reloadable_elements::<reloadable>()
        .run();
}

#[dexterous_developer_setup]
fn reloadable(_app: &mut ReloadableAppContents) {
    println!("Setting up reloadabless");
    // app.add_systems(Update, (update_sprite_layer, add_computed_visibility));
    println!("Done");
}

fn setup(
    mut commands: Commands,
    // mut meshes: ResMut<Assets<Mesh>>,
    // mut materials: ResMut<Assets<ColorMaterial>>,
) {
    commands.spawn(Camera2dBundle::default());

    // // Circle
    // commands.spawn(MaterialMesh2dBundle {
    //     mesh: meshes.add(shape::Circle::new(50.).into()).into(),
    //     material: materials.add(ColorMaterial::from(Color::PURPLE)),
    //     transform: Transform::from_translation(Vec3::new(-150., 0., 0.)),
    //     ..default()
    // });

    // // Rectangle
    // commands.spawn(SpriteBundle {
    //     sprite: Sprite {
    //         color: Color::rgb(0.25, 0.25, 0.75),
    //         custom_size: Some(Vec2::new(50.0, 100.0)),
    //         ..default()
    //     },
    //     transform: Transform::from_translation(Vec3::new(-50., 0., 0.)),
    //     ..default()
    // });

    // // Quad
    // commands.spawn(MaterialMesh2dBundle {
    //     mesh: meshes
    //         .add(shape::Quad::new(Vec2::new(50., 100.)).into())
    //         .into(),
    //     material: materials.add(ColorMaterial::from(Color::LIME_GREEN)),
    //     transform: Transform::from_translation(Vec3::new(50., 0., 0.)),
    //     ..default()
    // });

    // // Hexagon
    // commands.spawn(MaterialMesh2dBundle {
    //     mesh: meshes.add(shape::RegularPolygon::new(50., 6).into()).into(),
    //     material: materials.add(ColorMaterial::from(Color::TURQUOISE)),
    //     transform: Transform::from_translation(Vec3::new(150., 0., 0.)),
    //     ..default()
    // });
}

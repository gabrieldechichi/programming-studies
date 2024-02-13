use bevy::{
    math::{vec2, vec3},
    prelude::*,
    sprite::{
        collide_aabb::{
            collide,
            Collision::{self},
        },
        MaterialMesh2dBundle,
    },
};
use dexterous_developer::{
    dexterous_developer_setup, hot_bevy_main, InitialPlugins, ReloadableApp, ReloadableAppContents,
    ReloadableElementsSetup,
};

//paddle
const PADDLE_START_Y: f32 = BOTTOM_WALL + 60.;
const PADDLE_SIZE: Vec3 = Vec3::new(120.0, 20.0, 0.0);
const PADDLE_COLOR: Color = Color::rgb(0.3, 0.3, 0.7);
const PADDLE_SPEED: f32 = 500.0;
const BACKGROUND_COLOR: Color = Color::rgb(0.9, 0.9, 0.9);

//ball
const BALL_COLOR: Color = Color::rgb(1.0, 0.5, 0.5);
const BALL_STARTING_POSITION: Vec3 = Vec3::new(0.0, -50.0, 1.0);
const BALL_SIZE: Vec3 = Vec3::new(30.0, 30.0, 0.0);
const BALL_SPEED: f32 = 400.0;
const BALL_INITIAL_DIRECTION: Vec2 = Vec2::new(0.5, -0.5);

//wall
const WALL_THICKNESS: f32 = 10.0;
const WALL_COLOR: Color = Color::rgb(0.8, 0.8, 0.8);
const LEFT_WALL: f32 = -450.;
const RIGHT_WALL: f32 = 450.;
const BOTTOM_WALL: f32 = -300.;
const TOP_WALL: f32 = 300.;

//bricks
const BRICK_SIZE: Vec2 = Vec2::new(100., 30.);
const BRICK_COLOR: Color = Color::rgb(0.5, 0.5, 1.0);
const GAP_BETWEEN_PADDLE_AND_BRICKS: f32 = 270.0;
const GAP_BETWEEN_BRICKS: f32 = 5.0;
const GAP_BETWEEN_BRICKS_AND_CEILING: f32 = 20.0;
const GAP_BETWEEN_BRICKS_AND_SIDES: f32 = 20.0;

//scoreboard
const SCOREBOARD_FONT_SIZE: f32 = 40.0;
const SCOREBOARD_TEXT_PADDING: Val = Val::Px(5.0);
const TEXT_COLOR: Color = Color::rgb(0.5, 0.5, 1.0);
const SCORE_COLOR: Color = Color::rgb(1.0, 0.5, 0.5);

#[hot_bevy_main]
pub fn bevy_main(initial_plugins: impl InitialPlugins) {
    App::new()
        .add_plugins(initial_plugins.initialize::<DefaultPlugins>())
        .add_event::<CollisionEvent>()
        .setup_reloadable_elements::<reloadable>()
        .run();
}

#[dexterous_developer_setup]
fn reloadable(app: &mut ReloadableAppContents) {
    println!("Setting up reloadabless");
    app.add_systems(
        FixedUpdate,
        (
            move_paddle,
            check_ball_collisions,
            apply_velocity.before(check_ball_collisions),
            play_collision_sound.after(check_ball_collisions),
        ),
    );
    app.add_systems(Update, (update_scoreboard, bevy::window::close_on_esc));
    app.reset_resource_to_value(Scoreboard { score: 0 });
    app.reset_resource_to_value(ClearColor(BACKGROUND_COLOR));
    app.reset_resource::<CollisionSound>();
    app.reset_setup::<ReloadableComponent, _>(setup_reloadable);
    println!("Done");
}

#[derive(Component)]
struct ReloadableComponent;

#[derive(Component)]
struct Paddle;

#[derive(Component)]
struct Ball;

#[derive(Component)]
struct Collider;

#[derive(Component)]
struct Brick;

#[derive(Component, Deref, DerefMut)]
struct Velocity(Vec2);

#[derive(Resource, Clone, Copy)]
struct Scoreboard {
    score: usize,
}

#[derive(Event, Default)]
struct CollisionEvent;

#[derive(Resource, Default, Deref, DerefMut)]
struct CollisionSound(Handle<AudioSource>);

enum WallLocation {
    Left,
    Right,
    Bottom,
    Top,
}

impl WallLocation {
    fn position(&self) -> Vec2 {
        match self {
            WallLocation::Left => vec2(LEFT_WALL, 0.),
            WallLocation::Right => vec2(RIGHT_WALL, 0.),
            WallLocation::Bottom => vec2(0., BOTTOM_WALL),
            WallLocation::Top => vec2(0., TOP_WALL),
        }
    }

    fn size(&self) -> Vec2 {
        let arena_width = RIGHT_WALL - LEFT_WALL;
        let arena_height = TOP_WALL - BOTTOM_WALL;
        match self {
            WallLocation::Left | WallLocation::Right => {
                vec2(WALL_THICKNESS, arena_height + WALL_THICKNESS)
            }
            WallLocation::Bottom | WallLocation::Top => {
                vec2(arena_width + WALL_THICKNESS, WALL_THICKNESS)
            }
        }
    }
}

#[derive(Bundle)]
struct WallBundle {
    sprite_bundle: SpriteBundle,
    collider: Collider,
}

impl WallBundle {
    fn new(location: WallLocation) -> Self {
        Self {
            sprite_bundle: SpriteBundle {
                transform: Transform {
                    translation: location.position().extend(0.),
                    scale: location.size().extend(1.),
                    ..default()
                },
                sprite: Sprite {
                    color: WALL_COLOR,
                    ..default()
                },
                ..default()
            },
            collider: Collider,
        }
    }
}

fn setup_reloadable(
    mut commands: Commands,
    mut meshes: ResMut<Assets<Mesh>>,
    mut materials: ResMut<Assets<ColorMaterial>>,
    asset_server: Res<AssetServer>,
) {
    //camera
    commands.spawn((Camera2dBundle::default(), ReloadableComponent));

    //sound
    let ball_collision_sound = asset_server.load("sounds/breakout_collision.ogg");
    commands.insert_resource(CollisionSound(ball_collision_sound));

    //paddle
    commands.spawn((
        SpriteBundle {
            transform: Transform {
                translation: vec3(0., PADDLE_START_Y, 0.),
                scale: PADDLE_SIZE,
                ..default()
            },
            sprite: Sprite {
                color: PADDLE_COLOR,
                ..default()
            },
            ..default()
        },
        Paddle,
        Collider,
        ReloadableComponent,
    ));

    //ball
    commands.spawn((
        MaterialMesh2dBundle {
            mesh: meshes.add(shape::Circle::default().into()).into(),
            material: materials.add(ColorMaterial::from(BALL_COLOR)),
            transform: Transform::from_translation(BALL_STARTING_POSITION).with_scale(BALL_SIZE),
            ..default()
        },
        Ball,
        Velocity(BALL_SPEED * BALL_INITIAL_DIRECTION),
        ReloadableComponent,
    ));

    //walls
    commands.spawn((WallBundle::new(WallLocation::Left), ReloadableComponent));
    commands.spawn((WallBundle::new(WallLocation::Right), ReloadableComponent));
    commands.spawn((WallBundle::new(WallLocation::Bottom), ReloadableComponent));
    commands.spawn((WallBundle::new(WallLocation::Top), ReloadableComponent));

    //bicks
    {
        let offset_x = LEFT_WALL + GAP_BETWEEN_BRICKS_AND_SIDES + BRICK_SIZE.x * 0.5;
        let offset_y = BOTTOM_WALL + GAP_BETWEEN_PADDLE_AND_BRICKS + BRICK_SIZE.y * 0.5;

        let bricks_total_width = (RIGHT_WALL - LEFT_WALL) - 2. * GAP_BETWEEN_BRICKS_AND_SIDES;
        let bricks_total_height = (TOP_WALL - BOTTOM_WALL)
            - GAP_BETWEEN_BRICKS_AND_CEILING
            - GAP_BETWEEN_PADDLE_AND_BRICKS;

        let rows = (bricks_total_height / (BRICK_SIZE.y + GAP_BETWEEN_BRICKS)).floor() as i32;
        let columns = (bricks_total_width / (BRICK_SIZE.x + GAP_BETWEEN_BRICKS)).floor() as i32;

        for row in 0..rows {
            for column in 0..columns {
                let brick_pos = vec2(
                    offset_x + column as f32 * (BRICK_SIZE.x + GAP_BETWEEN_BRICKS),
                    offset_y + row as f32 * (BRICK_SIZE.y + GAP_BETWEEN_BRICKS),
                );

                commands.spawn((
                    SpriteBundle {
                        sprite: Sprite {
                            color: BRICK_COLOR,
                            ..default()
                        },
                        transform: Transform::from_translation(brick_pos.extend(0.0))
                            .with_scale(BRICK_SIZE.extend(1.0)),
                        ..default()
                    },
                    Brick,
                    Collider,
                    ReloadableComponent,
                ));
            }
        }
    }

    //Scoreboard
    commands.spawn((
        ReloadableComponent,
        TextBundle::from_sections([
            TextSection::new(
                "Score: ",
                TextStyle {
                    font_size: SCOREBOARD_FONT_SIZE,
                    color: TEXT_COLOR,
                    ..default()
                },
            ),
            TextSection::from_style(TextStyle {
                font_size: SCOREBOARD_FONT_SIZE,
                color: SCORE_COLOR,
                ..default()
            }),
        ])
        .with_style(Style {
            position_type: PositionType::Absolute,
            top: SCOREBOARD_TEXT_PADDING,
            left: SCOREBOARD_TEXT_PADDING,
            ..default()
        }),
    ));
}

fn move_paddle(
    input: Res<Input<KeyCode>>,
    mut query: Query<&mut Transform, With<Paddle>>,
    time_step: Res<FixedTime>,
) {
    let mut paddle_transform = query.single_mut();
    let mut direction = 0.0;

    if input.pressed(KeyCode::A) {
        direction -= 1.0;
    }
    if input.pressed(KeyCode::D) {
        direction += 1.0;
    }

    let mut new_x =
        paddle_transform.translation.x + direction * PADDLE_SPEED * time_step.period.as_secs_f32();

    new_x = new_x.min(RIGHT_WALL - (WALL_THICKNESS + PADDLE_SIZE.x) * 0.5);
    new_x = new_x.max(LEFT_WALL + (WALL_THICKNESS + PADDLE_SIZE.x) * 0.5);

    paddle_transform.translation.x = new_x;
}

fn apply_velocity(mut query: Query<(&mut Transform, &Velocity)>, time_step: Res<FixedTime>) {
    let dt = time_step.period.as_secs_f32();
    for (mut transform, velocity) in &mut query {
        transform.translation.x += velocity.x * dt;
        transform.translation.y += velocity.y * dt;
    }
}

fn check_ball_collisions(
    mut commands: Commands,
    mut score: ResMut<Scoreboard>,
    mut ball_query: Query<(&mut Velocity, &Transform), With<Ball>>,
    collider_query: Query<(Entity, &Transform, Option<&Brick>), With<Collider>>,
    mut collision_events: EventWriter<CollisionEvent>,
) {
    let (mut ball_velocity, ball_transform) = ball_query.single_mut();
    let ball_size = ball_transform.scale.truncate();
    let ball_pos = ball_transform.translation;
    for (collider_entity, transform, maybe_brick) in &collider_query {
        let collision = collide(
            ball_pos,
            ball_size,
            transform.translation,
            transform.scale.truncate(),
        );

        let mut reflect_x = false;
        let mut reflect_y = false;
        if let Some(collision) = collision {
            collision_events.send_default();
            match collision {
                Collision::Left => reflect_x = ball_velocity.x > 0.0,
                Collision::Right => reflect_x = ball_velocity.x < 0.0,
                Collision::Top => reflect_y = ball_velocity.y < 0.0,
                Collision::Bottom => reflect_y = ball_velocity.y > 0.0,
                Collision::Inside => { /* do nothing */ }
            }

            if reflect_x {
                ball_velocity.x *= -1.;
            }
            if reflect_y {
                ball_velocity.y *= -1.;
            }

            if maybe_brick.is_some() {
                score.score += 1;
                commands.entity(collider_entity).despawn();
            }
        }
    }
}

fn play_collision_sound(
    mut commands: Commands,
    mut collision_events: EventReader<CollisionEvent>,
    sound: Res<CollisionSound>,
) {
    if !collision_events.is_empty() {
        collision_events.clear();
        commands.spawn(AudioBundle {
            source: sound.clone(),
            settings: PlaybackSettings::DESPAWN,
        });
    }
}

fn update_scoreboard(score: Res<Scoreboard>, mut query: Query<&mut Text>) {
    let mut text = query.single_mut();
    text.sections[1].value = score.score.to_string();
}

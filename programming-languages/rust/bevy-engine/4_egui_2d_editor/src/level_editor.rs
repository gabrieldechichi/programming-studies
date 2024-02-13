use crate::common::*;
use std::fs::File;
use std::io::Write;

use bevy::app::AppExit;
use bevy::ecs::system::SystemState;
use bevy::input::mouse::{MouseMotion, MouseWheel};
use bevy::math::*;
use bevy::prelude::*;
use bevy::render::camera::{CameraProjection, Viewport};
use bevy::render::primitives::Aabb;
use bevy::scene::SceneInstance;
use bevy::tasks::IoTaskPool;
use bevy::window::PrimaryWindow;
use bevy_inspector_egui::bevy_egui::egui::{self, Align, ImageButton, Layout};
use bevy_inspector_egui::bevy_egui::{self, EguiContext, EguiPlugin, EguiUserTextures};
use bevy_inspector_egui::bevy_inspector;
use bevy_inspector_egui::bevy_inspector::hierarchy::{Hierarchy, SelectedEntities};
use bevy_inspector_egui::bevy_inspector::{
    ui_for_entities_shared_components, ui_for_entity_with_children,
};
use bevy_inspector_egui::DefaultInspectorConfigPlugin;
use dexterous_developer::*;
use egui_dock::{DockArea, DockState, NodeIndex};
use egui_gizmo::{Gizmo, GizmoMode, GizmoOrientation};

pub struct LevelEditorPlugin {
    level_path: &'static str,
    pixels_per_unit: u32,
}

impl Plugin for LevelEditorPlugin {
    fn build(&self, app: &mut App) {
        app.add_plugins((EguiPlugin, DefaultInspectorConfigPlugin))
            .register_type::<Option<Vec2>>()
            .register_type::<Option<Rect>>()
            .init_resource::<SpriteHandles>()
            .insert_resource(EditorData::new(self.level_path, self.pixels_per_unit))
            .insert_resource(EditorTabs::new())
            .add_systems(Startup, setup_level_editor)
            .setup_reloadable_elements::<editor_reloadables>();
    }
}

impl LevelEditorPlugin {
    pub fn new(level_path: &'static str, pixels_per_unit: u32) -> Self {
        Self {
            level_path,
            pixels_per_unit,
        }
    }
}

#[derive(Resource, Default)]
struct SpriteHandles {
    handles: Vec<HandleUntyped>,
}

#[derive(Component)]
struct EditorCamera;

#[derive(Debug)]
enum EditorWindow {
    SceneView,
    Hierarchy,
    Inspector,
    Assets,
    Resources,
}

#[derive(Resource)]
struct EditorData {
    viewport_rect: egui::Rect,
    gizmo_mode: GizmoMode,
    gizmo_orientation: GizmoOrientation,
    selected_entities: SelectedEntities,
    level_path: String,
    pixels_per_unit: u32,
}

impl EditorData {
    fn new(level_path: &str, pixels_per_unit: u32) -> Self {
        Self {
            viewport_rect: egui::Rect::NOTHING,
            gizmo_mode: GizmoMode::Translate,
            gizmo_orientation: GizmoOrientation::Local,
            selected_entities: SelectedEntities::default(),
            level_path: level_path.to_string(),
            pixels_per_unit,
        }
    }
    fn sprite_scale(&self) -> Vec3 {
        let s = 1. / self.pixels_per_unit as f32;
        vec3(s, s, s)
    }
}

#[derive(Resource)]
struct EditorTabs {
    tree: DockState<EditorWindow>,
}

impl EditorTabs {
    fn new() -> Self {
        let mut tree = DockState::new(vec![EditorWindow::SceneView]);
        let [game, _inspector] = tree.main_surface_mut().split_right(
            NodeIndex::root(),
            0.75,
            vec![EditorWindow::Inspector],
        );
        let [game, _hierarchy] =
            tree.main_surface_mut()
                .split_left(game, 0.2, vec![EditorWindow::Hierarchy]);
        let [_game, _bottom] = tree.main_surface_mut().split_below(
            game,
            0.8,
            vec![EditorWindow::Assets, EditorWindow::Resources],
        );
        Self { tree }
    }
}

struct EditorWindowDrawer<'a> {
    world: &'a mut World,
    editor_data: &'a mut EditorData,
}

impl egui_dock::TabViewer for EditorWindowDrawer<'_> {
    type Tab = EditorWindow;

    fn ui(&mut self, ui: &mut egui::Ui, tab: &mut Self::Tab) {
        match tab {
            EditorWindow::SceneView => {
                self.editor_data.viewport_rect = ui.clip_rect();
                scene_view_controls(ui, self.world, self.editor_data);
            }
            EditorWindow::Hierarchy => {
                draw_hierarchy(self.world, self.editor_data, ui);
            }
            EditorWindow::Inspector => {
                draw_inspector(self.world, self.editor_data, ui);
            }
            EditorWindow::Assets => {
                draw_assets_panel(self.world, self.editor_data, ui);
            }
            EditorWindow::Resources => {
                bevy_inspector::ui_for_resources(self.world, ui);
            }
        }
    }

    fn title(&mut self, tab: &mut Self::Tab) -> egui::WidgetText {
        format!("{tab:?}").into()
    }

    fn clear_background(&self, window: &Self::Tab) -> bool {
        !matches!(window, EditorWindow::SceneView)
    }
}

#[dexterous_developer_setup]
fn editor_reloadables(app: &mut ReloadableAppContents) {
    println!("Setting up editor reloadables");
    app.add_systems(OnEnter(EngineState::Editor), enable_editor_camera);
    app.add_systems(OnExit(EngineState::Editor), disable_editor_camera);
    app.add_systems(
        Update,
        (
            toggle_editor,
            level_editor_system.run_if(in_state(EngineState::Editor)),
        ),
    );
    // app.add_systems(Last, save_scene_system);
    println!("Done");
}

fn toggle_editor(
    input: Res<Input<KeyCode>>,
    current_state: Res<State<EngineState>>,
    mut next_state: ResMut<NextState<EngineState>>,
) {
    if input.just_pressed(KeyCode::F11) {
        match current_state.get() {
            EngineState::Game => next_state.set(EngineState::Editor),
            EngineState::Editor => next_state.set(EngineState::Game),
        }
    }
}

fn enable_editor_camera(
    mut editor_cameras: Query<&mut Camera, (With<EditorCamera>, Without<MainCamera>)>,
    mut game_cameras: Query<&mut Camera, (With<MainCamera>, Without<EditorCamera>)>,
) {
    for mut cam in &mut editor_cameras {
        cam.is_active = true;
    }
    for mut cam in &mut game_cameras {
        cam.is_active = false;
    }
}

fn disable_editor_camera(
    mut editor_cameras: Query<&mut Camera, (With<EditorCamera>, Without<MainCamera>)>,
    mut game_cameras: Query<&mut Camera, (With<MainCamera>, Without<EditorCamera>)>,
) {
    for mut cam in &mut editor_cameras {
        cam.is_active = false;
    }
    for mut cam in &mut game_cameras {
        cam.is_active = true;
    }
}

fn setup_level_editor(
    mut commands: Commands,
    asset_server: Res<AssetServer>,
    mut sprite_handles: ResMut<SpriteHandles>,
) {
    sprite_handles.handles = asset_server.load_folder("sprites").unwrap();
    commands.spawn((
        Camera2dBundle {
            camera: Camera {
                order: 1,
                is_active: false,
                ..default()
            },
            projection: OrthographicProjection {
                scale: 0.01,
                ..default()
            },
            ..default()
        },
        EditorCamera,
        Name::from("Editor Camera"),
    ));
}

fn build_dynamic_scene(world: &mut World) -> Option<DynamicScene> {
    //Temporarily remove scene children (avoid infinite nestnig)
    let scene_children = {
        let mut system_state =
            SystemState::<(Commands, Query<(Entity, &Children), With<SceneInstance>>)>::new(world);

        let (mut commands, q_scenes) = system_state.get_mut(world);

        let Ok((scene, children)) = q_scenes.get_single() else {
            return None;
        };

        let children: Vec<Entity> = children.iter().map(|e| *e).collect();

        commands.entity(scene).clear_children();
        system_state.apply(world);
        children
    };

    let scene = {
        fn extract_entity_recursive(
            entity: Entity,
            q_children: &Query<&Children>,
            scene_builder: &mut DynamicSceneBuilder,
        ) {
            scene_builder.extract_entity(entity);
            match q_children.get(entity) {
                Ok(children) => {
                    for &child in children.iter() {
                        extract_entity_recursive(child, q_children, scene_builder);
                    }
                }
                Err(_) => (),
            }
        }

        let mut system_state = SystemState::<Query<&Children>>::new(world);
        let q_children = system_state.get(world);

        let mut scene_builder = DynamicSceneBuilder::from_world(world);
        scene_builder.allow_all();
        scene_builder.allow_all_resources();
        scene_builder.deny::<ComputedVisibility>();
        scene_builder.deny::<Handle<DynamicScene>>();
        scene_builder.deny_all_resources();
        scene_builder.allow_resource::<ClearColor>();
        scene_builder.extract_resources();

        for scene_child in scene_children.iter() {
            extract_entity_recursive(*scene_child, &q_children, &mut scene_builder);
        }

        scene_builder.build()
    };

    //re-add scene children
    {
        let mut system_state =
            SystemState::<(Commands, Query<Entity, With<SceneInstance>>)>::new(world);
        let (mut commands, q_scenes) = system_state.get_mut(world);

        let Ok(scene) = q_scenes.get_single() else {
            return None;
        };

        commands.entity(scene).replace_children(&scene_children);
        system_state.apply(world);
    }

    Some(scene)
}

fn save_scene(world: &mut World, file_path: String) {
    let Some(scene) = build_dynamic_scene(world) else {
        return;
    };
    let type_registry = world.resource::<AppTypeRegistry>();
    match scene.serialize_ron(type_registry) {
        Ok(serialized_scene) => {
            println!("{}", serialized_scene);
            #[cfg(not(target_arch = "wasm32"))]
            IoTaskPool::get()
                .spawn(async move {
                    // Write the scene RON data to file
                    File::create(format!("assets/{file_path}"))
                        .and_then(|mut file| file.write(serialized_scene.as_bytes()))
                        .expect("Error while writing scene to file");
                })
                .detach();
        }
        Err(e) => println!("Error {}", e),
    }
}

fn scene_view_controls(ui: &mut egui::Ui, world: &mut World, editor_data: &mut EditorData) {
    //update gizmo mode
    {
        let mut system_state = SystemState::<Res<Input<KeyCode>>>::new(world);
        let input = system_state.get_mut(world);
        for (key, mode) in [
            (KeyCode::T, GizmoMode::Translate),
            (KeyCode::R, GizmoMode::Rotate),
            (KeyCode::S, GizmoMode::Scale),
        ] {
            if input.just_pressed(key) {
                editor_data.gizmo_mode = mode;
            }
        }
        for (key, orientation) in [
            (KeyCode::G, GizmoOrientation::Global),
            (KeyCode::L, GizmoOrientation::Local),
        ] {
            if input.just_pressed(key) {
                editor_data.gizmo_orientation = orientation;
            }
        }
    };

    //update camera viewport
    {
        let mut system_state = SystemState::<(
            Res<bevy_egui::EguiSettings>,
            Query<&Window, With<PrimaryWindow>>,
            Query<&mut Camera, With<EditorCamera>>,
        )>::new(world);
        let (egui_settings, q_windows, mut q_cameras) = system_state.get_mut(world);

        let window = q_windows.single();

        let scale_factor = window.scale_factor() * egui_settings.scale_factor;
        // let scale_factor = 1;

        let viewport_pos = editor_data.viewport_rect.left_top().to_vec2() * scale_factor as f32;
        let viewport_size = editor_data.viewport_rect.size() * scale_factor as f32;

        for mut camera in &mut q_cameras {
            camera.viewport = Some(Viewport {
                physical_position: UVec2::new(viewport_pos.x as u32, viewport_pos.y as u32),
                physical_size: UVec2::new(viewport_size.x as u32, viewport_size.y as u32),
                depth: 0.0..0.0,
            });
        }

        system_state.apply(world);
    }

    let is_cursor_inside_viewport = {
        let mut system_state = SystemState::<(
            Query<&Camera, With<EditorCamera>>,
            Query<&Window, With<PrimaryWindow>>,
        )>::new(world);

        let (q_cameras, q_windows) = system_state.get(world);

        let Ok(window) = q_windows.get_single() else {
            return;
        };
        let Ok(camera) = q_cameras.get_single() else {
            return;
        };

        let Some(cursor_position) = window.cursor_position() else {
            return;
        };
        let Some(camera_viewport) = camera.logical_viewport_rect() else {
            return;
        };

        camera_viewport.contains(cursor_position)
    };

    let mut did_move_gizmos = false;
    //gizmos controls
    if editor_data.selected_entities.len() == 1 {
        let mut system_state = SystemState::<
            Query<(&GlobalTransform, &OrthographicProjection), With<EditorCamera>>,
        >::new(world);
        let q_cameras = system_state.get(world);
        let (cam_transform, projection) = q_cameras.single();

        let view_matrix = Mat4::from(cam_transform.affine().inverse());
        let projection_matrix = projection.get_projection_matrix();

        for selected in editor_data.selected_entities.iter() {
            let Some(transform) = world.get::<Transform>(selected) else {
                continue;
            };
            let model_matrix = transform.compute_matrix();

            if let Some(result) = Gizmo::new(selected)
                .model_matrix(model_matrix.to_cols_array_2d())
                .view_matrix(view_matrix.to_cols_array_2d())
                .projection_matrix(projection_matrix.to_cols_array_2d())
                .orientation(GizmoOrientation::Local)
                .mode(editor_data.gizmo_mode)
                .interact(ui)
            {
                did_move_gizmos = true;
                let mut transform = world.get_mut::<Transform>(selected).unwrap();
                *transform = Transform {
                    translation: Vec3::from(<[f32; 3]>::from(result.translation)),
                    rotation: Quat::from_array(<[f32; 4]>::from(result.rotation)),
                    scale: Vec3::from(<[f32; 3]>::from(result.scale)),
                };
            };
        }
    }

    //camera controls
    if is_cursor_inside_viewport {
        let mut system_state = SystemState::<(
            Res<Input<MouseButton>>,
            Res<Input<KeyCode>>,
            Res<Time>,
            EventReader<MouseMotion>,
            EventReader<MouseWheel>,
            Query<(&mut Transform, &mut OrthographicProjection), With<EditorCamera>>,
        )>::new(world);
        let (
            mouse_input,
            keyboard_input,
            time,
            mut ev_mouse_motion,
            mut ev_mouse_wheel,
            mut q_cameras,
        ) = system_state.get_mut(world);
        let (mut camera_transform, mut projection) = q_cameras.single_mut();
        let dt = time.delta_seconds();
        if mouse_input.pressed(MouseButton::Middle)
            || (mouse_input.pressed(MouseButton::Left) && keyboard_input.pressed(KeyCode::AltLeft))
        {
            for motion in ev_mouse_motion.iter() {
                camera_transform.translation.x -= motion.delta.x * dt;
                camera_transform.translation.y += motion.delta.y * dt;
            }
        }
        for wheel in ev_mouse_wheel.iter() {
            projection.scale = (projection.scale - wheel.y * dt * 0.2).max(0.001);
        }
    }

    //camera picking
    if is_cursor_inside_viewport && !did_move_gizmos {
        let mut system_state = SystemState::<(
            Res<Input<MouseButton>>,
            //camera
            Query<(&GlobalTransform, &Camera), With<EditorCamera>>,
            //clickable entities
            Query<(Entity, &GlobalTransform, &Aabb)>,
            //window
            Query<&Window, With<PrimaryWindow>>,
        )>::new(world);

        let (mouse_input, q_cameras, q_entities, q_windows) = system_state.get(world);
        let (camera_transform, camera) = q_cameras.single();
        let window = q_windows.single();

        if mouse_input.just_pressed(MouseButton::Left) {
            let cursor_position = window.cursor_position().unwrap();
            let camera_viewport = camera.logical_viewport_rect().unwrap();

            let cursor_world_pos = camera
                .viewport_to_world_2d(camera_transform, cursor_position - camera_viewport.min)
                .unwrap();

            for (entity, transform, aabb) in &q_entities {
                let world_to_local = transform.compute_matrix().inverse();
                let cursor_world_pos = world_to_local
                    .transform_point(cursor_world_pos.extend(0.))
                    .truncate();

                let min = aabb.min().truncate();
                let max = aabb.max().truncate();

                if cursor_world_pos.x >= min.x
                    && cursor_world_pos.x <= max.x
                    && cursor_world_pos.y >= min.y
                    && cursor_world_pos.y <= max.y
                {
                    editor_data.selected_entities.select_replace(entity);
                }
            }
        }
    }
}

fn level_editor_system(world: &mut World) {
    let Ok(egui_context) = world
        .query_filtered::<&mut EguiContext, With<PrimaryWindow>>()
        .get_single(world)
    else {
        return;
    };
    let mut egui_context = egui_context.clone();
    let ctx = egui_context.get_mut();

    world.resource_scope::<EditorData, _>(|world, mut editor_data| {
        world.resource_scope::<EditorTabs, _>(|world, mut editor_tabs| {
            let mut tab_viewer = EditorWindowDrawer {
                world,
                editor_data: &mut editor_data,
            };
            DockArea::new(&mut editor_tabs.tree)
                .style(egui_dock::Style::from_egui(ctx.style().as_ref()))
                .show(ctx, &mut tab_viewer);
            save_scene_system(world, editor_data.as_ref());
        })
    });
}

fn draw_hierarchy(world: &mut World, editor_data: &mut EditorData, ui: &mut egui::Ui) {
    world.resource_scope::<AppTypeRegistry, _>(|world, type_registry| {
        let type_registry = type_registry.read();
        let mut context_menu =
            |ui: &mut egui::Ui, entity: Entity, world: &mut World, _extra_state: &mut ()| {
                if ui.button("Delete - PS: no undo :)").clicked() {
                    world.entity_mut(entity).remove_parent();
                    world.despawn(entity);
                    ui.close_menu();
                }
            };

        Hierarchy {
            world,
            type_registry: &type_registry,
            selected: &mut editor_data.selected_entities,
            context_menu: Some(&mut context_menu),
            shortcircuit_entity: None,
            extra_state: &mut (),
        }
        .show::<()>(ui);
    });
}

fn draw_inspector(world: &mut World, editor_data: &mut EditorData, ui: &mut egui::Ui) {
    match editor_data.selected_entities.as_slice() {
        &[entity] => ui_for_entity_with_children(world, entity, ui),
        entities => ui_for_entities_shared_components(world, entities, ui),
    }
}

fn draw_assets_panel(world: &mut World, editor_data: &mut EditorData, ui: &mut egui::Ui) {
    let mut system_state = SystemState::<(
        Commands,
        ResMut<EguiUserTextures>,
        Res<SpriteHandles>,
        Res<AssetServer>,
        Res<Assets<Image>>,
        Query<Entity, With<SceneInstance>>,
    )>::new(world);
    let (mut commands, mut egui_user_textures, sprite_handles, asset_server, images, q_scene) =
        system_state.get_mut(world);

    let Ok(scene) = q_scene.get_single() else {
        return;
    };

    let thumb_size = egui::vec2(96., 96.);
    let row_count = (ui.available_width() / thumb_size.x).floor() as i32;
    egui::Grid::new("thumbnails").show(ui, |ui| {
        let mut count = 0;
        for handle in &sprite_handles.handles {
            let img_handle = handle.clone().typed::<Image>();
            let texture_id = match egui_user_textures.image_id(&img_handle) {
                Some(texture_id) => texture_id,
                None => egui_user_textures.add_image(img_handle.clone()),
            };

            ui.group(|ui| {
                ui.set_min_size(thumb_size);
                ui.with_layout(Layout::bottom_up(Align::Center), |ui| {
                    ui.set_min_size(thumb_size);
                    // ui.set_max_size(thumb_size);
                    let file_name = String::from(
                        asset_server
                            .get_handle_path(img_handle.clone())
                            .unwrap()
                            .path()
                            .file_name()
                            .unwrap()
                            .to_string_lossy(),
                    );
                    let width = 72.;
                    let height = (width * images.get(&img_handle).unwrap().aspect_2d()).min(72.);

                    ui.label(&file_name);
                    let button_response =
                        ui.add(ImageButton::new(texture_id, [width, height]).frame(true));

                    if button_response.clicked() {
                        let entity = commands
                            .spawn((
                                SpriteBundle {
                                    transform: Transform {
                                        scale: editor_data.sprite_scale(),
                                        ..default()
                                    },
                                    texture: img_handle.clone(),
                                    ..default()
                                },
                                SpriteLayer::Midground,
                                Name::new(file_name),
                            ))
                            .id();

                        commands.entity(scene).push_children(&[entity]);

                        editor_data.selected_entities.select_replace(entity);
                    }
                })
            });

            count += 1;
            if count == row_count - 1 {
                ui.end_row();
                count = 0;
            }
        }
    });
    system_state.apply(world);
}
//TODO: Maybe save scene every X seconds
fn save_scene_system(world: &mut World, editor_data: &EditorData) {
    let mut system_state = SystemState::<(Res<Input<KeyCode>>, EventReader<AppExit>)>::new(world);
    let (input, exit_events) = system_state.get(world);
    let requested_save_scene =
        input.pressed(KeyCode::ControlLeft) && input.just_pressed(KeyCode::S);
    if !exit_events.is_empty() || requested_save_scene {
        save_scene(world, editor_data.level_path.clone());
    }
}

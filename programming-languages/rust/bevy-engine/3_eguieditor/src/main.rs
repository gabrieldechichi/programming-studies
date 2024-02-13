use bevy::math::uvec2;
use bevy::prelude::*;
use bevy_egui::{EguiContext, EguiPlugin};
use bevy_inspector_egui::bevy_inspector::hierarchy::{hierarchy_ui, SelectedEntities};
use bevy_inspector_egui::bevy_inspector::{
    ui_for_entities_shared_components, ui_for_entity_with_children,
};
use bevy_inspector_egui::DefaultInspectorConfigPlugin;
// use bevy_mod_picking::backends::egui::EguiPointer;
// use bevy_mod_picking::prelude::*;
use bevy_egui::EguiSet;
use bevy_math::*;
use bevy_render::camera::{CameraProjection, Viewport};
use bevy_render::primitives::Aabb;
use bevy_window::{close_on_esc, PrimaryWindow};
use egui_dock::{DockArea, NodeIndex, Style, Tree};
use egui_gizmo::{Gizmo, GizmoMode, GizmoOrientation};

fn main() {
    App::new()
        .add_plugins(DefaultPlugins)
        .add_plugins((EguiPlugin, DefaultInspectorConfigPlugin))
        .add_systems(Startup, setup)
        .add_systems(
            Update,
            (close_on_esc, set_gizmo_mode, select_entity_in_world),
        )
        .insert_resource(UiState::new())
        .add_systems(
            PostUpdate,
            (
                show_ui.after(EguiSet::ProcessOutput),
                set_camera_viewport.after(show_ui),
            ),
        )
        .run();
}

#[derive(Component)]
struct MainCamera;

#[derive(Debug)]
enum EguiWindow {
    GameView,
    Hierarchy,
    Inspector,
}

#[derive(Resource)]
struct UiState {
    tree: Tree<EguiWindow>,
    selected_entities: SelectedEntities,
    viewport_rect: egui::Rect,
    gizmos_mode: GizmoMode,
}

impl UiState {
    pub fn new() -> Self {
        let mut tree = Tree::new(vec![EguiWindow::GameView]);
        let [game, _inspector] =
            tree.split_right(NodeIndex::root(), 0.75, vec![EguiWindow::Inspector]);
        let [_game, _hierarchy] = tree.split_left(game, 0.2, vec![EguiWindow::Hierarchy]);
        Self {
            tree,
            selected_entities: SelectedEntities::default(),
            viewport_rect: egui::Rect::NOTHING,
            gizmos_mode: GizmoMode::Translate,
        }
    }

    fn ui(&mut self, world: &mut World, ctx: &mut bevy_egui::egui::Context) {
        let mut tab_viewer = TabViewer {
            world,
            selected_entities: &mut self.selected_entities,
            viewport_rect: &mut self.viewport_rect,
            gizmo_mode: self.gizmos_mode,
        };
        DockArea::new(&mut self.tree)
            .style(Style::from_egui(ctx.style().as_ref()))
            .show(ctx, &mut tab_viewer);
    }
}

struct TabViewer<'a> {
    world: &'a mut World,
    selected_entities: &'a mut SelectedEntities,
    viewport_rect: &'a mut egui::Rect,
    gizmo_mode: GizmoMode,
}

impl egui_dock::TabViewer for TabViewer<'_> {
    type Tab = EguiWindow;

    fn title(&mut self, window: &mut Self::Tab) -> egui_dock::egui::WidgetText {
        format!("{window:?}").into()
    }

    fn ui(&mut self, ui: &mut egui_dock::egui::Ui, tab: &mut Self::Tab) {
        match tab {
            EguiWindow::GameView => {
                *self.viewport_rect = ui.clip_rect();
                draw_gizmos(ui, self.world, &self.selected_entities, self.gizmo_mode)
            }
            EguiWindow::Hierarchy => {
                let _selected = hierarchy_ui(self.world, ui, self.selected_entities);
            }
            EguiWindow::Inspector => match self.selected_entities.as_slice() {
                &[entity] => ui_for_entity_with_children(self.world, entity, ui),
                entities => ui_for_entities_shared_components(self.world, entities, ui),
            },
        }
    }

    fn clear_background(&self, window: &Self::Tab) -> bool {
        !matches!(window, EguiWindow::GameView)
    }
}

fn setup(
    mut commands: Commands,
    mut meshes: ResMut<Assets<Mesh>>,
    mut materials: ResMut<Assets<StandardMaterial>>,
) {
    let box_size = 2.0;
    let box_thickness = 0.15;
    let box_offset = (box_size + box_thickness) / 2.0;
    //environment
    {
        commands.spawn((
            Camera3dBundle {
                transform: Transform::from_xyz(0.0, box_offset, 4.0)
                    .looking_at(Vec3::new(0.0, box_offset, 0.0), Vec3::Y),
                ..Default::default()
            },
            MainCamera,
            // PickRaycastSource,
        ));
        commands.insert_resource(AmbientLight {
            color: Color::WHITE,
            brightness: 1.,
        });
        commands.spawn(DirectionalLightBundle {
            directional_light: DirectionalLight {
                illuminance: 10000.0,
                ..default()
            },
            transform: Transform::from_rotation(Quat::from_rotation_x(-std::f32::consts::PI / 2.0)),
            ..default()
        });

        commands
            .spawn((
                PbrBundle {
                    mesh: meshes.add(Mesh::from(shape::Plane::from_size(0.4))),
                    transform: Transform::from_matrix(Mat4::from_scale_rotation_translation(
                        Vec3::ONE,
                        Quat::from_rotation_x(std::f32::consts::PI),
                        Vec3::new(0.0, box_size + 0.5 * box_thickness, 0.0),
                    )),
                    material: materials.add(StandardMaterial {
                        base_color: Color::WHITE,
                        emissive: Color::WHITE * 100.0,
                        ..Default::default()
                    }),
                    ..Default::default()
                },
                Name::new("Top Light"),
            ))
            .with_children(|builder| {
                builder.spawn(PointLightBundle {
                    point_light: PointLight {
                        color: Color::WHITE,
                        intensity: 25.0,
                        ..Default::default()
                    },
                    transform: Transform::from_translation((box_thickness + 0.05) * Vec3::Y),
                    ..Default::default()
                });
            });
    }
    //objects
    {
        // left - red
        let mut transform = Transform::from_xyz(-box_offset, box_offset, 0.0);
        transform.rotate(Quat::from_rotation_z(std::f32::consts::FRAC_PI_2));
        commands.spawn((
            PbrBundle {
                mesh: meshes.add(Mesh::from(shape::Box::new(
                    box_size,
                    box_thickness,
                    box_size,
                ))),
                transform,
                material: materials.add(StandardMaterial {
                    base_color: Color::rgb(0.63, 0.065, 0.05),
                    ..Default::default()
                }),
                ..Default::default()
            },
            Name::new("Red Box"),
        ));
        // right - green
        let mut transform = Transform::from_xyz(box_offset, box_offset, 0.0);
        transform.rotate(Quat::from_rotation_z(std::f32::consts::FRAC_PI_2));
        commands.spawn((
            PbrBundle {
                mesh: meshes.add(Mesh::from(shape::Box::new(
                    box_size,
                    box_thickness,
                    box_size,
                ))),
                transform,
                material: materials.add(StandardMaterial {
                    base_color: Color::rgb(0.14, 0.45, 0.091),
                    ..Default::default()
                }),
                ..Default::default()
            },
            Name::new("Green Box"),
        ));
        // bottom - white
        commands.spawn((
            PbrBundle {
                mesh: meshes.add(Mesh::from(shape::Box::new(
                    box_size + 2.0 * box_thickness,
                    box_thickness,
                    box_size,
                ))),
                material: materials.add(StandardMaterial {
                    base_color: Color::rgb(0.725, 0.71, 0.68),
                    ..Default::default()
                }),
                ..Default::default()
            },
            Name::new("White Box Bottom"),
        ));
        // top - white
        let transform = Transform::from_xyz(0.0, 2.0 * box_offset, 0.0);
        commands.spawn((
            PbrBundle {
                mesh: meshes.add(Mesh::from(shape::Box::new(
                    box_size + 2.0 * box_thickness,
                    box_thickness,
                    box_size,
                ))),
                transform,
                material: materials.add(StandardMaterial {
                    base_color: Color::rgb(0.725, 0.71, 0.68),
                    ..Default::default()
                }),
                ..Default::default()
            },
            Name::new("White Box Top"),
        ));
        // back - white
        let mut transform = Transform::from_xyz(0.0, box_offset, -box_offset);
        transform.rotate(Quat::from_rotation_x(std::f32::consts::FRAC_PI_2));
        commands.spawn((
            PbrBundle {
                mesh: meshes.add(Mesh::from(shape::Box::new(
                    box_size + 2.0 * box_thickness,
                    box_thickness,
                    box_size + 2.0 * box_thickness,
                ))),
                transform,
                material: materials.add(StandardMaterial {
                    base_color: Color::rgb(0.725, 0.71, 0.68),
                    ..Default::default()
                }),
                ..Default::default()
            },
            Name::new("White Box Back"),
        ));
    }
}

fn show_ui(world: &mut World) {
    let Ok(egui_context) = world
        .query_filtered::<&mut EguiContext, With<PrimaryWindow>>()
        .get_single(world)
    else {
        return;
    };
    let mut egui_context = egui_context.clone();

    world.resource_scope::<UiState, _>(|world, mut ui_state| {
        ui_state.ui(world, egui_context.get_mut())
    });
}

fn set_camera_viewport(
    ui_state: Res<UiState>,
    primary_window: Query<&mut Window, With<PrimaryWindow>>,
    egui_settings: Res<bevy_egui::EguiSettings>,
    mut cameras: Query<&mut Camera, With<MainCamera>>,
) {
    let Ok(mut cam) = cameras.get_single_mut() else{return;};
    let Ok(window) = primary_window.get_single() else{return;};

    let scale_factor = (window.scale_factor() * egui_settings.scale_factor) as f32;
    let viewport_pos = ui_state.viewport_rect.left_top().to_vec2() * scale_factor;
    let viewport_size = ui_state.viewport_rect.size() * scale_factor;

    cam.viewport = Some(Viewport {
        physical_position: uvec2(viewport_pos.x as u32, viewport_pos.y as u32),
        physical_size: uvec2(viewport_size.x as u32, viewport_size.y as u32),
        depth: 0.0..1.0,
    })
}

fn draw_gizmos(
    ui: &mut egui::Ui,
    world: &mut World,
    selected_entities: &SelectedEntities,
    gizmo_mode: GizmoMode,
) {
    if selected_entities.len() != 1 {
        return;
    }

    let Ok((cam_transform, projection)) = world
        .query_filtered::<(&GlobalTransform, &Projection), With<MainCamera>>()
        .get_single(world) else {return;};

    let view_matrix = Mat4::from(cam_transform.affine().inverse());
    let projection_matrix = projection.get_projection_matrix();

    for selected in selected_entities.iter() {
        let Some(mut transform) = world.get_mut::<Transform>(selected) else{continue;};
        let model_matrix = transform.compute_matrix();

        let Some(result) = Gizmo::new(selected)
            .model_matrix(model_matrix.to_cols_array_2d())
            .view_matrix(view_matrix.to_cols_array_2d())
            .projection_matrix(projection_matrix.to_cols_array_2d())
            .orientation(GizmoOrientation::Local)
            .mode(gizmo_mode)
            .interact(ui)
        else{
            continue;
        };

        *transform = Transform {
            translation: Vec3::from(<[f32; 3]>::from(result.translation)),
            rotation: Quat::from_array(<[f32; 4]>::from(result.rotation)),
            scale: Vec3::from(<[f32; 3]>::from(result.scale)),
        };
    }
}

fn set_gizmo_mode(input: Res<Input<KeyCode>>, mut ui_state: ResMut<UiState>) {
    for (key, mode) in [
        (KeyCode::Q, GizmoMode::Translate),
        (KeyCode::W, GizmoMode::Rotate),
        (KeyCode::R, GizmoMode::Scale),
    ] {
        if input.just_pressed(key) {
            ui_state.gizmos_mode = mode;
        }
    }
}

fn select_entity_in_world(
    mouse: Res<Input<MouseButton>>,
    mut ui_state: ResMut<UiState>,
    windows: Query<&Window, With<PrimaryWindow>>,
    cameras: Query<(&Camera, &GlobalTransform), With<MainCamera>>,
    aabbs: Query<(Entity, &Aabb, &Transform, &Name)>,
) {
    if mouse.just_pressed(MouseButton::Left) {
        let Ok(window) = windows.get_single() else{return;};
        let Ok((camera, camera_transform)) = cameras.get_single() else{return;};

        let Some(viewport_rect ) = camera.logical_viewport_rect() else{return;};

        let Some(ray) = window.cursor_position()
            .and_then(|cursor_pos| camera.viewport_to_world(camera_transform, cursor_pos - viewport_rect.min))
            else { return; };

        for (entity, aabb, transform, name) in &aabbs {
            let Some(hit) = ray_intersects_aabb(&ray, aabb, transform) else{continue;};
            println!("{}: {}, {}, {}", name, hit.x, hit.y, hit.z);
            ui_state.selected_entities.select_replace(entity);
            break;
        }
    }
}

fn ray_intersects_aabb(ray: &Ray, aabb: &Aabb, transform: &Transform) -> Option<Vec3> {
    // Invert the transform to transform the ray to the AABB's local space.
    let inv_transform = transform.compute_matrix().inverse();

    // Transform the ray's origin and direction.
    let local_origin = inv_transform.transform_point3(ray.origin);
    let local_direction = inv_transform.transform_vector3(ray.direction);

    let inv_dir = 1.0 / local_direction;

    // Since we're now in the local space of the AABB, we can work directly with its center and half_extents.
    let t1 = (aabb.center.x - aabb.half_extents.x - local_origin.x) * inv_dir.x;
    let t2 = (aabb.center.x + aabb.half_extents.x - local_origin.x) * inv_dir.x;
    let t3 = (aabb.center.y - aabb.half_extents.y - local_origin.y) * inv_dir.y;
    let t4 = (aabb.center.y + aabb.half_extents.y - local_origin.y) * inv_dir.y;
    let t5 = (aabb.center.z - aabb.half_extents.z - local_origin.z) * inv_dir.z;
    let t6 = (aabb.center.z + aabb.half_extents.z - local_origin.z) * inv_dir.z;

    let tmin = t1.min(t2).max(t3.min(t4)).max(t5.min(t6));
    let tmax = t1.max(t2).min(t3.max(t4)).min(t5.max(t6));

    if tmax < 0.0 || tmin > tmax {
        return None;
    }

    // Calculate the intersection point in the local space
    let local_intersection = local_origin + local_direction * tmin;

    // Transform the intersection point back to world space
    let world_intersection = transform.transform_point(local_intersection);

    Some(world_intersection)
}

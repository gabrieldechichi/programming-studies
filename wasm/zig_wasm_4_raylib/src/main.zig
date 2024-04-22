const std = @import("std");
const rl = @cImport(@cInclude("raylib.h"));
const rlmath = @cImport(@cInclude("raymath.h"));
const Vector2 = rlmath.Vector2;

const ScreenSize = struct {
    width: u16,
    height: u16,

    pub fn width_f(self: *ScreenSize) f32 {
        return @floatFromInt(self.width);
    }
    pub fn height_f(self: *ScreenSize) f32 {
        return @floatFromInt(self.height);
    }
};

const BallColor = enum { one, two, three };
// const Ball = struct { pos: Vector2, velocity: Vector2, radius: f32, color: rl.Color };
const Ball = struct { x: f32, y: f32, vx: f32, vy: f32, color: BallColor };

const ScreenPos = struct { x: i32, y: i32 };

const FrameStats = struct {
    const entry_count = 100;
    dts: [entry_count]f32,
    avg_dt: f32,
    last_entry: usize,

    pub fn record(self: *FrameStats) void {
        self.dts[self.last_entry] = rl.GetFrameTime();
        self.last_entry = (self.last_entry + 1) % entry_count;
        var s: f32 = 0.0;
        for (self.dts) |dt| {
            s += dt;
        }
        self.avg_dt = s / entry_count;
    }

    pub fn frameTimeMs(self: *const FrameStats) f32 {
        return self.avg_dt * 1000;
    }
};

fn genSquareMesh() rl.Mesh {
    const vertices = &[_]f32{
        -0.5, 0.5, 0.0, // Top-left
        0.5, 0.5, 0.0, // Top-right
        0.5, -0.5, 0.0, // Bottom-right
        -0.5, -0.5, 0.0, // Bottom-left
    };

    const texcoords = &[_]f32{
        0.0, 0.0, // Top-left
        1.0, 0.0, // Top-right
        1.0, 1.0, // Bottom-right
        0.0, 1.0, // Bottom-left
    };

    const indices = &[_]c_ushort{
        0, 1, 2, // First triangle
        2, 3, 0, // Second triangle
    };

    return rl.Mesh{
        .vertexCount = 4,
        .triangleCount = 2,
        .vertices = @as([*c]f32, @constCast(vertices.ptr)),
        .texcoords = @as([*c]f32, @constCast(texcoords.ptr)),
        .texcoords2 = null,
        .normals = null,
        .tangents = null,
        .colors = null,
        .indices = @as([*c]c_ushort, @constCast(indices.ptr)),
        .animVertices = null,
        .animNormals = null,
        .boneIds = null,
        .boneWeights = null,
        .vaoId = undefined, // Initialized later by raylib functions
        .vboId = undefined, // Initialized later by raylib functions
    };
}

fn matrixTranslate2D(pos: Vector2) rl.Matrix {
    return @bitCast(rlmath.MatrixTranslate(pos.x, pos.y, 0));
}
fn matrix2DTS(pos: Vector2, scale: Vector2) rl.Matrix {
    const m_translate = rlmath.MatrixTranslate(pos.x, pos.y, 0);
    const m_scale = rlmath.MatrixScale(scale.x, scale.y, 1);
    return @bitCast(rlmath.MatrixMultiply(m_scale, m_translate));
}

pub fn main() !void {
    const allocator = std.heap.c_allocator;
    rl.InitWindow(960, 540, "My Window Name");
    rl.SetTargetFPS(10000);
    defer rl.CloseWindow();

    var frame_stats = FrameStats{
        .dts = undefined,
        .avg_dt = 0,
        .last_entry = 0,
    };
    var perf_stats_buffer: [25]u8 = undefined;

    var screen_size = ScreenSize{ .width = @intCast(rl.GetScreenWidth()), .height = @intCast(rl.GetScreenHeight()) };

    const shader = rl.LoadShader(
        "./resources/shaders/vertex_default_instancing.vs",
        "./resources/shaders/frag_default_instancing.fs",
    );
    // shader.locs[rl.SHADER_LOC_MATRIX_MVP] = rl.GetShaderLocation(shader, "mvp");
    // shader.locs[rl.SHADER_LOC_VECTOR_VIEW] = rl.GetShaderLocation(shader, "viewPos");
    shader.locs[rl.SHADER_LOC_MATRIX_MODEL] = rl.GetShaderLocationAttrib(shader, "instanceTransform");

    // Set shader value: ambient light level
    // const ambientLoc = rl.GetShaderLocation(shader, "ambient");
    // rl.SetShaderValue(shader, ambientLoc, &.{ 0.2, 0.2, 0.2, 1.0 }, rl.SHADER_UNIFORM_VEC4);

    // Create one light
    // rl.CreateLight(rl.LIGHT_DIRECTIONAL, &.{ 50.0, 50.0, 0.0 }, @bitCast(rlmath.Vector3Zero()), rl.WHITE, shader);

    var square_mesh = genSquareMesh();
    rl.UploadMesh(&square_mesh, false);
    defer rl.UnloadMesh(square_mesh);
    var square_material = rl.LoadMaterialDefault();
    defer rl.UnloadMaterial(square_material);
    square_material.maps[rl.MATERIAL_MAP_DIFFUSE].color = rl.RED;
    square_material.shader = shader;

    var rnd = std.rand.DefaultPrng.init(0);

    const ball_radius = 20.0;
    const batch_size = 64 + 32;
    const total_balls = batch_size * 1024;
    // const total_balls = 10;
    var balls = try std.ArrayList(Ball).initCapacity(allocator, total_balls);
    var ball_transforms = try std.ArrayList(rl.Matrix).initCapacity(allocator, total_balls);
    for (0..total_balls) |_| {
        const color = switch (rnd.random().uintLessThan(u8, 3)) {
            0 => BallColor.one,
            1 => BallColor.two,
            else => BallColor.three,
        };
        const vx: f32 = @floatFromInt(std.math.sign(rnd.random().int(u16)) * (rnd.random().uintAtMost(u16, 100) + 200));
        const vy: f32 = @floatFromInt(std.math.sign(rnd.random().int(u16)) * (rnd.random().uintAtMost(u16, 100) + 200));
        var ball = Ball{ .color = color, .x = 0, .y = 0, .vx = vx, .vy = vy };
        try balls.append(ball);
        try ball_transforms.append(matrix2DTS(
            .{ .x = ball.x, .y = ball.y },
            .{ .x = ball_radius * 2, .y = ball_radius * 2 },
        ));
    }

    while (!rl.WindowShouldClose()) {
        //update
        {
            const dt = rl.GetFrameTime();
            for (balls.items) |*ball| {
                if (ball.x - ball_radius < -screen_size.width_f() / 2 or ball.x + ball_radius > screen_size.width_f() / 2) {
                    ball.vx *= -1;
                }
                if (ball.y - ball_radius < -screen_size.height_f() / 2 or ball.y + ball_radius > screen_size.height_f() / 2) {
                    ball.vy *= -1;
                }
                ball.x += ball.vx * dt;
                ball.y += ball.vy * dt;
            }
        }

        //render
        {
            for (balls.items, ball_transforms.items) |ball, *ball_transform| {
                ball_transform.* = matrix2DTS(
                    .{
                        .x = ball.x + screen_size.width_f() / 2,
                        .y = ball.y + screen_size.height_f() / 2,
                    },
                    .{ .x = ball_radius * 2, .y = ball_radius * 2 },
                );
            }

            rl.BeginDrawing();

            {
                rl.ClearBackground(rl.BLACK);

                // for (balls.items, ball_transforms.items) |_, ball_transform| {
                //     rl.DrawMesh(square_mesh, square_material, ball_transform);
                // }
                rl.DrawMeshInstanced(
                    square_mesh,
                    square_material,
                    @as([*c]const rl.Matrix, ball_transforms.items.ptr),
                    @intCast(ball_transforms.items.len),
                );

                frame_stats.record();
                const stats = try std.fmt.bufPrint(
                    &perf_stats_buffer,
                    "Dt: {d:.2}ms",
                    .{frame_stats.frameTimeMs()},
                );
                rl.DrawText(stats.ptr, 12, 12, 20, rl.ORANGE);

                rl.EndTextureMode();
            }

            rl.EndDrawing();
        }
    }
}

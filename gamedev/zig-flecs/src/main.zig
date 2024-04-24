const std = @import("std");
const ecs = @import("zflecs");

const Position = struct { x: f32, y: f32 };
const Velocity = struct { x: f32, y: f32 };
const Eats = struct {};
const Apples = struct {};

fn move_system(positions: []Position, velocities: []Velocity) void {
    for (positions, velocities) |*p, v| {
        p.x += v.x;
        p.y += v.y;
    }
}

fn move(it: *ecs.iter_t) callconv(.C) void {
    const p = ecs.field(it, Position, 1).?;
    const v = ecs.field(it, Velocity, 2).?;

    const type_str = ecs.table_str(it.world, it.table).?;
    std.debug.print("Move entities with [{s}]\n", .{type_str});
    defer ecs.os.free(type_str);
    move_system(p, v);
}

pub fn ADD_SYSTEM(world: *ecs.world_t, comptime fn_system: anytype, name: [*:0]const u8, phase: ecs.entity_t) void {
    var desc = SYSTEM_DESC(fn_system);
    ecs.SYSTEM(world, name, phase, &desc);
}

pub fn SYSTEM_DESC(comptime fn_system: anytype) ecs.system_desc_t {
    const system_struct = struct {
        fn exec(it: *ecs.iter_t) callconv(.C) void {
            const fn_type = @typeInfo(@TypeOf(fn_system));
            switch (fn_type.Fn.params.len) {
                1 => {
                    const c1 = ecs.field(it, @typeInfo(fn_type.Fn.params[0].type.?).Pointer.child, 1).?;
                    fn_system(c1);
                },
                2 => {
                    const c1 = ecs.field(it, @typeInfo(fn_type.Fn.params[0].type.?).Pointer.child, 1).?;
                    const c2 = ecs.field(it, @typeInfo(fn_type.Fn.params[1].type.?).Pointer.child, 2).?;
                    fn_system(c1, c2);
                },
                3 => {
                    const c1 = ecs.field(it, @typeInfo(fn_type.Fn.params[0].type.?).Pointer.child, 1).?;
                    const c2 = ecs.field(it, @typeInfo(fn_type.Fn.params[1].type.?).Pointer.child, 2).?;
                    const c3 = ecs.field(it, @typeInfo(fn_type.Fn.params[2].type.?).Pointer.child, 3).?;
                    fn_system(c1, c2, c3);
                },
                else => unreachable,
            }
        }
    };

    var system_desc = ecs.system_desc_t{};
    system_desc.callback = system_struct.exec;

    inline for (@typeInfo(@TypeOf(fn_system)).Fn.params, 0..) |p, i| {
        const t = @typeInfo(p.type.?).Pointer.child;
        system_desc.query.filter.terms[i] = .{ .id = ecs.id(t) };
    }

    return system_desc;
}

// pub fn SYSTEM(world: *ecs.world_t, name: [*:0]const u8, phase: ecs.entity_t, comptime fn_system: anytype) void {
//     comptime {
//         const typeInfo = @typeInfo(@TypeOf(fn_system));
//
//         @compileLog(typeInfo.Fn.params);
//         // inline for (typeInfo.Fn.params) |p| {
//         //     @compileLog(p);
//         // }
//     }
//
//     //TODO: fix
//     var system_desc = ecs.system_desc_t{};
//     system_desc.callback = fn_system;
//     system_desc.query.filter.terms[0] = .{ .id = ecs.id(Position) };
//     system_desc.query.filter.terms[1] = .{ .id = ecs.id(Velocity) };
//     ecs.SYSTEM(world, name, phase, &system_desc);
// }

pub fn main() !void {
    const world = ecs.init();
    defer _ = ecs.fini(world);

    ecs.COMPONENT(world, Position);
    ecs.COMPONENT(world, Velocity);

    ecs.TAG(world, Eats);
    ecs.TAG(world, Apples);

    {
        ADD_SYSTEM(world, move_system, "move_system", ecs.OnUpdate);
    }
    {

        // var system_desc = ecs.system_desc_t{};
        // system_desc.callback = move;
        // system_desc.query.filter.terms[0] = .{ .id = ecs.id(Position) };
        // system_desc.query.filter.terms[1] = .{ .id = ecs.id(Velocity) };
        // ecs.SYSTEM(world, "move system", ecs.OnUpdate, &system_desc);
    }

    const bob = ecs.new_entity(world, "Bob");
    _ = ecs.set(world, bob, Position, .{ .x = 2, .y = 0 });
    _ = ecs.set(world, bob, Velocity, .{ .x = 1, .y = 2 });
    ecs.add_pair(world, bob, ecs.id(Eats), ecs.id(Apples));

    _ = ecs.progress(world, 0);
    _ = ecs.progress(world, 0);

    const p = ecs.get(world, bob, Position).?;
    std.debug.print("Bob's position is ({d}, {d})\n", .{ p.x, p.y });
}

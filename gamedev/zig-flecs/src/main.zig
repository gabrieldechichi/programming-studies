const std = @import("std");
const ecs = @import("zflecs");

const Position = struct { x: f32, y: f32 };
const Velocity = struct { x: f32, y: f32 };
const Eats = struct {};
const Apples = struct {};

fn move_system(positions: []Position, velocities: []const Velocity) void {
    for (positions, velocities) |*p, *v| {
        p.x += v.x;
        p.y += v.y;
    }
}

pub fn ADD_SYSTEM(world: *ecs.world_t, comptime fn_system: anytype, name: [*:0]const u8, phase: ecs.entity_t) void {
    var desc = SYSTEM_DESC(fn_system);
    ecs.SYSTEM(world, name, phase, &desc);
}

fn SystemImpl(comptime fn_system: anytype) type {
    return struct {
        fn exec(it: *ecs.iter_t) callconv(.C) void {
            const fn_type = @typeInfo(@TypeOf(fn_system));
            const ArgsTupleType = std.meta.ArgsTuple(@TypeOf(fn_system));
            var args_tuple: ArgsTupleType = undefined;
            inline for (fn_type.Fn.params, 0..) |p, i| {
                args_tuple[i] = ecs.field(it, @typeInfo(p.type.?).Pointer.child, i + 1).?;
            }

            //NOTE: .always_inline seems ok, but unsure. Replace to .auto if it breaks
            _ = @call(.always_inline, fn_system, args_tuple);
        }
    };
}

pub fn SYSTEM_DESC(comptime fn_system: anytype) ecs.system_desc_t {
    const system_struct = SystemImpl(fn_system);

    var system_desc = ecs.system_desc_t{};
    system_desc.callback = system_struct.exec;

    inline for (@typeInfo(@TypeOf(fn_system)).Fn.params, 0..) |p, i| {
        const param_type_info = @typeInfo(p.type.?).Pointer;
        const inout = if (param_type_info.is_const) .In else .InOut;
        system_desc.query.filter.terms[i] = .{
            .id = ecs.id(param_type_info.child),
            .inout = inout
        };
    }

    return system_desc;
}

pub fn main() !void {
    const world = ecs.init();
    defer _ = ecs.fini(world);

    ecs.COMPONENT(world, Position);
    ecs.COMPONENT(world, Velocity);

    ecs.TAG(world, Eats);
    ecs.TAG(world, Apples);

    ADD_SYSTEM(world, move_system, "move_system", ecs.OnUpdate);

    const bob = ecs.new_entity(world, "Bob");
    _ = ecs.set(world, bob, Position, .{ .x = 2, .y = 0 });
    _ = ecs.set(world, bob, Velocity, .{ .x = 1, .y = 2 });
    ecs.add_pair(world, bob, ecs.id(Eats), ecs.id(Apples));

    _ = ecs.progress(world, 0);
    _ = ecs.progress(world, 0);

    const p = ecs.get(world, bob, Position).?;
    std.debug.print("Bob's position is ({d}, {d})\n", .{ p.x, p.y });
}

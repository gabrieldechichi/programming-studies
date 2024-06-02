# Idea for massively parallel ECS with compile time dependency calculation

Assume archetypes defined as SOA. Ex:

```odin
Player :: struct {
	using graphics: Graphics,
	using entity:   Entity,
	velocity:       rl.Vector2,
	movement_speed: f32,
	jump_speed:     f32,
	grounded:       bool,
	animations:     PlayerAnimations,
}
```

You can have systems as follows:

```odin
test_system :: proc(len: int, velocities: [^]rl.Vector2, speeds: [^]f32) {
	for i := 0; i < len; i += 1 {
		v := velocities[i]
		s := speeds[i]
		fmt.println(v, s, i)
	}
}

world_update :: proc(world: ^World) {
    //world.players is #soa [dynamic]Player
	test_system(
		len(world.players),
		world.players.velocity,
		world.players.movement_speed,
	)
}
```

Assume another entity with velocity and movement speed

```odin
Enemy :: struct {
	using graphics: Graphics,
	using entity:   Entity,
	velocity:       rl.Vector2,
	movement_speed: f32,
}

world_update :: proc(world: ^World) {
    //world.players is #soa [dynamic]Player
	test_system(
		len(world.players),
		world.players.velocity,
		world.players.movement_speed,
	)

	test_system(
		len(world.enemies),
		world.enemies.velocity,
		world.enemies.movement_speed,
	)
}
```

This systems can run in parallel. There are no dependencies between them. But Unity ECS can't do that. Neither can Bevy I think?

But at compile time one can analyse this calls and build a dependency graph,
than distribute the function calls within threads?

Is there any advantage of doing that in compile time vs runtime? Maybe runtime
is better, as long as it is cached.

Runtime code can't introspect calls though?

## TODOS:
- [ ] Test Bevy and see if they are able to paralelize systems with the same components but different archetypes
- [ ] In order to know more you should understand how job systems work.

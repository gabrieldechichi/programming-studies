package fixed_string

import "base:runtime"
import "core:fmt"
import "core:mem"
import "core:strings"

FixedString16 :: FixedString(16)
FixedString32 :: FixedString(32)
FixedString64 :: FixedString(64)
FixedString128 :: FixedString(128)

new_from_string_16 :: proc(s: string) -> FixedString(16) {
	return _new_from_string(16, s)
}

to_string_16 :: proc(
	s: FixedString(16),
	allocator: mem.Allocator = context.allocator,
) -> string {
	return _to_string(s, allocator)
}

new_from_string_32 :: proc(s: string) -> FixedString(32) {
	return _new_from_string(32, s)
}

to_string_32 :: proc(
	s: FixedString(32),
	allocator: mem.Allocator = context.allocator,
) -> string {
	return _to_string(s, allocator)
}

new_from_string_64 :: proc(s: string) -> FixedString(64) {
	return _new_from_string(64, s)
}

to_string_64 :: proc(
	s: FixedString(64),
	allocator: mem.Allocator = context.allocator,
) -> string {
	return _to_string(s, allocator)
}

new_from_string_128 :: proc(s: string) -> FixedString(128) {
	return _new_from_string(128, s)
}

to_string_128 :: proc(
	s: FixedString(128),
	allocator: mem.Allocator = context.allocator,
) -> string {
	return _to_string(s, allocator)
}

@(private)
FixedString :: struct($N: int) {
	chars: [N]byte,
	len:   int,
}

@(private)
_new_from_string :: proc($N: int, s: string) -> FixedString(N) {
	assert(len(s) <= N)
	fixed := FixedString(N) {
		len = len(s),
	}
	mem.copy(raw_data(fixed.chars[:]), raw_data(s), len(s))
	return fixed
}

@(private)
_to_string :: proc(
	s: FixedString($N),
	allocator: mem.Allocator = context.allocator,
) -> string {
	s := s
	return strings.clone_from_bytes(s.chars[:s.len], allocator)
}

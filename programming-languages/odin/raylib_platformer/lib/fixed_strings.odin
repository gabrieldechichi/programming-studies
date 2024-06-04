package lib

import "base:runtime"
import "core:fmt"
import "core:mem"
import "core:strings"

FixedString16 :: FixedString(16)
FixedString32 :: FixedString(32)
FixedString64 :: FixedString(64)
FixedString128 :: FixedString(128)

fixedstring16_from_string_copy :: proc(s: string) -> FixedString(16) {
	return _fixedstring_from_string_copy(16, s)
}

fixedstring16_to_string :: proc(
	s: FixedString(16),
	allocator: mem.Allocator = context.allocator,
) -> string {
	return _fixedstring_to_string(s, allocator)
}

fixedstring32_from_string_copy :: proc(s: string) -> FixedString(32) {
	return _fixedstring_from_string_copy(32, s)
}

fixedstring32_to_string :: proc(
	s: FixedString(32),
	allocator: mem.Allocator = context.allocator,
) -> string {
	return _fixedstring_to_string(s, allocator)
}

fixedstring64_from_string_copy :: proc(s: string) -> FixedString(64) {
	return _fixedstring_from_string_copy(64, s)
}

fixedstring64_to_string :: proc(
	s: FixedString(64),
	allocator: mem.Allocator = context.allocator,
) -> string {
	return _fixedstring_to_string(s, allocator)
}

fixedstring128_from_string_copy :: proc(s: string) -> FixedString(128) {
	return _fixedstring_from_string_copy(128, s)
}

fixedstring128_to_string :: proc(
	s: FixedString(128),
	allocator: mem.Allocator = context.allocator,
) -> string {
	return _fixedstring_to_string(s, allocator)
}

@(private)
FixedString :: struct($N: int) {
	chars: [N]byte,
	len:   int,
}

@(private)
_fixedstring_from_string_copy :: proc($N: int, s: string) -> FixedString(N) {
	assert(len(s) <= N)
	fixed := FixedString(N) {
		len = len(s),
	}
	mem.copy(raw_data(fixed.chars[:]), raw_data(s), len(s))
	return fixed
}

@(private)
_fixedstring_to_string :: proc(
	s: FixedString($N),
	allocator: mem.Allocator = context.allocator,
) -> string {
	s := s
	return strings.clone_from_bytes(s.chars[:s.len], allocator)
}

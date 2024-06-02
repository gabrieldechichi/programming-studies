package lib

import "base:runtime"
import "core:fmt"
import "core:mem"
import "core:strings"

FixedString :: struct {
	chars: [128]byte,
	len:   int,
}

fixedstring_from_string_copy :: proc(s: string) -> FixedString {
	assert(len(s) <= 128)
	fixed := FixedString {
		len = len(s),
	}
	mem.copy(raw_data(fixed.chars[:]), raw_data(s), len(s))
	fixed_string_to_string(fixed)
	return fixed
}

fixed_string_to_string :: proc(
	s: FixedString,
	allocator: mem.Allocator = context.allocator,
) -> string {
	s := s
	return strings.clone_from_bytes(s.chars[:s.len], allocator)
}

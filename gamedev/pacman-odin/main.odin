package main

import "core:fmt"
import wasmjs "vendor:wasm/js"

main :: proc() {
	run()
}

@(export = true)
run :: proc() {
	fmt.printfln("Heyo")
}

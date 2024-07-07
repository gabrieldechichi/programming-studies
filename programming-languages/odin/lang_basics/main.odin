package main

import "core:fmt"
import "core:reflect"
import "core:math"

main :: proc() {
	x := 2

	fmt.println("Hello", len("Hello"))

	//loops
	{
		for i := 0; i < 10; i += 1 {
			// fmt.println("Hi", i)
		}

		for i in 0 ..< 10 {
			fmt.println("Hi", i)
		}

		i := 0
		for i < 10 {
			// fmt.println("Hi", i)
			i += 1
		}

		//infinite loop
		for {
			break
		}

		//reverse
		array := [?]int{1, 2, 3}
		#reverse for e, index in array {
			fmt.printfln("(%d) %d", index, e)
		}
	}

	//if statements
	{
		x := 2
		if x > 2 {
			fmt.println("That's werid")
		} else {
			fmt.println("That's expected")
		}

		//scope variable if
		if y := 2; y < 3 {
			fmt.println(y)
		}
	}
}

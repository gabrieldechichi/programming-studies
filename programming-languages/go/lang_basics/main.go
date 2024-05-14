package main

import (
	"errors"
	"fmt"
	"math"
	"unicode/utf8"
)

func main() {
	//hello world
	{
		fmt.Println("Hello Go!")
	}
	//types
	{
		//int8..64, float32..64, bool, uint, etc etc
		var intNum int
		intNum = -1
		otherInt := 2
		_ = otherInt
		floatNum := float32(123.4)

		//casting
		result := float32(intNum) + floatNum
		_ = result

		myString := "Hello" + "Go"
		myString += "More string"

		//note: Length in bytes, only works for UTF8
		fmt.Println(len(myString))
		//if looking for actual length
		utf8.RuneCount([]byte(myString))

		myFlag := false
		_ = myFlag

		//consts
		const pi float64 = math.Pi
		_ = pi
	}

	//functions
	{
		printStuff()
		div, rmd := intDiv(3, 2)
		_ = div
		_ = rmd
		_, _, err := intDivSafe(3, 0)
		if err != nil {
			fmt.Println(err)
		}
	}

	//if and switch
	{
		a, b := 3, 0
		_, rmd, err := intDivSafe(a, b)
		if err != nil {
			fmt.Println(err)
		} else if rmd == 0 {
			fmt.Printf("%d is divisible by %d\n", a, b)
		} else {
			fmt.Printf("%d is NOT divisible by %d\n", a, b)
		}

		//this switch is equivalent
		switch {
		case err != nil:
			fmt.Println(err)
		case rmd == 0:
			fmt.Printf("%d is divisible by %d\n", a, b)
		default:
			fmt.Printf("%d is NOT divisible by %d\n", a, b)
		}
	}

	//arrays
	{
		// var arr [3]int;
		arr := [...]int{1, 23, 0}
		arr[0] = 2
		arr[1] = 3
		//compile error
		// arr[5] = 0;
		fmt.Println(arr[1], arr[0:2], arr[1:], arr[:2], &arr[1])
	}

	//slices (just like lists)
	{
		s := []int{1, 23}
		s = append(s, 2)
		s = append(s, 2)
		s = append(s, 2)
		fmt.Println(s[len(s)-1], len(s), cap(s))
	}

	//maps
	{
		myMap := make(map[string]uint8)
		myMap["one"] = 1
		myMap["two"] = 2

		var myMap2 map[string]uint8 = map[string]uint8{"one": 1, "two": 2}
		_ = myMap2
		var n, ok = myMap["one"]
		if ok {
			fmt.Println(n)
		}
		// myMap := make(map[string]uint8)
	}

	//structs
	{
		myEngine := gasEngine{mpg: 2, gallons: 2, ownerInfo: owner{"Owner"}}
		fmt.Println(myEngine)

		//structs accept composition (allows for some polymorphism)
		type gasEngine2 struct {
			mpg     uint8
			gallons uint8
			owner
		}
		myEngine2 := gasEngine2{mpg: 2, gallons: 2, owner: owner{name: "Owner"}}
		fmt.Println(myEngine2)
		//works
		myEngine2.sayHi()
		//works
		myEngine.ownerInfo.sayHi()
		//doesn't work
		// myEngine.sayHi();

	}

	//interfaces
	{
		myEngine := engine(gasEngine{mpg: 2, gallons: 2, ownerInfo: owner{"Owner"}})
		fmt.Println(myEngine.milesLeft())
	}

	//generics
	{
		var arr []int = []int{1, 2, 3}
		n := sum(arr)
		fmt.Println(n == 6)
	}
}

// START FUNCTIONS
func printStuff() {
	fmt.Println("stuff")
}

func intDiv(a int, b int) (int, int) {
	return a / b, a % b
}

func intDivSafe(a int, b int) (int, int, error) {
	if b == 0 {
		return 0, 0, errors.New("Cannot divide by zero")
	}
	div, rmd := intDiv(a, b)
	return div, rmd, nil
}

//END functions

//START STRUCTS

type owner struct {
	name string
}
type gasEngine struct {
	mpg       uint8
	gallons   uint8
	ownerInfo owner
}

func (o owner) sayHi() {
	fmt.Printf("Owner %s says hi", o.name)
}

func (g gasEngine) milesLeft() uint8 {
	fmt.Println(g.gallons)
	return g.gallons
}

//END STRUATS

// START INTERFACES
type engine interface {
	milesLeft() uint8
}

//END INTERFACES

// START GENERIC
func sum[T int | float32](arr []T) T {
	var s T
	for _, n := range arr {
		s += n
	}
	return s
}

type hydrogenEngine struct {
	mpg      uint8
	explodes bool
}

type car[T engine] struct {
	engine T
}

func (g hydrogenEngine) milesLeft() uint8 {
	return 0
}

func (c car[T]) miles() {
    c.engine.milesLeft()
}

// END GENERICS

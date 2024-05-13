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
}

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

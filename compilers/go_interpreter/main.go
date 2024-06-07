package main

import (
	"fmt"
	"go_interpreter/lexer"
	"go_interpreter/parser"
	"go_interpreter/repl"
	"os"
)

func main() {
    // uglyTest()
    runRepl()
}

func runRepl() {
	fmt.Println("Welcome to Monkey REPL")
	err := repl.Start(os.Stdin, os.Stdout)
	if err != nil {
		panic(err)
	}
	fmt.Println("Exitted Monkey REPL")
}

func uglyTest() {
	input := `let five = 5;
let ten = 10;
let add = fn(x, y) {
x + y;
};
let result = add(five, ten);
5 < 10 > 5;
if (5 < 10) {
return true;
} else {
return false;
}
10 == 10;
10 != 9;
    `
	p := parser.New(lexer.New(input))
	program := p.ParseProgram()
	if len(p.Errors()) > 0 {
		fmt.Println("Syntax error!")
		for _, e := range p.Errors() {
			fmt.Printf("\t >%s\n", e)
		}
	} else {
		fmt.Println("Parse success!")
		fmt.Printf(program.String())
	}
}

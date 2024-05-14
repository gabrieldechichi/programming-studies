package main

import (
	"fmt"
	"go_interpreter/lexer"
	"go_interpreter/repl"
	"go_interpreter/token"
	"os"
)

func main() {
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
!-/*5;
5 < 10 > 5;
if (5 < 10) {
return true;
} else {
return false;
}
10 == 10;
10 != 9;
    `
	l := lexer.New(input)
	fmt.Println(input)
	for {
		tok := l.NextToken()
		if tok.Type == token.EOF {
			break
		}
		fmt.Println(tok.Type, tok.Literal)
	}
	fmt.Println("done")

}

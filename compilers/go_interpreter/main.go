package main

import (
	"fmt"
	"go_interpreter/lexer"
	"go_interpreter/token"
)

func main() {
	input := `let five = func(x,y);
let five = 4
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

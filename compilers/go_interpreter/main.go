package main

import (
	"fmt"
	"go_interpreter/lexer"
	"go_interpreter/token"
)

func main() {
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

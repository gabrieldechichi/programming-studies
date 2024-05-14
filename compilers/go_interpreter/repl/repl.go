package repl

import (
	"bufio"
	"fmt"
	"go_interpreter/lexer"
	"go_interpreter/token"
	"io"
)

const PROMPT = ">> "

func Start(in io.Reader, out io.Writer) error {
	scanner := bufio.NewScanner(in)
	for {
		fmt.Printf("%s ", PROMPT)
		scanned := scanner.Scan()
		if !scanned {
			//err is nil if EOF
			return scanner.Err()
		}

		input := scanner.Text()
		if input == "exit" {
			return nil
		}

		lex := lexer.New(input)

		for tk := lex.NextToken(); tk.Type != token.EOF; tk = lex.NextToken() {
            fmt.Printf("%+v\n", tk)
		}
	}
}

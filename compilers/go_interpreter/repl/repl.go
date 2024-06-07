package repl

import (
	"bufio"
	"fmt"
	"go_interpreter/lexer"
	"go_interpreter/parser"
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

		p := parser.New(lexer.New(input))
		program := p.ParseProgram()
		if len(p.Errors()) != 0 {
			fmt.Println("Syntax errors!")
			for _, msg := range p.Errors() {
				io.WriteString(out, "\t"+msg+"\n")
			}
			continue
		} else {
			fmt.Printf("%s\n", program.String())
		}
	}
}

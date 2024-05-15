package parser

import (
	"go_interpreter/ast"
	"go_interpreter/lexer"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestLetStatements(t *testing.T) {
	input := `
let x = 5;
let y = 10;
let foobar = 838383;
`

	p := New(lexer.New(input))

	program := p.ParseProgram()
	assert.NotNil(t, program)
	assertNoErrors(t, p)
	assert.Len(t, program.Statements, 3)

	testCases := []struct {
		expectedIdentifier string
	}{
		{"x"},
		{"y"},
		{"foobar"},
	}

	//sanity check
	assert.Equal(t, len(testCases), len(program.Statements))
	for i, testCase := range testCases {
		s := program.Statements[i]
		testLetStatement(t, s, testCase.expectedIdentifier)
	}
}

func TestLetStatementErrors(t *testing.T) {
	input := `
let x  5;
let  = 10;
`

	p := New(lexer.New(input))

	program := p.ParseProgram()
	assert.NotNil(t, program)
	assert.Len(t, program.Statements, 0)
    assert.Len(t, p.errors, 2)
    assert.Equal(t, "Expect next token to be =. Found INT", p.errors[0])
    assert.Equal(t, "Expect next token to be IDENT. Found =", p.errors[1])
}

func testLetStatement(t *testing.T, s ast.Statement, name string) {
	assert.Equal(t, "let", s.TokenLiteral())

	letStatement, ok := s.(*ast.LetStatement)
	assert.True(t, ok, "Statement %v+ not of type LetStatement", s)

	assert.Equal(t, name, letStatement.Identifier.TokenLiteral())
}

func assertNoErrors(t *testing.T, parser *Parser) {
	errorCount := len(parser.errors)
	if errorCount > 0 {
		t.Errorf("parser has %d errors", errorCount)
		for _, msg := range parser.errors {
			t.Errorf("parser error: %q", msg)
		}
		t.FailNow()
	}
}

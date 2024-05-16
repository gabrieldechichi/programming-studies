package parser

import (
	"fmt"
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
    fmt.Println(program.String())
	assert.NotNil(t, program)
	assert.Len(t, program.Statements, 0)
	assert.Len(t, p.errors, 2)
	assert.Equal(t, "Expect next token to be =. Found INT", p.errors[0])
	assert.Equal(t, "Expect next token to be IDENT. Found =", p.errors[1])
}

func TestReturnStatements(t *testing.T) {
	input := `
return 5;
return 10;
return 993322;
`
	p := New(lexer.New(input))

	program := p.ParseProgram()
	assert.NotNil(t, program)
	assertNoErrors(t, p)
	assert.Len(t, program.Statements, 3)

	testCases := []struct {
		// expectedIdentifier string
	}{
		{},
		{},
		{},
	}

	//sanity check
	assert.Equal(t, len(testCases), len(program.Statements))
	for i, _ := range testCases {
		s := program.Statements[i]
		testReturnStatement(t, s)
	}
}

func TestProgramToString(t*testing.T) {
    input := "let x = 5;"
    p := New(lexer.New(input))
    program := p.ParseProgram()
    assertNoErrors(t, p)
    str := program.String()
    //todo: expressions
    _ = str
    // assert.Equal(t, input, str)
}

func TestIdentifierExpression(t*testing.T) {
    input := "foobar;"
    p := New(lexer.New(input))
    program := p.ParseProgram()
    assertNoErrors(t, p)
    exprStrm, ok := program.Statements[0].(*ast.ExpressionStatement)
    assert.True(t, ok)
    identifierExpr, ok := exprStrm.Expression.(*ast.Identifier)
    assert.True(t, ok)

    assert.Equal(t, "foobar", identifierExpr.Value)
}

func testReturnStatement(t*testing.T, s ast.Statement) {
    assert.Equal(t, "return", s.TokenLiteral())

    _, ok := s.(*ast.ReturnStatement)
	assert.True(t, ok, "Statement %v+ not of type ReturnStatement", s)
    //TODO: test expressions
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

package parser

import (
	"fmt"
	"go_interpreter/ast"
	"go_interpreter/lexer"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
)

func parseProgramAndAssert(t *testing.T, input string) (*ast.Program, *Parser) {
	lineCount := len(strings.Split(strings.Trim(strings.TrimSpace(input), "\n"), "\n"))
	p := New(lexer.New(input))
	program := p.ParseProgram()
	assert.NotNil(t, program)
	assertNoErrors(t, p)
    if len(program.Statements) != lineCount{
        fmt.Printf("%d, %s, %s, %v\n\n", lineCount, input, program.String(), program.Statements)
    }
	assert.Len(t, program.Statements, lineCount)
	return program, p
}

func testProgramParsing[T any](t *testing.T, input string, testCases []T, f func(t *testing.T, testCase T, s ast.Statement)) {
	program, _ := parseProgramAndAssert(t, input)

	assert.Equal(t, len(testCases), len(program.Statements))

	for i, testCase := range testCases {
		s := program.Statements[i]
		f(t, testCase, s)
	}
}

func castAssert[U any](t *testing.T, v interface{}) *U {
	r, ok := v.(*U)
	assert.True(t, ok, "Value %v not of correct type. Expected *%T. Found %T", v, *new(U), v)
	return r
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

func TestLetStatements(t *testing.T) {
	input := `
let x = 5;
let y = 10;
let foobar = 838383;
`
	type TestCase struct {
		expectedIdentifier string
	}

	testCases := []TestCase{
		{"x"},
		{"y"},
		{"foobar"},
	}

	testProgramParsing(t, input, testCases, func(t *testing.T, testCase TestCase, s ast.Statement) {
		assert.Equal(t, "let", s.TokenLiteral())

		letStatement := castAssert[ast.LetStatement](t, s)

		assert.Equal(t, testCase.expectedIdentifier, letStatement.Identifier.TokenLiteral())

		//todo: test expressions
	})
}

func TestIntegerExpressions(t *testing.T) {
	input := "5"
	testCases := []int64{5}
	testProgramParsing(t, input, testCases, func(t *testing.T, testCase int64, s ast.Statement) {
		exprStmt := castAssert[ast.ExpressionStatement](t, s)
		intLiteral := castAssert[ast.IntegerLiteral](t, exprStmt.Expression)
		assert.Equal(t, testCase, intLiteral.Value)
	})
}

func TestPrefixExpressions(t *testing.T) {
	input := `
!5;
-10;
`
	type TestCase struct {
		operator string
		number   int64
	}

	testCases := []TestCase{
		{"!", 5},
		{"-", 10},
	}

	testProgramParsing(t, input, testCases, func(t *testing.T, testCase TestCase, s ast.Statement) {
		expr := castAssert[ast.ExpressionStatement](t, s)
		prefixExpr := castAssert[ast.PrefixExpression](t, expr.Expression)
		assert.Equal(t, testCase.operator, prefixExpr.Operator)

		intLit := castAssert[ast.IntegerLiteral](t, prefixExpr.Right)
		assert.Equal(t, testCase.number, intLit.Value)
	})
}

func TestInfixExpressions(t *testing.T) {
	input := `
5+5;
5-5;
5*5;
5/5;
5>5;
5<5;
5==5;
5!=5;
`
	type TestCase struct {
		left     int64
		right    int64
		operator string
	}

	testCases := []TestCase{
		{5, 5, "+"},
		{5, 5, "-"},
		{5, 5, "*"},
		{5, 5, "/"},
		{5, 5, ">"},
		{5, 5, "<"},
		{5, 5, "=="},
		{5, 5, "!="},
	}

	testProgramParsing(t, input, testCases, func(t *testing.T, testCase TestCase, s ast.Statement) {
		expr := castAssert[ast.ExpressionStatement](t, s)
		infixExpr := castAssert[ast.InfixExpression](t, expr.Expression)
		assert.Equal(t, testCase.operator, infixExpr.Operator)

		left := castAssert[ast.IntegerLiteral](t, infixExpr.Left)
		right := castAssert[ast.IntegerLiteral](t, infixExpr.Right)
		assert.Equal(t, testCase.left, left.Value)
		assert.Equal(t, testCase.right, right.Value)
	})
}

func TestOperatorPrecedenceParsing(t *testing.T) {
	tests := []struct {
		input    string
		expected string
	}{
		{
			"-a * b",
			"((-a) * b)",
		},
		{
			"!-a",
			"(!(-a))",
		},
		{
			"a + b + c",
			"((a + b) + c)",
		},
		{
			"a + b - c",
			"((a + b) - c)",
		},
		{
			"a * b * c",
			"((a * b) * c)",
		},
		{
			"a * b / c",
			"((a * b) / c)",
		},
		{
			"a + b / c",
			"(a + (b / c))",
		},
		{
			"a + b * c + d / e - f",
			"(((a + (b * c)) + (d / e)) - f)",
		},
		{
			"3 + 4;\n-5 * 5",
			"(3 + 4)((-5) * 5)",
		},
		{
			"5 > 4 == 3 < 4",
			"((5 > 4) == (3 < 4))",
		},
		{
			"5 < 4 != 3 > 4",
			"((5 < 4) != (3 > 4))",
		},
		{
			"3 + 4 * 5 == 3 * 1 + 4 * 5",
			"((3 + (4 * 5)) == ((3 * 1) + (4 * 5)))",
		},
		{
			"3 + 4 * 5 == 3 * 1 + 4 * 5",
			"((3 + (4 * 5)) == ((3 * 1) + (4 * 5)))",
		},
	}

    for _, testCase := range tests {
        program, _ := parseProgramAndAssert(t, testCase.input)
        actual := program.String()
        assert.Equal(t, testCase.expected, actual)
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
    fmt.Println(program.Statements)
	assert.Len(t, p.errors, 3)
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
		assert.Equal(t, "return", s.TokenLiteral())

		r := castAssert[ast.ReturnStatement](t, s)
		_ = r
		//TODO: test expressions
	}
}

func TestProgramToString(t *testing.T) {
	input := "let x = 5;"
	p := New(lexer.New(input))
	program := p.ParseProgram()
	assertNoErrors(t, p)
	str := program.String()
	//todo: expressions
	_ = str
	// assert.Equal(t, input, str)
}

func TestIdentifierExpression(t *testing.T) {
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

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
	if len(program.Statements) != lineCount {
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
		expr               string
	}

	testCases := []TestCase{
		{"x", "5"},
		{"y", "10"},
		{"foobar", "838383"},
	}

	testProgramParsing(t, input, testCases, func(t *testing.T, testCase TestCase, s ast.Statement) {
		assert.Equal(t, "let", s.TokenLiteral())

		letStatement := castAssert[ast.LetStatement](t, s)

		assert.Equal(t, testCase.expectedIdentifier, letStatement.Identifier.TokenLiteral())

		assertCodeText(t, testCase.expr, letStatement.Value.String())
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

func TestBooleanExpressions(t *testing.T) {
	input := `true;
false;
    `
	testCases := []bool{true, false}
	testProgramParsing(t, input, testCases, func(t *testing.T, testCase bool, s ast.Statement) {
		exprStmt := castAssert[ast.ExpressionStatement](t, s)
		boolLiteral := castAssert[ast.BooleanLiteral](t, exprStmt.Expression)
		assert.Equal(t, testCase, boolLiteral.Value)
	})
}

func TestStringExpression(t *testing.T) {
	input := `"foobar";
"foo bar";`
	testCases := []string{"foobar", "foo bar"}
	testProgramParsing(t, input, testCases, func(t *testing.T, testCase string, s ast.Statement) {
		exprStmt := castAssert[ast.ExpressionStatement](t, s)
		stringLiteral := castAssert[ast.StringLiteral](t, exprStmt.Expression)
		assert.Equal(t, testCase, stringLiteral.Value)
	})
}

func TestArrayExpression(t *testing.T) {
	input := `[1, 2 * 2, 3 + 3];
[1, "foobar", true];
["foobar", [1,2]];
    `
	testCases := [][]string{
		{"1", "(2 * 2)", "(3 + 3)"},
		{"1", `"foobar"`, "true"},
		{`"foobar"`, "[1,2]"},
	}
	testProgramParsing(t, input, testCases, func(t *testing.T, testCase []string, s ast.Statement) {
		exprStmt := castAssert[ast.ExpressionStatement](t, s)
		arrayLiteral := castAssert[ast.ArrayExpression](t, exprStmt.Expression)
		assert.Len(t, arrayLiteral.Args, len(testCase))
		for i := range testCase {
			expected := testCase[i]
			actual := arrayLiteral.Args[i].String()
			assert.Equal(t, expected, actual)
		}
	})
}

func TestArrayIndexing(t *testing.T) {
	input := `x[1];
[1,2,3][0];
"foo"[0];
myArray[1 + 2];
y[getIndex()];
y[x[0]];
getArray()[1];
`
    type TestCase struct {
        left string
        index string
    }
	testCases := []TestCase{
        {"x", "1"},
        {"[1,2,3]", "0"},
        {`"foo"`, "0"},
        {"myArray", "(1 + 2)"},
        {"y", "getIndex()"},
        {"y", "(x[0])"},
        {"getArray()", "1"},
	}

	testProgramParsing(t, input, testCases, func(t *testing.T, testCase TestCase, s ast.Statement) {
		exprStmt := castAssert[ast.ExpressionStatement](t, s)
		indexArrayExpr := castAssert[ast.IndexArrayExpression](t, exprStmt.Expression)
        assert.Equal(t, testCase.left, indexArrayExpr.Left.String())
        assert.Equal(t, testCase.index, indexArrayExpr.Index.String())
	})
}

func TestPrefixExpressions(t *testing.T) {
	input := `
!5;
-10;
!true;
!false;
`
	type TestCase struct {
		operator string
		value    interface{}
	}

	testCases := []TestCase{
		{"!", 5},
		{"-", 10},
		{"!", true},
		{"!", false},
	}

	testProgramParsing(t, input, testCases, func(t *testing.T, testCase TestCase, s ast.Statement) {
		expr := castAssert[ast.ExpressionStatement](t, s)
		prefixExpr := castAssert[ast.PrefixExpression](t, expr.Expression)
		assert.Equal(t, testCase.operator, prefixExpr.Operator)

		testLiteralExpression(t, prefixExpr.Right, testCase.value)
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
5<=5;
5>=5;
true == false;
true != false;
`
	type TestCase struct {
		left     interface{}
		right    interface{}
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
		{5, 5, "<="},
		{5, 5, ">="},
		{true, false, "=="},
		{true, false, "!="},
	}

	testProgramParsing(t, input, testCases, func(t *testing.T, testCase TestCase, s ast.Statement) {
		expr := castAssert[ast.ExpressionStatement](t, s)
		infixExpr := castAssert[ast.InfixExpression](t, expr.Expression)
		assert.Equal(t, testCase.operator, infixExpr.Operator)

		testLiteralExpression(t, infixExpr.Left, testCase.left)
		testLiteralExpression(t, infixExpr.Right, testCase.right)
	})
}

func testLiteralExpression(t *testing.T, expr ast.Expression, expected interface{}) {
	switch v := expected.(type) {
	case int:
		intLit := castAssert[ast.IntegerLiteral](t, expr)
		assert.Equal(t, int64(v), intLit.Value)
	case bool:
		boolLit := castAssert[ast.BooleanLiteral](t, expr)
		assert.Equal(t, bool(v), boolLit.Value)
	}
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
			"1 + 2 * 3",
			"(1 + (2 * 3))",
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
		{
			"3 > 5 == true",
			"((3 > 5) == true)",
		},
		{
			"3 == 5 != false",
			"((3 == 5) != false)",
		},
		{
			"1 + (2 + 3) + 4",
			"((1 + (2 + 3)) + 4)",
		},
		{
			"(5 + 5) * 2",
			"((5 + 5) * 2)",
		},
		{
			"2 / (5 + 5)",
			"(2 / (5 + 5))",
		},
		{
			"-(5 + 5)",
			"(-(5 + 5))",
		},
		{
			"!(true == true)",
			"(!(true == true))",
		},
        {
            "a * [1, 2, 3, 4][b * c] * d",
            "((a * ([1, 2, 3, 4][(b * c)])) * d)",
        },
        {
            "add(a * b[2], b[1], 2 * [1, 2][1])",
            "add((a * (b[2])), (b[1]), (2 * ([1, 2][1])))",
        },
	}

	for _, testCase := range tests {
		program, _ := parseProgramAndAssert(t, testCase.input)
		actual := program.String()
		assertCodeText(t, testCase.expected, actual)
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
	assert.Len(t, p.errors, 3)
	assert.Equal(t, "Expect next token to be =. Found INT", p.errors[0])
	assert.Equal(t, "Expect next token to be IDENT. Found =", p.errors[1])
}

func TestReturnStatements(t *testing.T) {
	input := `
return 5;
return x + 1;
return x + y * 2;
return add(1,2);
`
	type TestCase struct {
		expectedIdentifier string
	}

	testCases := []TestCase{
		{"5"},
		{"(x+1)"},
		{"(x + (y*2))"},
		{"add(1,2)"},
	}

	testProgramParsing(t, input, testCases, func(t *testing.T, testCase TestCase, s ast.Statement) {
		assert.Equal(t, "return", s.TokenLiteral())

		r := castAssert[ast.ReturnStatement](t, s)
		assertCodeText(t, testCase.expectedIdentifier, r.Expression.String())
	})
}

func TestProgramToString(t *testing.T) {
	input := `
let print = fn(v) {
    log(v)
};

let x = 5;
let y = 4;

if (x > 5) {
    print(y+x)
} else {
    print(1 + x * 5)
}

if (x <= 5) {
    print((y+x))
}

let x = "foobar";
`
	expected := `
let print = fn(v) {
    log(v)
};

let x = 5;
let y = 4;

if ((x > 5)) {
    print((y+x))
} else {
    print((1 + (x * 5)))
}

if ((x <= 5)) {
    print((y+x))
}

let x = "foobar"
`

	p := New(lexer.New(input))
	program := p.ParseProgram()
	assertNoErrors(t, p)
	str := program.String()
	assertCodeText(t, expected, str)
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

func TestIfExpressions(t *testing.T) {
	input := `
if (x < y) { x }
if (x < y) { x } else { y }
`
	type TestCase struct {
		condition      string
		consequence    string
		hasAlternative bool
		alternative    string
	}

	testCases := []TestCase{
		{"(x < y)", "x", false, ""},
		{"(x < y)", "x", true, "y"},
	}

	testProgramParsing(t, input, testCases, func(t *testing.T, testCase TestCase, s ast.Statement) {
		assert.Equal(t, "if", s.TokenLiteral())

		exprStmt := castAssert[ast.ExpressionStatement](t, s)
		ifExpr := castAssert[ast.IfExpression](t, exprStmt.Expression)

		assert.Equal(t, testCase.condition, ifExpr.Condition.String())
		assert.Equal(t, testCase.consequence, ifExpr.Consequence.String())
		if testCase.hasAlternative {
			assert.NotNil(t, ifExpr.Alternative)
			assert.Equal(t, testCase.alternative, ifExpr.Alternative.String())
		}
	})
}

func TestFunctionExpression(t *testing.T) {
	input := `
fn() {};
fn() { return 2 * 2; };
fn(x) {};
fn(x, y, z) {};
fn(x, y, z) { return x + y + z; };
`
	type TestCase struct {
		parameters []string
		body       string
	}

	testCases := []TestCase{
		{[]string{}, ""},
		{[]string{}, "return (2 * 2);"},
		{[]string{"x"}, ""},
		{[]string{"x", "y", "z"}, ""},
		{[]string{"x", "y", "z"}, "return ((x + y) + z);"},
	}

	testProgramParsing(t, input, testCases, func(t *testing.T, testCase TestCase, s ast.Statement) {
		assert.Equal(t, "fn", s.TokenLiteral())

		exprStmt := castAssert[ast.ExpressionStatement](t, s)
		fnExpr := castAssert[ast.FunctionExpression](t, exprStmt.Expression)

		assert.Len(t, fnExpr.Parameters, len(testCase.parameters))
		for i := range fnExpr.Parameters {
			identifier := fnExpr.Parameters[i]
			expected := testCase.parameters[i]
			assert.Equal(t, expected, identifier.Value)
		}
		assert.Equal(t, testCase.body, fnExpr.Body.String())
	})
}

func cleanCodeString(s string) string {
	return strings.ReplaceAll(strings.ReplaceAll(strings.ReplaceAll(strings.ReplaceAll(s, " ", ""), "\t", ""), "\n", ""), ";", "")
}

func assertCodeText(t *testing.T, expected string, actual string) {
	assert.Equal(t, cleanCodeString(expected), cleanCodeString(actual))
}

func TestCallExpression(t *testing.T) {
	input := `add(2, 3);
add(2+3*4, 2 * 4 + 4);
add((2+3)*4, 3);
fn(x, y) { x + y; }(2, 3);
add(pow(2,3), 3);
`
	type TestCase struct {
		function  string
		arguments []string
	}

	testCases := []TestCase{
		{"add", []string{"2", "3"}},
		{"add", []string{"(2 + (3 * 4))", "((2 * 4) + 4)"}},
		{"add", []string{"((2 + 3) * 4)", "3"}},
		{"fn(x, y) { (x + y) }", []string{"2", "3"}},
		{"add", []string{"pow(2,3)", "3"}},
	}

	testProgramParsing(t, input, testCases, func(t *testing.T, testCase TestCase, s ast.Statement) {
		exprStmt := castAssert[ast.ExpressionStatement](t, s)
		callExpr := castAssert[ast.CallExpression](t, exprStmt.Expression)
		assertCodeText(t, testCase.function, callExpr.Function.String())
		assert.Len(t, callExpr.Arguments, len(testCase.arguments))

		for i := range testCase.arguments {
			assertCodeText(t, testCase.arguments[i], callExpr.Arguments[i].String())
		}
	})
}

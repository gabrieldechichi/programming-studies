package evaluator

import (
	"go_interpreter/lexer"
	"go_interpreter/object"
	"go_interpreter/parser"
	"strings"
	"testing"

	"github.com/stretchr/testify/assert"
)

func testEval(t *testing.T, input string) object.Object {
	result := testEvalNoErrorCheck(input)
	if error, isError := result.(*object.ErrorObj); isError {
		t.Fatalf("Eval returned error: %s", error.Inspect())
	}
	return result
}

func testEvalNoErrorCheck(input string) object.Object {
	l := lexer.New(input)
	p := parser.New(l)
	program := p.ParseProgram()
	env := object.NewEnvironment()
	return EvalProgram(program, env)
}

// REPEATED CODE
func castAssert[U any](t *testing.T, v interface{}) *U {
	r, ok := v.(*U)
	assert.True(t, ok, "Value %v not of correct type. Expected *%T. Found %T", v, *new(U), v)
	return r
}

func cleanCodeString(s string) string {
	return strings.ReplaceAll(strings.ReplaceAll(strings.ReplaceAll(strings.ReplaceAll(s, " ", ""), "\t", ""), "\n", ""), ";", "")
}

func assertCodeText(t *testing.T, expected string, actual string) {
	assert.Equal(t, cleanCodeString(expected), cleanCodeString(actual))
}

//end repeated code

func assertNativeAndObjectEqual(t *testing.T, v interface{}, o object.Object) {
	switch value := v.(type) {
	case bool:
		boolValue := castAssert[object.BooleanObj](t, o)
		assert.Equal(t, value, boolValue.Value)
	case int:
		intValue := castAssert[object.IntegerObj](t, o)
		assert.Equal(t, int64(value), int64(intValue.Value))
	case string:
		if err, isErr := o.(*object.ErrorObj); isErr {
            assert.Equal(t, value, err.Value)
		} else {
			stringValue := castAssert[object.StringObj](t, o)
			assert.Equal(t, value, stringValue.Value)
		}
	case nil:
		castAssert[object.NullObj](t, o)
	}
}

func TestEvalIntegerExpression(t *testing.T) {
	tests := []struct {
		input    string
		expected int64
	}{
		{"5", 5},
		{"10", 10},
		{"-5", -5},
		{"-10", -10},
		{"5 + 5 + 5 + 5 - 10", 10},
		{"2 * 2 * 2 * 2 * 2", 32},
		{"-50 + 100 + -50", 0},
		{"5 * 2 + 10", 20},
		{"5 + 2 * 10", 25},
		{"20 + 2 * -10", 0},
		{"50 / 2 * 2 + 10", 60},
		{"2 * (5 + 10)", 30},
		{"3 * 3 * 3 + 10", 37},
		{"3 * (3 * 3) + 10", 37},
		{"(5 + 10 * 2 + 15 / 3) * 2 + -10", 50},
	}
	for _, tt := range tests {
		obj := testEval(t, tt.input)
		result := castAssert[object.IntegerObj](t, obj)
		assert.Equal(t, tt.expected, result.Value)
	}
}

func TestEvalBooleanExpression(t *testing.T) {
	tests := []struct {
		input    string
		expected bool
	}{
		{"true", true},
		{"false", false},
		{"1 < 2", true},
		{"1 > 2", false},
		{"1 < 1", false},
		{"1 > 1", false},
		{"1 == 1", true},
		{"1 != 1", false},
		{"1 == 2", false},
		{"1 != 2", true},
		{"1 >= 2", false},
		{"1 <= 2", true},
		{"2 <= 2", true},
		{"true == true", true},
		{"false == false", true},
		{"true == false", false},
		{"true != false", true},
		{"false != true", true},
		{"(1 < 2) == true", true},
		{"(1 < 2) == false", false},
		{"(1 > 2) == true", false},
		{"(1 > 2) == false", true},
	}

	for _, tt := range tests {
		obj := testEval(t, tt.input)
		result := castAssert[object.BooleanObj](t, obj)
		assert.Equal(t, tt.expected, result.Value)
	}
}

func TestEvalStringExpression(t *testing.T) {
	tests := []struct {
		input    string
		expected interface{}
	}{
		{`"5"`, "5"},
		{`"foo bar"`, "foo bar"},
		{`"foo" + "bar"`, "foobar"},
		{`"foo" + "bar" + " " + "bar"`, "foobar bar"},
		{`"foo" == "bar"`, false},
		{`"foo" != "bar"`, true},
	}
	for _, tt := range tests {
		obj := testEval(t, tt.input)
		assertNativeAndObjectEqual(t, tt.expected, obj)
	}
}

func TestEvalBandOperator(t *testing.T) {
	tests := []struct {
		input    string
		expected bool
	}{
		{"!true", false},
		{"!false", true},
		{"!5", false},
		{"!!true", true},
		{"!!false", false},
		{"!!5", true},
	}

	for _, tt := range tests {
		obj := testEval(t, tt.input)
		result := castAssert[object.BooleanObj](t, obj)
		assert.Equal(t, tt.expected, result.Value)
	}
}

func TestIfElseExpressions(t *testing.T) {
	tests := []struct {
		input    string
		expected interface{}
	}{
		{"if (true) { 10 }", 10},
		{"if (false) { 10 }", nil},
		{"if (1) { 10 }", 10},
		{"if (1 < 2) { 10 }", 10},
		{"if (1 > 2) { 10 }", nil},
		{"if (1 > 2) { 10 } else { 20 }", 20},
		{"if (1 < 2) { 10 } else { 20 }", 10},
		{`if (1 < 2) { "hey" }`, "hey"},
		{`if ("hey" == "hey") { "yo" }`, "yo"},
		{`
if (1 < 2) {
    5 + 5;
    5 + 10;
    5 + 5;
} else { 20 }`, 10},
	}

	for _, tt := range tests {
		obj := testEval(t, tt.input)
		assertNativeAndObjectEqual(t, tt.expected, obj)
	}
}

func TestReturnStatement(t *testing.T) {
	tests := []struct {
		input    string
		expected int64
	}{
		{"return 10;", 10},
		{"return 10; 9;", 10},
		{"return 2 * 5; 9;", 10},
		{"9; return 2 * 5; 9;", 10},
		{`
if (10 > 1) {
    if (10 > 1){
        return 10
    }
    return 1
}`, 10},
	}
	for _, tt := range tests {
		obj := testEval(t, tt.input)
		assertNativeAndObjectEqual(t, tt.expected, obj)
	}
}
func TestErrorHandling(t *testing.T) {
	tests := []struct {
		input           string
		expectedMessage string
	}{
		{
			"5 + true;",
			"type mismatch: INTEGER + BOOLEAN",
		},
		{
			"5 + true; 5;",
			"type mismatch: INTEGER + BOOLEAN",
		},
		{
			"-true",
			"unknown operator: -BOOLEAN",
		},
		{
			"true + false;",
			"unknown operator: BOOLEAN + BOOLEAN",
		},
		{
			"5; true + false; 5",
			"unknown operator: BOOLEAN + BOOLEAN",
		},
		{
			"(true + true + (false * false))",
			"unknown operator: BOOLEAN + BOOLEAN",
		},
		{
			"if (10 > 1) { true + false; }",
			"unknown operator: BOOLEAN + BOOLEAN",
		},
		{
			`
132
if (10 > 1) {
    if (10 > 1) {
        return true + false;
    }
    return 1;
}
`,
			"unknown operator: BOOLEAN + BOOLEAN",
		},
		{
			"foobar",
			"identifier not found: foobar",
		},
		{
			`"foobar" - "bar"`,
			`unknown operator: STRING - STRING`,
		},
	}

	for _, tt := range tests {
		obj := testEvalNoErrorCheck(tt.input)
		errorObj := castAssert[object.ErrorObj](t, obj)
		assert.Equal(t, tt.expectedMessage, errorObj.Value)
	}
}

func TestLetStatements(t *testing.T) {
	tests := []struct {
		input    string
		expected int64
	}{
		{"let a = 5; a;", 5},
		{"let a = 5 * 5; a;", 25},
		{"let a = 5; let b = a; b;", 5},
		{"let a = 5; let b = a; let c = a + b + 5; c;", 15},
	}
	for _, tt := range tests {
		obj := testEval(t, tt.input)
		assertNativeAndObjectEqual(t, tt.expected, obj)
	}
}

func TestFunctionObject(t *testing.T) {
	tests := []struct {
		input        string
		expectedArgs []string
	}{
		{"fn (x) { (x + 1) }", []string{"x"}},
	}
	for _, tt := range tests {
		obj := testEval(t, tt.input)
		funcObj := castAssert[object.FunctionObj](t, obj)
		assert.Len(t, funcObj.Arguments, len(tt.expectedArgs))
		for i := range funcObj.Arguments {
			actual := funcObj.Arguments[i].Value
			expected := tt.expectedArgs[i]
			assert.Equal(t, expected, actual)
		}
		assertCodeText(t, tt.input, funcObj.Inspect())
	}
}

func TestFunctionApplication(t *testing.T) {
	tests := []struct {
		input    string
		expected interface{}
	}{
		{"let identity = fn(x) { x; }; identity(5);", 5},
		{"let identity = fn(x) { return true; }; identity(5);", true},
		{"let double = fn(x) { x * 2; }; double(5);", 10},
		{"let add = fn(x, y) { x + y; }; add(5, 5);", 10},
		{"let add = fn(x, y) { x + y; }; add(5 + 5, add(5, 5));", 20},
		{"fn(x) { x; }(5)", 5},
	}
	for _, tt := range tests {
		obj := testEval(t, tt.input)
		assertNativeAndObjectEqual(t, tt.expected, obj)
	}
}

func TestClosures(t *testing.T) {
	tests := []struct {
		input    string
		expected interface{}
	}{
		{`
let newAdder = fn(x) {
    fn(y) { x + y };
};
let addTwo = newAdder(2);
addTwo(2);`, 4},
		{`
let add = fn(a, b) { a + b };
let applyFunc = fn(a, b, func) { func(a, b) };
applyFunc(2, 2, add);`, 4},
	}
	for _, tt := range tests {
		obj := testEval(t, tt.input)
		assertNativeAndObjectEqual(t, tt.expected, obj)
	}
}

func TestBuiltinFunctions(t *testing.T) {
	tests := []struct {
		input    string
		expected interface{}
	}{
		{`len("")`, 0},
		{`len("four")`, 4},
		{`len("hello world")`, 11},
		{`len(1)`, "argument to `len` not supported, got INTEGER"},
		{`len("one", "two")`, "wrong number of arguments. got=2, want=1"},
	}

	for _, tt := range tests {
		obj := testEvalNoErrorCheck(tt.input)
		assertNativeAndObjectEqual(t, tt.expected, obj)
	}
}

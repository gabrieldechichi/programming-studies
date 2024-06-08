package evaluator

import (
	"go_interpreter/lexer"
	"go_interpreter/object"
	"go_interpreter/parser"
	"testing"

	"github.com/stretchr/testify/assert"
)

func testEval(input string) object.Object {
	l := lexer.New(input)
	p := parser.New(l)
	program := p.ParseProgram()
	return EvalProgram(program)
}

func castAssert[U any](t *testing.T, v interface{}) *U {
	r, ok := v.(*U)
	assert.True(t, ok, "Value %v not of correct type. Expected *%T. Found %T", v, *new(U), v)
	return r
}

func assertNativeAndObjectEqual(t *testing.T, v interface{}, o object.Object) {
	switch value := v.(type) {
	case bool:
		boolValue := castAssert[object.BooleanObj](t, o)
		assert.Equal(t, value, boolValue.Value)
	case int:
		intValue := castAssert[object.IntegerObj](t, o)
		assert.Equal(t, int64(value), int64(intValue.Value))
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
		obj := testEval(tt.input)
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
		obj := testEval(tt.input)
		result := castAssert[object.BooleanObj](t, obj)
		assert.Equal(t, tt.expected, result.Value)
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
		obj := testEval(tt.input)
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
		{`
if (1 < 2) {
    5 + 5;
    5 + 10;
    5 + 5;
} else { 20 }`, 10},
	}

	for _, tt := range tests {
		obj := testEval(tt.input)
		assertNativeAndObjectEqual(t, tt.expected, obj)
	}
}

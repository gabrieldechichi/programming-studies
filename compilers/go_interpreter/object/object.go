package object

import (
	"fmt"
	"go_interpreter/ast"
	"strings"
)

type ObjectType string

type Object interface {
	Inspect() string
	Type() ObjectType
}

const (
	INTEGER_OBJ  = "INTEGER"
	BOOLEAN_OBJ  = "BOOLEAN"
	STRING_OBJ  = "STRING"
	FUNCTION_OBJ = "FUNCTION"
	NULL_OBJ     = "NULL"
	RETURN_OBJ   = "RETURN"
	ERROR_OBJ    = "ERROR"
)

var (
	TRUE  = &BooleanObj{Value: true}
	FALSE = &BooleanObj{Value: false}
	NULL  = &NullObj{}
)

type IntegerObj struct {
	Value int64
}

func (i *IntegerObj) Inspect() string {
	return fmt.Sprintf("%d", i.Value)
}

func (i *IntegerObj) Type() ObjectType {
	return INTEGER_OBJ
}

type BooleanObj struct {
	Value bool
}

func (i *BooleanObj) Inspect() string {
	return fmt.Sprintf("%t", i.Value)
}

func (i *BooleanObj) Type() ObjectType {
	return BOOLEAN_OBJ
}

type StringObj struct {
	Value string
}

func (i *StringObj) Inspect() string {
	return fmt.Sprintf(`"%s"`, i.Value)
}

func (i *StringObj) Type() ObjectType {
	return STRING_OBJ
}

type NullObj struct {
}

func (i *NullObj) Inspect() string {
	return "null"
}

func (i *NullObj) Type() ObjectType {
	return NULL_OBJ
}

type ReturnObj struct {
	Value Object
}

func (i *ReturnObj) Inspect() string {
	return i.Value.Inspect()
}

func (i *ReturnObj) Type() ObjectType {
	return RETURN_OBJ
}

type ErrorObj struct {
	Value string
}

func (i *ErrorObj) Inspect() string {
	return fmt.Sprintf("ERROR: %s", i.Value)
}

func (i *ErrorObj) Type() ObjectType {
	return ERROR_OBJ
}

func (e *ErrorObj) Error() string {
	return e.Inspect()
}

type FunctionObj struct {
	Env       *Environment
	Arguments []*ast.Identifier
	Body      *ast.BlockStatement
}

func (i *FunctionObj) Inspect() string {
	var args []string
	for _, arg := range i.Arguments {
		args = append(args, arg.String())
	}
	return fmt.Sprintf("fn (%s) {\n%s\n}", strings.Join(args, ","), i.Body.String())
}

func (i *FunctionObj) Type() ObjectType {
	return FUNCTION_OBJ
}

package object

import (
	"fmt"
)

type ObjectType string

type Object interface {
	Inspect() string
	Type() ObjectType
}

const (
	INTEGER_OBJ = "int"
	BOOLEAN_OBJ = "bool"
	NULL_OBJ    = "null"
	RETURN_OBJ  = "return"
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

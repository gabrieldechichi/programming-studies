package evaluator

import (
	"fmt"
	"go_interpreter/ast"
	"go_interpreter/object"
)

func EvalProgram(program *ast.Program) object.Object {
	var result object.Object
	for _, stmt := range program.Statements {
		result = Eval(stmt)
		if result != nil && result.Type() == object.RETURN_OBJ {
			returnObj := result.(*object.ReturnObj)
			return returnObj.Value
		}
	}
	return result
}

func Eval(node ast.Node) object.Object {
	switch node := node.(type) {
	case *ast.ExpressionStatement:
		return Eval(node.Expression)
	case *ast.PrefixExpression:
		return evalPrefixExpression(node)
	case *ast.InfixExpression:
		return evalInfixExpression(node)
	case *ast.IfExpression:
		return evalIfExpression(node)
	case *ast.BlockStatement:
		return evalBlockStatement(node)
	case *ast.ReturnStatement:
		return &object.ReturnObj{Value: Eval(node.Expression)}
	case *ast.IntegerLiteral:
		return &object.IntegerObj{Value: node.Value}
	case *ast.BooleanLiteral:
		if node.Value {
			return object.TRUE
		}
		return object.FALSE
	default:
		panic(fmt.Sprintf("Unhanded Eval case %T", node))
	}
}

func evalBlockStatement(block *ast.BlockStatement) object.Object {
	var result object.Object
	for _, stmt := range block.Statements {
		result = Eval(stmt)
		if result != nil && result.Type() == object.RETURN_OBJ {
			return result
		}
	}
	return result
}

func evalIfExpression(node *ast.IfExpression) object.Object {
	cond := Eval(node.Condition)
	if isTruthy(cond) {
		return Eval(node.Consequence)
	} else if node.Alternative != nil {
		return Eval(node.Alternative)
	} else {
		return object.NULL
	}
}

func evalPrefixExpression(node *ast.PrefixExpression) object.Object {
	right := Eval(node.Right)
	switch node.Operator {
	case "!":
		{
			switch right {
			case object.TRUE:
				return object.FALSE
			case object.FALSE:
				return object.TRUE
			case object.NULL:
				return object.TRUE
			default:
				//TODO: !0 is FALSE??
				return object.FALSE
			}
		}
	case "-":
		{
			switch value := right.(type) {
			case *object.IntegerObj:
				return &object.IntegerObj{Value: -value.Value}
			default:
				return object.NULL
			}
		}
	default:
		return object.NULL
	}
}

func evalInfixExpression(node *ast.InfixExpression) object.Object {
	left := Eval(node.Left)
	right := Eval(node.Right)

	if left.Type() == object.INTEGER_OBJ && right.Type() == object.INTEGER_OBJ {
		leftNum := left.(*object.IntegerObj)
		rightNum := right.(*object.IntegerObj)
		//arithmetic operators
		switch node.Operator {
		case "+":
			return &object.IntegerObj{Value: leftNum.Value + rightNum.Value}
		case "-":
			return &object.IntegerObj{Value: leftNum.Value - rightNum.Value}
		case "*":
			return &object.IntegerObj{Value: leftNum.Value * rightNum.Value}
		case "/":
			return &object.IntegerObj{Value: leftNum.Value / rightNum.Value}
		}

		//boolean operators
		switch node.Operator {
		case "==":
			return nativeBoolToBooleanObj(leftNum.Value == rightNum.Value)
		case "!=":
			return nativeBoolToBooleanObj(leftNum.Value != rightNum.Value)
		case ">":
			return nativeBoolToBooleanObj(leftNum.Value > rightNum.Value)
		case "<":
			return nativeBoolToBooleanObj(leftNum.Value < rightNum.Value)
		case ">=":
			return nativeBoolToBooleanObj(leftNum.Value >= rightNum.Value)
		case "<=":
			return nativeBoolToBooleanObj(leftNum.Value <= rightNum.Value)
		}
	} else if left.Type() == object.BOOLEAN_OBJ && right.Type() == object.BOOLEAN_OBJ {
		leftBool := left.(*object.BooleanObj)
		rightBool := right.(*object.BooleanObj)
		switch node.Operator {
		case "==":
			return nativeBoolToBooleanObj(leftBool.Value == rightBool.Value)
		case "!=":
			return nativeBoolToBooleanObj(leftBool.Value != rightBool.Value)
		}
	}

	return object.NULL
}

func isTruthy(o object.Object) bool {
	return !(o == object.FALSE || o == object.NULL)
}

func nativeBoolToBooleanObj(b bool) *object.BooleanObj {
	if b {
		return object.TRUE
	}
	return object.FALSE
}

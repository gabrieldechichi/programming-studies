package evaluator

import (
	"fmt"
	"go_interpreter/ast"
	"go_interpreter/object"
)

func EvalProgram(program *ast.Program) object.Object {
	var result object.Object
	var err error
	for _, stmt := range program.Statements {
		result, err = Eval(stmt)
		if err != nil {
			return err.(*object.ErrorObj)
		}
		if result != nil && result.Type() == object.RETURN_OBJ {
			returnObj := result.(*object.ReturnObj)
			return returnObj.Value
		}
	}
	return result
}

func Eval(node ast.Node) (object.Object, error) {
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
		o, err := Eval(node.Expression)
		if err != nil {
			return nil, err
		}
		return &object.ReturnObj{Value: o}, nil
	case *ast.IntegerLiteral:
		return &object.IntegerObj{Value: node.Value}, nil
	case *ast.BooleanLiteral:
		if node.Value {
			return object.TRUE, nil
		}
		return object.FALSE, nil
	default:
		panic(fmt.Sprintf("Unhanded Eval case %T", node))
	}
}

func evalBlockStatement(block *ast.BlockStatement) (object.Object, error) {
	var result object.Object
	var err error
	for _, stmt := range block.Statements {
		result, err = Eval(stmt)
		if err != nil {
			return nil, err
		}
		if result != nil && result.Type() == object.RETURN_OBJ {
			return result, nil
		}
	}
	return result, nil
}

func evalIfExpression(node *ast.IfExpression) (object.Object, error) {
	cond, err := Eval(node.Condition)
	if err != nil {
		return nil, err
	}
	if isTruthy(cond) {
		return Eval(node.Consequence)
	} else if node.Alternative != nil {
		return Eval(node.Alternative)
	} else {
		return object.NULL, nil
	}
}

func evalPrefixExpression(node *ast.PrefixExpression) (object.Object, error) {
	right, err := Eval(node.Right)
	if err != nil {
		return nil, err
	}
	switch node.Operator {
	case "!":
		{
			switch right {
			case object.TRUE:
				return object.FALSE, nil
			case object.FALSE:
				return object.TRUE, nil
			case object.NULL:
				return object.TRUE, nil
			default:
				//FIX: !0 is FALSE?? !string?? Fix
				return object.FALSE, nil
			}
		}
	case "-":
		{
			switch value := right.(type) {
			case *object.IntegerObj:
				return &object.IntegerObj{Value: -value.Value}, nil
			default:
				return nil, errorObj("unknown operator: -%s", right.Type())
			}
		}
	default:
		return nil, errorObj("unknown operator: %s, on line: %s", node.Operator, node.String())
	}
}

func errorObj(format string, a ...any) error {
	return &object.ErrorObj{Value: fmt.Sprintf(format, a...)}
}

func evalInfixExpression(node *ast.InfixExpression) (object.Object, error) {
	left, err := Eval(node.Left)
	if err != nil {
		return nil, err
	}
	right, err := Eval(node.Right)
	if err != nil {
		return nil, err
	}

	if left.Type() == object.INTEGER_OBJ && right.Type() == object.INTEGER_OBJ {
		leftNum := left.(*object.IntegerObj)
		rightNum := right.(*object.IntegerObj)
		//arithmetic operators
		switch node.Operator {
		case "+":
			return &object.IntegerObj{Value: leftNum.Value + rightNum.Value}, nil
		case "-":
			return &object.IntegerObj{Value: leftNum.Value - rightNum.Value}, nil
		case "*":
			return &object.IntegerObj{Value: leftNum.Value * rightNum.Value}, nil
		case "/":
			return &object.IntegerObj{Value: leftNum.Value / rightNum.Value}, nil
		}

		//boolean operators
		switch node.Operator {
		case "==":
			return nativeBoolToBooleanObj(leftNum.Value == rightNum.Value), nil
		case "!=":
			return nativeBoolToBooleanObj(leftNum.Value != rightNum.Value), nil
		case ">":
			return nativeBoolToBooleanObj(leftNum.Value > rightNum.Value), nil
		case "<":
			return nativeBoolToBooleanObj(leftNum.Value < rightNum.Value), nil
		case ">=":
			return nativeBoolToBooleanObj(leftNum.Value >= rightNum.Value), nil
		case "<=":
			return nativeBoolToBooleanObj(leftNum.Value <= rightNum.Value), nil
		}
	} else if left.Type() == object.BOOLEAN_OBJ && right.Type() == object.BOOLEAN_OBJ {
		leftBool := left.(*object.BooleanObj)
		rightBool := right.(*object.BooleanObj)
		switch node.Operator {
		case "==":
			return nativeBoolToBooleanObj(leftBool.Value == rightBool.Value), nil
		case "!=":
			return nativeBoolToBooleanObj(leftBool.Value != rightBool.Value), nil
		}
	} else {
		return nil, errorObj("type mismatch: %s %s %s", left.Type(), node.Operator, right.Type())
	}

	return nil, errorObj("unknown operator: %s %s %s", left.Type(), node.Operator, right.Type())
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

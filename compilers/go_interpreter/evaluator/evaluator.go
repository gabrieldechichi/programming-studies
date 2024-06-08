package evaluator

import (
	"go_interpreter/ast"
	"go_interpreter/object"
)

func EvalProgram(program *ast.Program) object.Object {
	var result object.Object
	for _, stmt := range program.Statements {
		result = Eval(stmt)
	}
	return result
}

func Eval(node ast.Node) object.Object {
	switch node := node.(type) {
	case *ast.ExpressionStatement:
		return Eval(node.Expression)
	case *ast.PrefixExpression:
		return evalPrefixExpression(node)
	case *ast.IntegerLiteral:
		return &object.IntegerObj{Value: node.Value}
	case *ast.BooleanLiteral:
		if node.Value {
			return object.TRUE
		}
		return object.FALSE
	}
	return nil
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

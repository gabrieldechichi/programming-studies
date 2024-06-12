package evaluator

import (
	"fmt"
	"go_interpreter/ast"
	"go_interpreter/object"
)

func EvalProgram(program *ast.Program, env *object.Environment) object.Object {
	var result object.Object
	var err error
	for _, stmt := range program.Statements {
		result, err = Eval(stmt, env)
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

func Eval(node ast.Node, env *object.Environment) (object.Object, error) {
	switch node := node.(type) {
	case *ast.ExpressionStatement:
		return Eval(node.Expression, env)
	case *ast.PrefixExpression:
		return evalPrefixExpression(node, env)
	case *ast.InfixExpression:
		return evalInfixExpression(node, env)
	case *ast.IfExpression:
		return evalIfExpression(node, env)
	case *ast.BlockStatement:
		return evalBlockStatement(node, env)
	case *ast.ReturnStatement:
		o, err := Eval(node.Expression, env)
		if err != nil {
			return nil, err
		}
		return &object.ReturnObj{Value: o}, nil
	case *ast.LetStatement:
		return evalLetStatement(node, env)
	case *ast.Identifier:
		return evalIdentifierStatement(node, env)
	case *ast.FunctionExpression:
		return evalFunctionExpression(node, env)
	case *ast.CallExpression:
		return evalCallExpression(node, env)
	case *ast.IntegerLiteral:
		return &object.IntegerObj{Value: node.Value}, nil
	case *ast.BooleanLiteral:
		if node.Value {
			return object.TRUE, nil
		}
		return object.FALSE, nil
	case *ast.StringLiteral:
		return &object.StringObj{Value: node.Value}, nil
	default:
		panic(fmt.Sprintf("Unhanded Eval case %T", node))
	}
}

func evalCallExpression(node *ast.CallExpression, env *object.Environment) (object.Object, error) {
	//eval function
	obj, err := Eval(node.Function, env)
	if err != nil {
		return nil, err
	}
	funcObj, ok := obj.(*object.FunctionObj)
	if !ok {
		return nil, errorObj("not a function %s", obj.Inspect())
	}

	//eval arguments
	var args []object.Object
	for _, arg := range node.Arguments {
		argObj, err := Eval(arg, env)
		if err != nil {
			return nil, err
		}
		args = append(args, argObj)
	}

	//create enclosed env and bind arguments
	functionEnv := object.NewEnclosedEnvironment(funcObj.Env)
	for i := range args {
		argObj := args[i]
		identifier := funcObj.Arguments[i]
		functionEnv.Set(identifier.Value, argObj)
	}

	//evaluate function body
	result, err := Eval(funcObj.Body, functionEnv)
	if err != nil {
		return nil, err
	}

	//unwrap return value if needed
	if result, ok := result.(*object.ReturnObj); ok {
		return result.Value, nil
	}
	return result, nil
}

func evalFunctionExpression(node *ast.FunctionExpression, env *object.Environment) (object.Object, error) {
	funcObj := &object.FunctionObj{Arguments: node.Parameters, Body: node.Body, Env: env}
	return funcObj, nil
}

func evalIdentifierStatement(node *ast.Identifier, env *object.Environment) (object.Object, error) {
	identifierName := node.Value
	if val, ok := env.Get(identifierName); ok {
		return val, nil
	}
	return nil, errorObj("identifier not found: %s", identifierName)
}

func evalLetStatement(node *ast.LetStatement, env *object.Environment) (object.Object, error) {
	identifierName := node.Identifier.Value
	if _, ok := env.Get(identifierName); ok {
		return nil, errorObj("identifier %s already bound to a value", identifierName)
	}
	expr, err := Eval(node.Value, env)
	if err != nil {
		return nil, err
	}
	env.Set(identifierName, expr)
	return object.NULL, nil
}

func evalBlockStatement(block *ast.BlockStatement, env *object.Environment) (object.Object, error) {
	var result object.Object
	var err error
	for _, stmt := range block.Statements {
		result, err = Eval(stmt, env)
		if err != nil {
			return nil, err
		}
		if result != nil && result.Type() == object.RETURN_OBJ {
			return result, nil
		}
	}
	return result, nil
}

func evalIfExpression(node *ast.IfExpression, env *object.Environment) (object.Object, error) {
	cond, err := Eval(node.Condition, env)
	if err != nil {
		return nil, err
	}
	if isTruthy(cond) {
		return Eval(node.Consequence, env)
	} else if node.Alternative != nil {
		return Eval(node.Alternative, env)
	} else {
		return object.NULL, nil
	}
}

func evalPrefixExpression(node *ast.PrefixExpression, env *object.Environment) (object.Object, error) {
	right, err := Eval(node.Right, env)
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

func evalInfixExpression(node *ast.InfixExpression, env *object.Environment) (object.Object, error) {
	left, err := Eval(node.Left, env)
	if err != nil {
		return nil, err
	}
	right, err := Eval(node.Right, env)
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
	} else if left.Type() == object.STRING_OBJ && right.Type() == object.STRING_OBJ {
		leftStr := left.(*object.StringObj)
		rightStr := right.(*object.StringObj)
		switch node.Operator {
		case "+":
			return &object.StringObj{Value: leftStr.Value + rightStr.Value}, nil
		case "==":
			return nativeBoolToBooleanObj(leftStr.Value == rightStr.Value),nil
		case "!=":
			return nativeBoolToBooleanObj(leftStr.Value != rightStr.Value),nil
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

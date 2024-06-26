package evaluator

import (
	"fmt"
	"go_interpreter/ast"
	"go_interpreter/object"
)

var builtinFunctions = map[string]*object.BuiltinFunctionObj{
	"len": {
		Name: "len",
		Fn: func(args ...object.Object) object.Object {
			if len(args) != 1 {
				return errorObj("wrong number of arguments. got=%d, want=1", len(args))
			}
			arg := args[0]
			switch v := arg.(type) {
			case *object.StringObj:
				return &object.IntegerObj{Value: int64(len(v.Value))}
			case *object.ArrayObj:
				return &object.IntegerObj{Value: int64(len(v.Elements))}
			default:
				return errorObj("argument to `len` not supported, got %s", arg.Type())
			}
		},
	},
	"first": {
		Name: "first",
		Fn: func(args ...object.Object) object.Object {
			if len(args) != 1 {
				return errorObj("wrong number of arguments. got=%d, want=1", len(args))
			}
			obj, err := executeObjectIndex(args[0], 0)
			if err != nil {
				return err.(*object.ErrorObj)
			}
			return obj
		},
	},
	"last": {
		Name: "last",
		Fn: func(args ...object.Object) object.Object {
			if len(args) != 1 {
				return errorObj("wrong number of arguments. got=%d, want=1", len(args))
			}
			var l int
			switch arg := args[0].(type) {
			case *object.ArrayObj:
				l = len(arg.Elements)
			case *object.StringObj:
				l = len(arg.Value)
			default:
				return errorObj("unsupported argument type %s", args[0].Type())
			}
			obj, err := executeObjectIndex(args[0], l-1)
			if err != nil {
				return err.(*object.ErrorObj)
			}
			return obj
		},
	},
	"push": {
		Name: "push",
		Fn: func(args ...object.Object) object.Object {
			if len(args) != 2 {
				return errorObj("wrong number of arguments. got=%d, want=2", len(args))
			}
			array, ok := args[0].(*object.ArrayObj)
			if !ok {
				return errorObj("mismatched argument type for position 0. Expected array, got %s", args[0].Type())
			}

			newLen := len(array.Elements) + 1
			element := args[1]
			newArray := make([]object.Object, newLen, newLen)
			copy(newArray, array.Elements)
			newArray[newLen-1] = element
			return &object.ArrayObj{Elements: newArray}
		},
	},
}

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
	case *ast.ArrayExpression:
		o := &object.ArrayObj{}
		for _, elementExpr := range node.Args {
			element, err := Eval(elementExpr, env)
			if err != nil {
				return nil, err
			}
			o.Elements = append(o.Elements, element)
		}
		return o, nil
	case *ast.IndexArrayExpression:
		return evalIndexArrayExpression(node, env)

	default:
		panic(fmt.Sprintf("Unhanded Eval case %T", node))
	}
}

func executeObjectIndex(leftObj object.Object, index int) (object.Object, error) {
	switch left := leftObj.(type) {
	case *object.ArrayObj:
		if index < 0 || index >= len(left.Elements) {
			return object.NULL, nil
		}
		return left.Elements[index], nil
	case *object.StringObj:
		if index < 0 || index >= len(left.Value) {
			return object.NULL, nil
		}
		return &object.StringObj{Value: string(left.Value[index])}, nil
	default:
		return nil, errorObj("Expected array. Found true (%s)", leftObj.Type())
	}
}

func evalIndexArrayExpression(node *ast.IndexArrayExpression, env *object.Environment) (object.Object, error) {
	indexObj, err := Eval(node.Index, env)
	if err != nil {
		return nil, err
	}
	index, ok := indexObj.(*object.IntegerObj)
	if !ok {
		return nil, errorObj("Index must be an integer. Found %s (%s)", indexObj.Inspect(), indexObj.Type())
	}

	leftObj, err := Eval(node.Left, env)
	if err != nil {
		return nil, err
	}

	return executeObjectIndex(leftObj, int(index.Value))
}

func evalCallExpression(node *ast.CallExpression, env *object.Environment) (object.Object, error) {
	//eval function
	obj, err := Eval(node.Function, env)
	if err != nil {
		return nil, err
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

	switch funcObj := obj.(type) {
	case *object.FunctionObj:
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
	case *object.BuiltinFunctionObj:
		result := funcObj.Fn(args...)
		if result.Type() == object.ERROR_OBJ {
			return nil, result.(*object.ErrorObj)
		}
		return result, nil
	default:
		return nil, errorObj("not a function %s", obj.Inspect())
	}
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
	if val, ok := builtinFunctions[identifierName]; ok {
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

func errorObj(format string, a ...any) *object.ErrorObj {
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
			return nativeBoolToBooleanObj(leftStr.Value == rightStr.Value), nil
		case "!=":
			return nativeBoolToBooleanObj(leftStr.Value != rightStr.Value), nil
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

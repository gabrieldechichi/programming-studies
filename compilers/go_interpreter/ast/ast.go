package ast

import (
	"bytes"
	"fmt"
	"go_interpreter/token"
)

type Node interface {
	TokenLiteral() string
	String() string
}

type Statement interface {
	Node
	statementNode()
}

type Expression interface {
	Node
	expressionNode()
}

type Program struct {
	Statements []Statement
}

func (p *Program) TokenLiteral() string {
	if len(p.Statements) > 0 {
		return p.Statements[0].TokenLiteral()
	} else {
		return ""
	}
}

func (p *Program) String() string {
	var out bytes.Buffer

	for _, s := range p.Statements {
		out.WriteString(s.String())
	}
	return out.String()
}

type LetStatement struct {
	Token      token.Token
	Identifier *Identifier
	Value      Expression
}

func (s *LetStatement) TokenLiteral() string {
	return s.Token.Literal
}

func (s *LetStatement) String() string {
	return fmt.Sprintf("let %s = %s", s.Identifier.String(), s.Value.String())
}

func (s *LetStatement) statementNode() {}

type Identifier struct {
	Token token.Token
	Value string
}

func (s *Identifier) TokenLiteral() string {
	return s.Token.Literal
}

func (s *Identifier) String() string {
	return s.Value
}
func (s *Identifier) expressionNode() {}

type ReturnStatement struct {
	Token      token.Token
	Expression Expression
}

func (s *ReturnStatement) TokenLiteral() string { return s.Token.Literal }
func (s *ReturnStatement) String() string {
	return fmt.Sprintf("return %s;", s.Expression.String())
}
func (s *ReturnStatement) statementNode() {}

type ExpressionStatement struct {
	Token      token.Token
	Expression Expression
}

func (s *ExpressionStatement) TokenLiteral() string { return s.Token.Literal }

func (s *ExpressionStatement) String() string {
	if s.Expression == nil {
		return ""
	}
	return fmt.Sprintf("%s", s.Expression.String())
}

func (s *ExpressionStatement) statementNode() {}

type IntegerLiteral struct {
	Token token.Token
	Value int64
}

func (s *IntegerLiteral) TokenLiteral() string { return s.Token.Literal }

func (s *IntegerLiteral) String() string {
	return s.Token.Literal
}
func (s *IntegerLiteral) expressionNode() {}

type BooleanLiteral struct {
	Token token.Token
	Value bool
}

func (s *BooleanLiteral) TokenLiteral() string { return s.Token.Literal }
func (s *BooleanLiteral) String() string  { return s.Token.Literal }
func (s *BooleanLiteral) expressionNode() {}

type PrefixExpression struct {
	Token    token.Token
	Operator string
	Right    Expression
}

func (s *PrefixExpression) expressionNode() {}

func (s *PrefixExpression) TokenLiteral() string { return s.Token.Literal }

func (s *PrefixExpression) String() string {
	return fmt.Sprintf("(%s%s)", s.Operator, s.Right.String())
}

type InfixExpression struct {
	Token    token.Token
	Left     Expression
	Operator string
	Right    Expression
}

func (s *InfixExpression) expressionNode() {}

func (s *InfixExpression) TokenLiteral() string { return s.Token.Literal }

func (s *InfixExpression) String() string {
	return fmt.Sprintf("(%s %s %s)", s.Left.String(), s.Operator, s.Right.String())
}

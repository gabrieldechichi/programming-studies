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
}

type Expression interface {
	Node
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

func (p* Program) String() string {
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
	//todo: expressions
	return fmt.Sprintf("let %s = %s;", s.Identifier.String(), "TODO")
}

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

type ReturnStatement struct {
	Token      token.Token
	Expression Expression
}

func (s *ReturnStatement) TokenLiteral() string { return s.Token.Literal }
func (s *ReturnStatement) String() string {
	//todo: expressions
	return fmt.Sprintf("return %s;", "TODO")
}

type ExpressionStatement struct {
	Token      token.Token
	Expression Expression
}

func (s *ExpressionStatement) TokenLiteral() string { return s.Token.Literal }

func (s *ExpressionStatement) String() string {
	//todo: expressions
	return fmt.Sprintf("%s;", "TODO")
}

package parser

import (
	"fmt"
	"go_interpreter/ast"
	"go_interpreter/lexer"
	"go_interpreter/token"
)

type Parser struct {
	l         *lexer.Lexer
	curToken  token.Token
	peekToken token.Token
	errors    []string
}

func New(l *lexer.Lexer) *Parser {
	p := Parser{l: l, errors: []string{}}
	p.nextToken()
	p.nextToken()
	return &p
}

func (p *Parser) nextToken() {
	p.curToken = p.peekToken
	p.peekToken = p.l.NextToken()
}

func (p *Parser) ParseProgram() *ast.Program {
	program := ast.Program{}
	program.Statements = []ast.Statement{}

	for !p.curTokenIs(token.EOF) {
		statement := p.parseStatement()
		if statement != nil {
			program.Statements = append(program.Statements, statement)
		}
		p.nextToken()
	}
	return &program
}

func (p *Parser) curTokenIs(tokenType token.TokenType) bool {
	return p.curToken.Type == tokenType
}
func (p *Parser) peekTokenIs(tokenType token.TokenType) bool {
	return p.peekToken.Type == tokenType
}

func (p *Parser) parseStatement() ast.Statement {
	switch p.curToken.Type {
	case token.LET:
		return p.parseLetStatement()
	default:
		return nil
	}
}

func (p *Parser) addError(format string, a ...any) {
	p.errors = append(p.errors, fmt.Sprintf(format, a...))
}

func (p *Parser) expectPeek(tokenType token.TokenType) bool {
	if p.peekToken.Type == tokenType {
		p.nextToken()
		return true
	} else {
		p.addError("Expect next token to be %s. Found %s", tokenType, p.peekToken.Type)
		return false
	}
}

func (p *Parser) parseLetStatement() ast.Statement {
	letStmt := ast.LetStatement{}
	letStmt.Token = p.curToken

	if !p.expectPeek(token.IDENT) {
		return nil
	}

	letStmt.Identifier = &ast.Identifier{
		Token: p.curToken,
		Value: p.curToken.Literal,
	}

	if !p.expectPeek(token.ASSIGN) {
		return nil
	}

	//TODO: actually support expressions
	for !p.curTokenIs(token.SEMICOLON) {
		p.nextToken()
	}
	return &letStmt
}

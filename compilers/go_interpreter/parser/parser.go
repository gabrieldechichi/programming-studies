package parser

import (
	"fmt"
	"go_interpreter/ast"
	"go_interpreter/lexer"
	"go_interpreter/token"
	"strconv"
)

type (
	prefixParseFn func() ast.Expression
	infixParseFn  func(lhs ast.Expression) ast.Expression
)

// expression types (including operator precedence)
const (
	_ int = iota
	LOWEST
	EQUALS      // ==
	LESSGREATER // > or <
	SUM         // +
	PRODUCT     // *
	PREFIX      // -X or !X
	CALL        // myFunction(X)
)

type Parser struct {
	l         *lexer.Lexer
	curToken  token.Token
	peekToken token.Token
	errors    []string

	prefixParseFunctions map[token.TokenType]prefixParseFn
	infixParseFunctions  map[token.TokenType]infixParseFn
}

func New(l *lexer.Lexer) *Parser {
	p := Parser{l: l, errors: []string{}}
	p.nextToken()
	p.nextToken()

	p.prefixParseFunctions = make(map[token.TokenType]prefixParseFn)
	p.infixParseFunctions = make(map[token.TokenType]infixParseFn)

	p.registerPrefixParseFn(token.IDENT, p.parseExprIdentifier)
    p.registerPrefixParseFn(token.INT, p.parseExprIntegerLiteral)
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

func (p *Parser) registerPrefixParseFn(tokenType token.TokenType, parseFn prefixParseFn) {
	//todo: validate?
	p.prefixParseFunctions[tokenType] = parseFn
}

func (p *Parser) registerInfixParseFn(tokenType token.TokenType, parseFn infixParseFn) {
	//todo: validate?
	p.infixParseFunctions[tokenType] = parseFn
}

func (p *Parser) getPrefixParseFn(tokType token.TokenType) (prefixParseFn, bool) {
	//todo: switch case faster?
	fn, ok := p.prefixParseFunctions[tokType]
	return fn, ok
}
func (p *Parser) getInfixParseFn(tokType token.TokenType) (infixParseFn, bool) {
	//todo: switch case faster?
	fn, ok := p.infixParseFunctions[tokType]
	return fn, ok
}

func (p *Parser) parseStatement() ast.Statement {
	switch p.curToken.Type {
	case token.LET:
		return p.parseLetStatement()
	case token.RETURN:
		return p.parseReturnStatemetn()
	default:
		return p.parseExpressionStatement()
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

func (p *Parser) parseReturnStatemetn() ast.Statement {
	retStm := ast.ReturnStatement{}
	retStm.Token = p.curToken
	//TODO: actually support expressions
	for !p.curTokenIs(token.SEMICOLON) {
		p.nextToken()
	}
	return &retStm
}

func (p *Parser) parseExpressionStatement() ast.Statement {
	exprStmt := ast.ExpressionStatement{}
	exprStmt.Token = p.curToken
	exprStmt.Expression = p.parseExpression(LOWEST)
	//skip semicolon (optional)
	if p.peekTokenIs(token.SEMICOLON) {
		p.nextToken()
	}
	return &exprStmt
}

func (p *Parser) parseExpression(precedence int) ast.Expression {
	_ = precedence
	prefixFn, ok := p.getPrefixParseFn(p.curToken.Type)
	if !ok {
		return nil
	}
	leftExpr := prefixFn()
	return leftExpr
}

// Prefix parsers
func (p *Parser) parseExprIdentifier() ast.Expression {
	return &ast.Identifier{Token: p.curToken, Value: p.curToken.Literal}
}

func (p *Parser) parseExprIntegerLiteral() ast.Expression {
	i := &ast.IntegerLiteral{Token: p.curToken}
    v, err := strconv.ParseInt(i.Token.Literal, 10, 64)
    if err != nil {
        p.addError("Failed to parse int: %s", i.Token.Literal)
        return nil
    }
    i.Value = v
    return i
}

//

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

	//prefix
	p.registerPrefixParseFn(token.IDENT, p.parseExprIdentifier)
	p.registerPrefixParseFn(token.INT, p.parseExprIntegerLiteral)
	p.registerPrefixParseFn(token.LPAREN, p.parseExprGroup)
	p.registerPrefixParseFnMulti([]token.TokenType{token.TRUE, token.FALSE}, p.parseExprBooleanLiteral)
	p.registerPrefixParseFnMulti([]token.TokenType{token.BANG, token.MINUS}, p.parseExprPrefix)
	p.registerPrefixParseFn(token.IF, p.parseExprIf)
	p.registerPrefixParseFn(token.FUNC, p.parseExprFunction)

	//infix
	p.registerInfixParseFnMulti([]token.TokenType{
		token.EQ,
		token.NOT_EQ,
		token.GT, token.LT,
		token.PLUS, token.MINUS,
		token.ASTERISK, token.SLASH,
	}, p.parseExprInfix)
	p.registerInfixParseFn(token.LPAREN, p.parseExprInfixFunctionCall)
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

func (p *Parser) registerPrefixParseFnMulti(tokenTypes []token.TokenType, parseFn prefixParseFn) {
	for _, tokenType := range tokenTypes {
		p.registerPrefixParseFn(tokenType, parseFn)
	}
}

func (p *Parser) registerInfixParseFnMulti(tokenTypes []token.TokenType, parseFn infixParseFn) {
	for _, tokenType := range tokenTypes {
		p.registerInfixParseFn(tokenType, parseFn)
	}
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
	p.nextToken()

	letStmt.Value = p.parseExpression(LOWEST)

	if !p.expectPeek(token.SEMICOLON) {
		return nil
	}
	return &letStmt
}

func (p *Parser) parseReturnStatemetn() ast.Statement {
	retStm := ast.ReturnStatement{}
	retStm.Token = p.curToken
	p.nextToken()
	retStm.Expression = p.parseExpression(LOWEST)

	if !p.expectPeek(token.SEMICOLON) {
		return nil
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
	prefixFn, ok := p.getPrefixParseFn(p.curToken.Type)
	if !ok {
		p.addError("No prefixParseFn for %s", p.curToken.Type)
		return nil
	}
	leftExpr := prefixFn()
	for !p.curTokenIs(token.SEMICOLON) && precedence < p.peekPrecedence() {
		infixFn, ok := p.getInfixParseFn(p.peekToken.Type)
		if !ok {
			//todo: is error correct here?
			p.addError("No infixParseFn for %s", p.peekToken.Type)
			return nil
		}
		p.nextToken()
		leftExpr = infixFn(leftExpr)
	}
	return leftExpr
}

func (p *Parser) currentPrecedence() int { return tokenPrecedence(p.curToken.Type) }

func (p *Parser) peekPrecedence() int { return tokenPrecedence(p.peekToken.Type) }

func tokenPrecedence(tokenType token.TokenType) int {
	switch tokenType {
	case token.EQ, token.NOT_EQ:
		return EQUALS
	case token.GT, token.LT:
		return LESSGREATER
	case token.PLUS, token.MINUS:
		return SUM
	case token.ASTERISK, token.SLASH:
		return PRODUCT
	case token.LPAREN:
		return CALL
	}
	return LOWEST
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

func (p *Parser) parseExprBooleanLiteral() ast.Expression {
	return &ast.BooleanLiteral{Token: p.curToken, Value: p.curTokenIs(token.TRUE)}
}

func (p *Parser) parseExprGroup() ast.Expression {
	p.nextToken()
	e := p.parseExpression(LOWEST)
	if !p.expectPeek(token.RPAREN) {
		return nil
	}
	return e
}

func (p *Parser) parseExprPrefix() ast.Expression {
	e := &ast.PrefixExpression{Token: p.curToken, Operator: p.curToken.Literal}
	if p.curToken.Type != token.BANG && p.curToken.Type != token.MINUS {
		p.addError("Prefix operator can only be ! or -. Found %s", p.curToken.Type)
		return e
	}
	p.nextToken()
	e.Right = p.parseExpression(PREFIX)
	return e
}

func (p *Parser) parseExprIf() ast.Expression {
	e := &ast.IfExpression{Token: p.curToken}
	if !p.expectPeek(token.LPAREN) {
		return nil
	}
	p.nextToken()
	e.Condition = p.parseExpression(LOWEST)
	if !p.expectPeek(token.RPAREN) {
		return nil
	}
	if !p.expectPeek(token.LBRACE) {
		return nil
	}
	e.Consequence = p.parseBlockStatement()

	if p.peekTokenIs(token.ELSE) {
		p.nextToken()
		if !p.expectPeek(token.LBRACE) {
			return nil
		}
		e.Alternative = p.parseBlockStatement()
	}

	return e
}

func (p *Parser) parseBlockStatement() *ast.BlockStatement {
	block := &ast.BlockStatement{Token: p.curToken, Statements: []ast.Statement{}}
	p.nextToken()
	for !p.curTokenIs(token.RBRACE) && !p.curTokenIs(token.EOF) {
		block.Statements = append(block.Statements, p.parseStatement())
		p.nextToken()
	}
	return block
}

func (p *Parser) parseExprFunction() ast.Expression {
	e := &ast.FunctionExpression{Token: p.curToken, Parameters: []*ast.Identifier{}}
	if !p.expectPeek(token.LPAREN) {
		return nil
	}

	if !p.peekTokenIs(token.RPAREN) {
		for !p.curTokenIs(token.RPAREN) && !p.curTokenIs(token.EOF) {
			p.nextToken()
			parameter := &ast.Identifier{Token: p.curToken, Value: p.curToken.Literal}
			e.Parameters = append(e.Parameters, parameter)
			if p.peekTokenIs(token.COMMA) || p.peekTokenIs(token.RPAREN) {
				p.nextToken()
			}
		}
	} else {
		p.nextToken()
	}

	if !p.expectPeek(token.LBRACE) {
		return nil
	}
	e.Body = p.parseBlockStatement()
	return e
}

//

// Infix parsers
func (p *Parser) parseExprInfix(lhs ast.Expression) ast.Expression {
	e := &ast.InfixExpression{Token: p.curToken, Left: lhs, Operator: p.curToken.Literal}
	precedence := p.currentPrecedence()
	p.nextToken()
	e.Right = p.parseExpression(precedence)
	return e
}

func (p *Parser) parseExprInfixFunctionCall(lhs ast.Expression) ast.Expression {
	e := &ast.CallExpression{Token: p.curToken, Function: lhs}
	p.nextToken()
	for p.curToken.Type != token.RPAREN {
		arg := p.parseExpression(LOWEST)
		if arg == nil {
			p.addError("Fail to parse function arguments")
		}
		e.Arguments = append(e.Arguments, arg)
		p.nextToken()
		if p.curToken.Type == token.COMMA {
			p.nextToken()
		}
	}
	return e
}
//

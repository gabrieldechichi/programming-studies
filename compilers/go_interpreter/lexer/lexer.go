package lexer

import (
	"go_interpreter/token"
)

//TODO: support unicode

type Lexer struct {
	input        string
	position     int
	readPosition int
	c            byte
}

func New(input string) *Lexer {
	return &Lexer{
		input: input,
	}
}

func (l *Lexer) readChar() {
	if l.readPosition >= len(l.input) {
		l.c = 0
	} else {
		l.c = l.input[l.readPosition]
	}
	l.position = l.readPosition
	l.readPosition++
}

func (l *Lexer) goBack() {
	if l.position >= 0 {
		l.readPosition = l.position
		l.position--
	}
}

func (l *Lexer) eatWhitespace() {
	for l.c == ' ' || l.c == '\t' || l.c == '\n' || l.c == '\r' {
		l.readChar()
	}
}

func (l *Lexer) readIdentifier() string {
	start := l.position
	for isIdentifier(l.c) {
		l.readChar()
	}
	identifier := string(l.input[start:l.position])
	l.goBack()
	return identifier
}

func (l *Lexer) readDigits() string {
	start := l.position
	for isDigit(l.c) {
		l.readChar()
	}
	digit := string(l.input[start:l.position])
	l.goBack()
	return digit
}

func isIdentifier(c byte) bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
}

func isDigit(c byte) bool {
	return c >= '0' && c <= '9'
}

func (l *Lexer) NextToken() token.Token {
	var tok token.Token
	l.readChar()
	l.eatWhitespace()
	switch l.c {
	case '=':
		tok = newToken(token.ASSIGN, l.c)
	case '+':
		tok = newToken(token.PLUS, l.c)
	case ',':
		tok = newToken(token.COMMA, l.c)
	case ';':
		tok = newToken(token.SEMICOLON, l.c)

	case '(':
		tok = newToken(token.LPAREN, l.c)
	case ')':
		tok = newToken(token.RPAREN, l.c)
	case '{':
		tok = newToken(token.LBRACE, l.c)
	case '}':
		tok = newToken(token.RBRACE, l.c)
	case 0:
		tok.Literal = ""
		tok.Type = token.EOF
	default:
		{
			if isIdentifier(l.c) {
				tok.Literal = l.readIdentifier()
				tok.Type = token.IdentifierToTokenType(tok.Literal)
			} else if isDigit(l.c) {
                //TODO: floats, hex, etc?
				tok.Literal = l.readDigits()
				tok.Type = token.INT
			} else {
				tok.Literal = ""
				tok.Type = token.ILLEGAL
			}
		}
	}
	return tok
}

func newToken(tokenType token.TokenType, ch byte) token.Token {
	return token.Token{Type: tokenType, Literal: string(ch)}
}

package lexer

import "go_interpreter/token"

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

func (l *Lexer) nextToken() token.Token {
	var tok token.Token
	l.readChar()
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
	}
	return tok
}

func newToken(tokenType token.TokenType, ch byte) token.Token {
	return token.Token{Type: tokenType, Literal: string(ch)}
}

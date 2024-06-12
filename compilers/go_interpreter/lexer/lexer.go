package lexer

import (
	"go_interpreter/token"
	"strings"
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

func (l *Lexer) peekChar() byte {
	if l.readPosition >= len(l.input) {
		return 0
	}
	return l.input[l.readPosition]
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

func (l *Lexer) readString() string {
	var strBuilder strings.Builder
	l.readChar()
	for l.c != '"' && l.c != 0 {
		strBuilder.WriteByte(l.c)
		l.readChar()
	}
	return strBuilder.String()
}

func (l *Lexer) readIdentifier() string {
	start := l.position
	for isIdentifierCh(l.c) {
		l.readChar()
	}
	identifier := string(l.input[start:l.position])
	l.goBack()
	return identifier
}

func (l *Lexer) readDigits() string {
	start := l.position
	for isDigitCh(l.c) {
		l.readChar()
	}
	digit := string(l.input[start:l.position])
	l.goBack()
	return digit
}

func isIdentifierCh(c byte) bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
}

func isDigitCh(c byte) bool {
	return c >= '0' && c <= '9'
}

func (l *Lexer) NextToken() token.Token {
	var tok token.Token
	l.readChar()
	l.eatWhitespace()
	switch l.c {
	case '=':
		if l.peekChar() == '=' {
			l.readChar()
			tok = newTokenFromStr(token.EQ, "==")
		} else {
			tok = newTokenFromCh(token.ASSIGN, l.c)
		}
	case '!':
		if l.peekChar() == '=' {
			l.readChar()
			tok = newTokenFromStr(token.NOT_EQ, "!=")
		} else {
			tok = newTokenFromCh(token.BANG, l.c)
		}
	case '+':
		tok = newTokenFromCh(token.PLUS, l.c)
	case '-':
		tok = newTokenFromCh(token.MINUS, l.c)
	case '*':
		tok = newTokenFromCh(token.ASTERISK, l.c)
	case '/':
		tok = newTokenFromCh(token.SLASH, l.c)
	case '<':
		if l.peekChar() == '=' {
			l.readChar()
			tok = newTokenFromStr(token.LTOREQ, "<=")
		} else {
			tok = newTokenFromCh(token.LT, l.c)
		}
	case '>':
		if l.peekChar() == '=' {
			l.readChar()
			tok = newTokenFromStr(token.GTOREQ, ">=")
		} else {
			tok = newTokenFromCh(token.GT, l.c)
		}
	case ',':
		tok = newTokenFromCh(token.COMMA, l.c)
	case ';':
		tok = newTokenFromCh(token.SEMICOLON, l.c)
	case '(':
		tok = newTokenFromCh(token.LPAREN, l.c)
	case ')':
		tok = newTokenFromCh(token.RPAREN, l.c)
	case '{':
		tok = newTokenFromCh(token.LBRACE, l.c)
	case '}':
		tok = newTokenFromCh(token.RBRACE, l.c)
	case '"':
		tok = token.Token{Type: token.STRING}
		tok.Literal = l.readString()
	case 0:
		tok.Literal = ""
		tok.Type = token.EOF
	default:
		{
			if isIdentifierCh(l.c) {
				// tok = newTokenFromStr(token.IdentifierToTokenType(tok.Literal), l.readIdentifier())
				tok.Literal = l.readIdentifier()
				tok.Type = token.IdentifierToTokenType(tok.Literal)
			} else if isDigitCh(l.c) {
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

func newTokenFromCh(tokenType token.TokenType, ch byte) token.Token {
	return token.Token{Type: tokenType, Literal: string(ch)}
}

func newTokenFromStr(tokenType token.TokenType, s string) token.Token {
	return token.Token{Type: tokenType, Literal: s}
}

package lexer

import (
	"go_interpreter/token"
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestToken(t *testing.T) {
	input := `=+(){},;`
	tests := []struct {
		expectedType    token.TokenType
		expectedLiteral string
	}{
		{token.ASSIGN, "="},
		{token.PLUS, "+"},
		{token.LPAREN, "("},
		{token.RPAREN, ")"},
		{token.LBRACE, "{"},
		{token.RBRACE, "}"},
		{token.COMMA, ","},
		{token.SEMICOLON, ";"},
	}

	lexer := New(input)

	for i, tt := range tests {
		token := lexer.nextToken()

		assert.Equal(t, token.Type, tt.expectedType, "Expected tokenType to be equal on test case %d", i)
		assert.Equal(t, token.Literal, tt.expectedLiteral, "Expected tokenType to be equal on test case %d", i)
	}
}

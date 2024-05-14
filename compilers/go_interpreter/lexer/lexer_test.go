package lexer

import (
	"go_interpreter/token"
	"testing"

	"github.com/stretchr/testify/assert"
)

//	func TestToken(t *testing.T) {
//		input := `let five = 5;
//
// let ten = 10;
// let add = fn(x, y) {
// x + y;
// };
// let result = add(five, ten);
// `
//
//		tests := []struct {
//			expectedType    token.TokenType
//			expectedLiteral string
//		}{
//			//let five = 5;
//			{token.LET, "let"},
//			{token.IDENT, "five"},
//			{token.ASSIGN, "="},
//			{token.INT, "5"},
//			{token.SEMICOLON, ";"},
//
//	        //let ten = 10;
//			{token.LET, "let"},
//			{token.IDENT, "ten"},
//			{token.ASSIGN, "="},
//			{token.INT, "10"},
//			{token.SEMICOLON, ";"},
//
//	        // let add = fn(x, y) {
//	        // x + y;
//	        // };
//			{token.LET, "let"},
//			{token.IDENT, "add"},
//			{token.ASSIGN, "="},
//			{token.FUNC, "fn"},
//			{token.LPAREN, "("},
//			{token.IDENT, "x"},
//			{token.COMMA, ","},
//			{token.IDENT, "y"},
//			{token.RPAREN, ")"},
//			{token.LBRACE, "{"},
//			{token.IDENT, "x"},
//			{token.PLUS, "+"},
//			{token.IDENT, "y"},
//			{token.SEMICOLON, ";"},
//			{token.RBRACE, "}"},
//			{token.SEMICOLON, ";"},
//
//	        // let result = add(five, ten);
//			{token.LET, "let"},
//			{token.IDENT, "result"},
//			{token.ASSIGN, "="},
//			{token.FUNC, "add"},
//			{token.LPAREN, "("},
//			{token.IDENT, "five"},
//			{token.COMMA, ","},
//			{token.IDENT, "ten"},
//			{token.RPAREN, ")"},
//			{token.SEMICOLON, ";"},
//			{token.EOF, ""},
//		}
//
//		lexer := New(input)
//
//		for i, tt := range tests {
//			token := lexer.nextToken()
//
//			assert.Equal(t, tt.expectedType, token.Type, "Expected tokenType to be equal on test case %d", i)
//			assert.Equal(t, tt.expectedLiteral, token.Literal, "Expected tokenType to be equal on test case %d", i)
//		}
//	}
func TestToken(t *testing.T) {
	input := `let five = func(x,y);
let five = 4
   `
	tests := []struct {
		expectedType    token.TokenType
		expectedLiteral string
	}{
		//let five = 5;
		{token.LET, "let"},
		{token.IDENT, "five"},
		{token.ASSIGN, "="},
		{token.FUNC, "func"},
		{token.LPAREN, "("},
		{token.IDENT, "x"},
		{token.COMMA, ","},
		{token.IDENT, "y"},
		{token.RPAREN, ")"},
		{token.EOF, ""},
	}

	lexer := New(input)

	for i, tt := range tests {
		token := lexer.NextToken()

		assert.Equal(t, tt.expectedType, token.Type, "Expected tokenType to be equal on test case %d", i)
		assert.Equal(t, tt.expectedLiteral, token.Literal, "Expected tokenType to be equal on test case %d", i)
	}
}

package token

//TODO: add file name, line number, etc

// PERF: change to int or byte enum
type TokenType string

type Token struct {
	Type    TokenType
	Literal string
}

const (
	ILLEGAL = "ILLEGAL"
	EOF     = "EOF"

	IDENT = "IDENT"
	INT   = "INT"

	ASSIGN = "="
	PLUS   = "+"

	COMMA     = ","
	SEMICOLON = ";"

	LPAREN = "("
	RPAREN = ")"
	LBRACE = "{"
	RBRACE = "}"

	FUNC = "FUNC"
	LET  = "LET"
)

func IdentifierToTokenType(str string) TokenType {
	switch str {
	case "let":
		return LET
	case "func":
		return FUNC
	}
	return IDENT
}

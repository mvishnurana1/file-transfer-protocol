#ifndef MY_TOKEN_H
#define MY_TOKEN_H


#define MAXTOKENS 1000

	/**
	*  purpose	breaks up the sequence of chars in line into a
	*		sequence of tokens, storing the start address in
	*		token[] and it replaces the separator chars with
	*		Nulls.
	*  returns 	number of tokens found in line
	*		or -1 if token[] is too small
	*
	*  assumes	*token[] will be MAXTOKENS in length
	*
	*  note		sets trailing token to NULL (token[numTokens] = NULL)
	*/
int tokenise(char line[], char *token[], const char *separators);

#endif //end of MY_TOKEN_H

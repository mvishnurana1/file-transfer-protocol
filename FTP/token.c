#include <stdio.h>
#include <string.h>
#include "token.h"

//returns the length of a string
int length(const char *str)
{
	int i = 0;

	while(str[i] != '\0')		//while not end of string
	{				//check next char for end of string
		i++;
	}
	return(i);			//return number of characters to end of string
}

//searches a string for a particular character
int inlist(const char target, const char *list)
{
	int listlength;
	int loop;

	listlength = length(list);

	for (loop = 0; loop < listlength; loop++)	//check each character in list
	{
		if(target == list[loop])		//to see if the target matches one
		{
			return(1);			//if it does return true
		}
	}
	return (0);					//else return false
}

//clears all consecutive characters in a list from a string, returns the number cleared
int clrChLst(char *ch, const char *list)
{
	int i = 0;
	//set ch to null if it is in list and not null
	while ((inlist(*(ch + i), list)) && (*(ch + i) != '\0'))
	{
		*(ch + i) = '\0';
		i++;
	}
	return(i);
}

/*
	finds instances of the characters listed in separators in the string line,
	places a pointer to the character after in the token array, removes the
	seperator from the line to form a token.
	returns the number of tokens found.
*/

int tokenise (char line[], char *token[], const char separators[])
{
	int loop;				//for loops
	int tk = 0;					//return number of tokens
	int linelength;				//record length of line
	int makeToken;				//boolean to make next character start of next token

	linelength = length(line);
	makeToken = 1;				//prime first token
	loop = clrChLst(line, separators);	//clear leading seperators from line

	for(; loop < linelength; loop++)
	{
		if (makeToken)			//if make token flag on
		{
			makeToken = 0;			//remove make token flag
			token[tk] = line + loop;	//mark token
			tk ++;				//increment number of tokens
			if (tk > MAXTOKENS -1)		//check array bounds allowing for trailing NULL token
			{
				return(-1);		//return error if max tokens exceeded
			}
		}
		else				//else check next character in line
		{
			if (makeToken = clrChLst(line + loop, separators))	//set flag and clear separators
			{
				loop += makeToken -1;		//advance loops by number of extra separators found.
			}
		}
	}
	token[tk] = NULL;			//add trailing null token
	return (tk);				//return number of tokens found.
}


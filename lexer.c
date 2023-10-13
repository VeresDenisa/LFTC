#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "lexer.h"
#include "utils.h"

Token *tokens;	// single linked list of tokens
Token *lastTk;  // the last token in list

int line=1;		// the current line in the input file

// adds a token to the end of the tokens list and returns it
// sets its code and line
Token *addTk(int code){
	Token *tk = safeAlloc(sizeof(Token));
	tk->code = code;
	tk->line = line;
	tk->next = NULL;
	if(lastTk){
        lastTk->next = tk;
    }else{
        tokens = tk;
    }
	lastTk = tk;
	return tk;
	}

char *extract(const char *begin, const char *end){
    char *p;
    char *buf = safeAlloc((end-begin)*sizeof(char));

    int i = 0;
    for(p = (char *)begin; p!=end; p++){ buf[i] = p[0]; i++; }
    buf[i] = '\0';
    return buf;
}

Token *tokenize(const char *pch){
	const char *start;
	Token *tk;
	for(;;){
		switch(*pch){
			case ' ': pch++; break;
			case '\t': pch++; break;
			case '\r': if(pch[1]=='\n') pch++;
			case '\n': line++; pch++; break;
			case '\0': addTk(END); return tokens;
			case '{': addTk(LACC); pch++; break;
			case '}': addTk(RACC); pch++; break;
			case '(': addTk(LPAR); pch++; break;
			case ')': addTk(RPAR); pch++; break;
			case '[': addTk(LBRACKET); pch++; break;
			case ']': addTk(RBRACKET); pch++; break;
			case ';': addTk(SEMICOLON); pch++; break;
			case ',': addTk(COMMA); pch++; break;
			case '.': addTk(DOT); pch++; break;
            case '+': addTk(ADD); pch++; break;
            case '-': addTk(SUB); pch++; break;
            case '*': addTk(MUL); pch++; break;

            case '/':
                if(pch[1]=='/'){
                    for(; *pch!='\n' && *pch!='\r' && *pch!='\0'; pch++)
                        if(pch[0]=='\\' && (pch[1]=='n' || pch[1]=='r' || pch[1]=='0'))
                            err("Invalid comment line %d", line);
                }else{ addTk(DIV); pch++; }
                break;

			case '=':
				if(pch[1]=='='){ addTk(EQUAL); pch+=2; }
                else{ addTk(ASSIGN); pch++; }
                break;
			case '&':
				if(pch[1]=='&'){ addTk(AND); pch+=2; }
                else err("invalid AND: %c (%d)",*pch,*pch);
                break;
			case '|':
				if(pch[1]=='|'){ addTk(OR); pch+=2; }
                else err("invalid OR: %c (%d)",*pch,*pch);
                break;
			case '!':
				if(pch[1]=='='){ addTk(NOTEQ); pch+=2; }
				else{ addTk(NOT); pch++; }
                break;
			case '<':
				if(pch[1]=='='){ addTk(LESSEQ); pch+=2; }
				else{ addTk(LESS); pch++; }
                break;
			case '>':
				if(pch[1]=='='){ addTk(GREATEREQ); pch+=2; }
                else{ addTk(GREATER); pch++; }
                break;

			default:
                if(*pch=='\''){
                    if(pch[1]!='\'' && pch[2]=='\''){ tk = addTk(CHAR); tk->c = pch[1]; pch+=3; }
                    else err("invalid char: %c (%d)",*pch,*pch);
                }else if(*pch=='"'){
                    for(start = pch++; *pch!='\"' && *pch!='\0'; pch++){}
                    if(pch[0]=='\0') err("invalid string line %d", line);
                    else{
                        char *text = extract(start + 1, pch);
                        tk = addTk(STRING); tk->text = text; pch++;
                    }
                }else if(isalpha(*pch) || *pch=='_'){
					for(start = pch++; isalnum(*pch) || *pch=='_'; pch++){}
					char *text = extract(start, pch);
					if(strcmp(text, "char")==0) addTk(TYPE_CHAR);
                    else if(strcmp(text, "int")==0) addTk(TYPE_INT);
                    else if(strcmp(text, "double")==0) addTk(TYPE_DOUBLE);
                    else if(strcmp(text, "if")==0) addTk(IF);
                    else if(strcmp(text, "else")==0) addTk(ELSE);
                    else if(strcmp(text, "struct")==0) addTk(STRUCT);
                    else if(strcmp(text, "void")==0) addTk(VOID);
                    else if(strcmp(text, "while")==0) addTk(WHILE);
                    else if(strcmp(text, "return")==0) addTk(RETURN);
                    else{ tk = addTk(ID); tk->text = text; }
                }else if(isdigit(*pch)){
                    const char *begin = pch;
                    char *text;
                    for(start = pch++; isdigit(*pch); pch++){}
                    if(pch[0]=='.'){
                        if(isdigit(pch[1])){
                            for(start = pch++; isdigit(*pch); pch++){}
                            if(pch[0]=='e' || pch[0]=='E'){
                                if(pch[1]=='+' || pch[1]=='-') pch++;
                                pch++;
                                for(start = pch++; isdigit(*pch); pch++){}
                            }
                            text = extract(begin, pch);
                            tk = addTk(DOUBLE); tk->d = atof(text);
                        }else err("invalid double: %f (%d)",*pch,*pch);
                    }else if(pch[0]=='e' || pch[0]=='E'){
                        if(pch[1]=='+' || pch[1]=='-') pch++;
                        pch++;
                        for(; isdigit(*pch); pch++){}
                        text = extract(begin, pch);
                        tk = addTk(DOUBLE); tk->d = atof(text);
                    }else{
                        text = extract(begin, pch);
                        tk = addTk(INT); tk->i = atoi(text);
                    }
                }else err("invalid: %s (%d)",*pch,*pch);
			}
		}
	}

void showTokens(const Token *tokens){
    char *name[] = {"ID","TYPE_CHAR","TYPE_INT","TYPE_DOUBLE","IF","ELSE","VOID","STRUCT","WHILE","RETURN",
                    "INT","CHAR","DOUBLE","STRING","LACC","RACC","LPAR","RPAR","LBRACKET","RBRACKET",
                    "COMMA","SEMICOLON","END","ASSIGN","EQUAL","ADD","SUB","MUL","DIV","DOT","AND","OR",
                    "NOT","NOTEQ","LESS","LESSEQ","GREATER","GREATEREQ"};
	for(const Token *tk = tokens; tk; tk = tk->next){
		printf("%d  %s", tk->line, name[tk->code]);
		if(strcmp(name[tk->code], "ID")==0 || strcmp(name[tk->code], "STRING")==0) printf(":%s\n", tk->text);
        else if(strcmp(name[tk->code], "CHAR")==0) printf(":%c\n", tk->c);
        else if(strcmp(name[tk->code], "INT")==0) printf(":%d\n", tk->i);
        else if(strcmp(name[tk->code], "DOUBLE")==0) printf(":%g\n", tk->d);
        else printf("\n");
    }
}

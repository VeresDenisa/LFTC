#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>

#include "parser.h"
#include "utils.h"
#include "vm.h"
#include "ad.h"
#include "at.h"
#include "gc.h"

Token *iTk;		        // the iterator in the tokens list
Token *consumedTk;		// the last consumed token
Symbol *owner;

bool unit();            // unit:        ( structDef | fnDef | varDef )* END
bool structDef();       // structDef:   STRUCT ID LACC varDef* RACC SEMICOLON
bool varDef();          // varDef:      typeBase ID arrayDecl? SEMICOLON
bool typeBase(Type *t);   // typeBase:    TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
bool arrayDecl(Type *t);  // arrayDecl:   LBRACKET INT? RBRACKET
bool fnDef();           // fnDef:       ( typeBase | VOID ) ID LPAR ( fnParam ( COMMA fnParam )* )? RPAR stmCompound
bool fnParam();         // fnParam:     typeBase ID arrayDecl?
bool stm();             // stm:         stmCompound | IF LPAR expr RPAR stm ( ELSE stm )? | WHILE LPAR expr RPAR stm | RETURN expr? SEMICOLON | expr? SEMICOLON
bool stmCompound(bool newDomain); // stmCompound: LACC ( varDef | stm )* RACC
bool expr(Ret *r);            // expr:        exprAssign
bool exprAssign(Ret *r);      // exprAssign:  ( ID ASSIGN exprAssign ) | exprOr | ( exprUnary ASSIGN exprAssign )
bool exprOr(Ret *r);          // exprOr:      exprOr OR exprAnd | exprAnd                                           => exprOr:      exprAnd exprOrPrim
bool exprAnd(Ret *r);         // exprAnd:     exprAnd AND exprEq | exprEq                                           => exprAnd:     exprEq exprAndPrim
bool exprEq(Ret *r);          // exprEq:      exprEq ( EQUAL | NOTEQ ) exprRel | exprRel                            => exprEq:      exprRel exprEqPrim
bool exprRel(Ret *r);         // exprRel:     exprRel ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd | exprAdd     => exprRel:     exprAdd exprRelPrim
bool exprAdd(Ret *r);         // exprAdd:     exprAdd ( ADD | SUB ) exprMul | exprMul                               => exprAdd:     exprMul exprAddPrim
bool exprMul(Ret *r);         // exprMul:     exprMul ( MUL | DIV ) exprCast | exprCast                             => exprMul:     exprCast exprMulPrim
bool exprCast(Ret *r);        // exprCast:    LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
bool exprUnary(Ret *r);       // exprUnary:   ( SUB | NOT ) exprUnary | exprPostfix
bool exprPostfix(Ret *r);     // exprPostfix: exprPostfix LBRACKET expr RBRACKET | exprPostfix DOT ID | exprPrimary => exprPostfix: exprPrimary exprPostfixPrim
bool exprPrimary(Ret *r);     // exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )? | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR

bool exprOrPrim(Ret *r);      // exprOrPrim:      OR exprAnd exprOrPrim | e
bool exprAndPrim(Ret *r);     // exprAndPrim:     AND exprEq exprAndPrim | e
bool exprEqPrim(Ret *r);      // exprEqPrim:      ( EQUAL | NOTEQ ) exprRel exprEqPrim | e
bool exprRelPrim(Ret *r);     // exprRelPrim:     ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd exprRelPrim | e
bool exprAddPrim(Ret *r);     // exprAddPrim:     ( ADD | SUB ) exprMul exprAddPrim | e
bool exprMulPrim(Ret *r);     // exprMulPrim:     ( MUL | DIV ) exprCast exprMulPrxprUnary ASSIGN exprAssign | exprOrim | e
bool exprPostfixPrim(Ret *r); // exprPostfixPrim: LBRACKET expr RBRACKET exprPostfixPrim | DOT ID exprPostfixPrim | e

const char *tkCodeName(int code){
    char *name[] = {"ID","TYPE_CHAR","TYPE_INT","TYPE_DOUBLE","IF","ELSE","VOID","STRUCT","WHILE","RETURN",
                    "INT","CHAR","DOUBLE","STRING","LACC","RACC","LPAR","RPAR","LBRACKET","RBRACKET",
                    "COMMA","SEMICOLON","END","ASSIGN","EQUAL","ADD","SUB","MUL","DIV","DOT","AND","OR",
                    "NOT","NOTEQ","LESS","LESSEQ","GREATER","GREATEREQ"};
    return name[code];
}

void tkerr(const char *fmt,...){
	fprintf(stderr,"error in line %d: ",iTk->line);
	va_list va;
	va_start(va,fmt);
	vfprintf(stderr,fmt,va);
	va_end(va);
	fprintf(stderr,"\n");
	exit(EXIT_FAILURE);
}

bool consume(int code){
    printf(".consume(%s)",tkCodeName(iTk->code));
	if(iTk->code==code){
		consumedTk=iTk;
		iTk=iTk->next;
		printf(" => consumed %s\n",tkCodeName(consumedTk->code));
		return true;
		}
    printf(" => found %s\n",tkCodeName(iTk->code));
	return false;
}


// unit: ( structDef | fnDef | varDef )* END
bool unit(){
	for(;;){
		if(structDef()){}
		else if(fnDef()){}
		else if(varDef()){}
		else break;
    }
	if(consume(END)){
		return true;
		}else tkerr("Eroare de sintaxa!");
	return false;
}

// structDef: STRUCT ID LACC varDef* RACC SEMICOLON
bool structDef(){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(STRUCT)){
        if(consume(ID)){
            Token *tkName = consumedTk;
            if(consume(LACC)){
                Symbol *s = findSymbolInDomain(symTable, tkName->text);
				if(s) tkerr("symbol redefinition: %s", tkName->text);
				s = addSymbolToDomain(symTable, newSymbol(tkName->text, SK_STRUCT));
				s->type.tb = TB_STRUCT;
				s->type.s = s;
				s->type.n = -1;
				pushDomain();
				owner = s;
                for(;;){
                    if(varDef()){}
                    else break;
                }
                if(consume(RACC)){
                    if(consume(SEMICOLON)){
						owner = NULL;
						dropDomain();
                        return true;
                    }else tkerr("missing ; after }");
                }else tkerr("missing } after structure declaration");
            }
        }else tkerr("missing structure name");
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// varDef: typeBase ID arrayDecl? SEMICOLON
bool varDef(){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    Type t;
    if(typeBase(&t)){
        if(consume(ID)){
            Token *tkName = consumedTk;
            if(arrayDecl(&t)){
                if(t.n==0) tkerr("dimensiunea vectorului nu este specificata");
            }
            if(consume(SEMICOLON)){
                Symbol *var = findSymbolInDomain(symTable, tkName->text);
				if(var) tkerr("symbol redefinition: %s", tkName->text);
				var = newSymbol(tkName->text, SK_VAR);
				var->type = t;
				var->owner = owner;
				addSymbolToDomain(symTable, var);
				if(owner){
					switch(owner->kind)
					{
						case SK_FN:
                            var->varIdx = symbolsLen(owner->fn.locals);
                            addSymbolToList(&owner->fn.locals, dupSymbol(var));
                            break;
						case SK_STRUCT:
                            var->varIdx = typeSize(&owner->type);
                            addSymbolToList(&owner->structMembers, dupSymbol(var));
                            break;
						default: break;
					}
				}else{
					var->varMem = safeAlloc(typeSize(&t));
				}
                return true;
            }else tkerr("missing ; after variable declaration");
        }else tkerr("missing variable declaration name");
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// typeBase: TYPE_INT | TYPE_DOUBLE | TYPE_CHAR | STRUCT ID
bool typeBase(Type *t){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    t->n = -1;
	if(consume(TYPE_INT)){
        t->tb=TB_INT;
		return true;
		}
	if(consume(TYPE_DOUBLE)){
	    t->tb=TB_DOUBLE;
		return true;
		}
	if(consume(TYPE_CHAR)){
	    t->tb=TB_CHAR;
		return true;
		}
	if(consume(STRUCT)){
		if(consume(ID)){
		    Token *tkName = consumedTk;
			t->tb = TB_STRUCT;
			t->s = findSymbol(tkName->text);
			if(!t->s) tkerr("undefined structure: %s", tkName->text);
			return true;
			}
		}
    iTk = start;
    if(owner)delInstrAfter(startInstr);
	return false;
}

// arrayDecl: LBRACKET INT? RBRACKET
bool arrayDecl(Type *t){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(LBRACKET)){
        if(consume(INT)){
            Token *tkSize = consumedTk;
            t->n = tkSize->i;
        }else{
            t->n = 0;
        }
        if(consume(RBRACKET)){
            return true;
        }else tkerr("missing ] or invalid expression inside [...]");
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// fnDef: ( typeBase | VOID ) ID LPAR ( fnParam ( COMMA fnParam )* )? RPAR stmCompound
bool fnDef(){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    Type t;
    if(typeBase(&t)){
        if(consume(ID)){
            Token *tkName = consumedTk;
            if(consume(LPAR)){
                Symbol *fn = findSymbolInDomain(symTable, tkName->text);
				if(fn) tkerr("symbol redefinition: %s", tkName->text);
				fn = newSymbol(tkName->text, SK_FN);
				fn->type = t;
				addSymbolToDomain(symTable, fn);
				owner = fn;
				pushDomain();
                if(fnParam()){
                    for(;;){
                        if(consume(COMMA)){
                            if(fnParam()){}
                            else tkerr("missing parameter after ,");
                        }
                        else break;
                    }
                }
                if(consume(RPAR)){
                    addInstr(&fn->fn.instr,OP_ENTER);
                    if(stmCompound(false)){
                        fn->fn.instr->arg.i=symbolsLen(fn->fn.locals);
                        if(fn->type.tb==TB_VOID)
                            addInstrWithInt(&fn->fn.instr,OP_RET_VOID,symbolsLen(fn->fn.params));
                        dropDomain();
                        owner = NULL;
                        return true;
                    }else tkerr("missing compound");
                }else tkerr("missing ) after function parameters");
            }
        }else tkerr("missing function name");
    }else if(consume(VOID)){
        t.tb=TB_VOID;
        if(consume(ID)){
            Token *tkName = consumedTk;
            if(consume(LPAR)){
                Symbol *fn = findSymbolInDomain(symTable, tkName->text);
				if(fn) tkerr("symbol redefinition: %s", tkName->text);
				fn = newSymbol(tkName->text, SK_FN);
				fn->type = t;
				addSymbolToDomain(symTable, fn);
				owner = fn;
				pushDomain();
                if(fnParam()){
                    for(;;){
                        if(consume(COMMA)){
                            if(fnParam()){}
                            else tkerr("missing parameter after ,");
                        }
                        else break;
                    }
                }
                if(consume(RPAR)){
                    addInstr(&fn->fn.instr,OP_ENTER);
                    if(stmCompound(false)){
                        fn->fn.instr->arg.i=symbolsLen(fn->fn.locals);
                        if(fn->type.tb==TB_VOID)
                            addInstrWithInt(&fn->fn.instr,OP_RET_VOID,symbolsLen(fn->fn.params));
                        dropDomain();
                        owner = NULL;
                        return true;
                    }else tkerr("missing compound");
                }else tkerr("missing ) after function parameters");
            }
        }else tkerr("missing function name");
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// fnParam: typeBase ID arrayDecl?
bool fnParam(){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    Type t;
    if(typeBase(&t)){
        if(consume(ID)){
            Token *tkName = consumedTk;
            if(arrayDecl(&t)){ t.n=0; }
            Symbol *param = findSymbolInDomain(symTable, tkName->text);
			if(param) tkerr("symbol redefinition: %s", tkName->text);
			param = newSymbol(tkName->text, SK_PARAM);
			param->type = t;
			param->owner = owner;
			param->paramIdx = symbolsLen(owner->fn.params);
			addSymbolToDomain(symTable, param);
			addSymbolToList(&owner->fn.params, dupSymbol(param));
            return true;
        }else tkerr("missing variable declaration name");
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// stm: stmCompound | IF LPAR expr RPAR stm ( ELSE stm )? | WHILE LPAR expr RPAR stm | RETURN expr? SEMICOLON | expr? SEMICOLON
bool stm(){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    Ret rCond, rExpr;
    if(stmCompound(true)){
        return true;
    }else if(consume(IF)){
        if(consume(LPAR)){
            if(expr(&rCond)){
                if(!canBeScalar(&rCond)) tkerr("the if condition must be a scalar value");
                if(consume(RPAR)){
                    addRVal(&owner->fn.instr,rCond.lval,&rCond.type);
                    Type intType={TB_INT,NULL,-1};
                    insertConvIfNeeded(lastInstr(owner->fn.instr),&rCond.type,&intType);
                    Instr *ifJF=addInstr(&owner->fn.instr,OP_JF);
                    if(stm()){
                        if(consume(ELSE)){
                            Instr *ifJMP=addInstr(&owner->fn.instr,OP_JMP);
                            ifJF->arg.instr=addInstr(&owner->fn.instr,OP_NOP);
                            if(stm()){
                                ifJMP->arg.instr=addInstr(&owner->fn.instr,OP_NOP);
                            }
                            else tkerr("wrong else branch content");
                        }
                        ifJF->arg.instr=addInstr(&owner->fn.instr,OP_NOP);
                        return true;
                    }else tkerr("wrong if branch content");
                }else tkerr("missing ) after if expression");
            }else tkerr("missing expression");
        }else tkerr("missing ( before if expression");
    }else if(consume(WHILE)){
        Instr *beforeWhileCond = lastInstr(owner->fn.instr);
        if(consume(LPAR)){
            if(expr(&rCond)){
                if(!canBeScalar(&rCond)) tkerr("the while condition must be a scalar value");
                if(consume(RPAR)){
                    addRVal(&owner->fn.instr,rCond.lval,&rCond.type);
                    Type intType={TB_INT,NULL,-1};
                    insertConvIfNeeded(lastInstr(owner->fn.instr),&rCond.type,&intType);
                    Instr *whileJF=addInstr(&owner->fn.instr,OP_JF);
                    if(stm()){
                        addInstr(&owner->fn.instr,OP_JMP)->arg.instr=beforeWhileCond->next;
                        whileJF->arg.instr=addInstr(&owner->fn.instr,OP_NOP);
                        return true;
                    }else tkerr("wrong while content");
                }else tkerr("missing ) after while expression");
            }
        }else tkerr("missing ( before while expression");
    }else if(consume(RETURN)){
        if(expr(&rExpr)){
            addRVal(&owner->fn.instr,rExpr.lval,&rExpr.type);
            insertConvIfNeeded(lastInstr(owner->fn.instr),&rExpr.type,&owner->type);
            addInstrWithInt(&owner->fn.instr,OP_RET,symbolsLen(owner->fn.params));
            if(owner->type.tb == TB_VOID) tkerr("a void function cannot return a value");
            if(!canBeScalar(&rExpr)) tkerr("the return value must be a scalar value");
            if(!convTo(&rExpr.type, &owner->type)) tkerr("cannot convert the return expression type to the function return type");
        }else{
            addInstr(&owner->fn.instr,OP_RET_VOID);
            if(owner->type.tb != TB_VOID) tkerr("a non-void function must return a value");
        }
        if(consume(SEMICOLON)){
            return true;
        }else tkerr("missing ; after return");
    }else if(expr(&rExpr)){
        if(consume(SEMICOLON)){
            return true;
        } else tkerr("missing ; after expression");
    }else {
        if (expr(&rExpr)) {
            if (rExpr.type.tb != TB_VOID)
                addInstr(&owner->fn.instr, OP_DROP);
        }
        if(consume(SEMICOLON)){
            return true;
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// stmCompound: LACC ( varDef | stm )* RACC
bool stmCompound(bool newDomain){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(LACC)){
        if(newDomain)
			pushDomain();
        for(;;){
            if(varDef()){}
            else if(stm()){}
            else break;
        }
        if(consume(RACC)){
            if(newDomain)
				dropDomain();
            return true;
        }else tkerr("missing } at end");
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// expr: exprAssign
bool expr(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(exprAssign(r)){
        return true;
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// exprAssign: ( ID ASSIGN exprAssign ) | exprOr | ( exprUnary ASSIGN exprAssign )
bool exprAssign(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    Ret rDst;
    if(exprUnary(&rDst)){
        if(consume(ASSIGN)){
            if(exprAssign(r)){
                addRVal(&owner->fn.instr,r->lval,&r->type);
                insertConvIfNeeded(lastInstr(owner->fn.instr),&r->type,&rDst.type);
                switch(rDst.type.tb){
                    case TB_INT:addInstr(&owner->fn.instr,OP_STORE_I);break;
                    case TB_DOUBLE:addInstr(&owner->fn.instr,OP_STORE_F);break;
                }
                if(!rDst.lval) tkerr("the assign destination must be a left-value");
                if(rDst.ct) tkerr("the assign destination cannot be constant");
                if(!canBeScalar(&rDst)) tkerr("the assign destination must be scalar");
                if(!canBeScalar(r)) tkerr("the assign source must be scalar");
                if(!convTo(&r->type, &rDst.type)) tkerr("the assign source cannot be converted to destination");
                r->lval = false;
                r->ct = true;
                return true;
            }else tkerr( "Missing member after =");
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    if(exprOr(r)){
        return true;
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// exprOr: exprAnd exprOrPrim
bool exprOr(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(exprAnd(r)){
        if(exprOrPrim(r)){
            return true;
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// exprOrPrim: OR exprAnd exprOrPrim | e
bool exprOrPrim(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(OR)){
        Ret right;
        if(exprAnd(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for ||");
            *r = (Ret){{TB_INT, NULL, -1}, false, true};
            if(exprOrPrim(r)){
                return true;
            }
        }else tkerr("invalid expression after ||");
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return true;
}


// exprAnd: exprEq exprAndPrim
bool exprAnd(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(exprEq(r)){
        if(exprAndPrim(r)){
            return true;
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// exprAndPrim: AND exprEq exprAndPrim | e
bool exprAndPrim(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(AND)){
        Ret right;
        if(exprEq(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for &&");
            *r = (Ret){{TB_INT, NULL, -1}, false, true};
            if(exprAndPrim(r)){
                return true;
            }
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return true;
}


// exprEq: exprRel exprEqPrim
bool exprEq(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(exprRel(r)){
        if(exprEqPrim(r)){
            return true;
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// exprEqPrim: ( EQUAL | NOTEQ ) exprRel exprEqPrim | e
bool exprEqPrim(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(EQUAL)){
        Ret right;
        if(exprRel(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for == or!=");
            *r=(Ret){{TB_INT, NULL, -1}, false, true};
            if(exprEqPrim(r)){
                return true;
            }
        }else tkerr("invalid expression after ==");
    }
    if(consume(NOTEQ)){
        Ret right;
        if(exprRel(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for == or!=");
            *r=(Ret){{TB_INT, NULL, -1}, false, true};
            if(exprEqPrim(r)){
                return true;
            }
        }else tkerr("invalid expression after !=");
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return true;
}


// exprRel: exprAdd exprRelPrim
bool exprRel(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(exprAdd(r)){
        if(exprRelPrim(r)){
            return true;
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// exprRelPrim: ( LESS | LESSEQ | GREATER | GREATEREQ ) exprAdd exprRelPrim | e
bool exprRelPrim(Ret* r){
    Token *op;
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(LESS)){
        op = consumedTk;
        Ret right;
        Instr *lastLeft=lastInstr(owner->fn.instr);
        addRVal(&owner->fn.instr,r->lval,&r->type);
        if(exprAdd(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for <, <=, >,>=");
            addRVal(&owner->fn.instr,right.lval,&right.type);
            insertConvIfNeeded(lastLeft,&r->type,&tDst);
            insertConvIfNeeded(lastInstr(owner->fn.instr),&right.type,&tDst);
            switch(op->code){
                case LESS:
                    switch(tDst.tb){
                        case TB_INT:addInstr(&owner->fn.instr,OP_LESS_I);break;
                        case TB_DOUBLE:addInstr(&owner->fn.instr,OP_LESS_F);break;
                    }
                    break;
            }
            *r = (Ret){{TB_INT, NULL, -1}, false, true};
            if(exprRelPrim(r)){
                return true;
            }
        }else tkerr("missing variable after <");
    }
    if(consume(LESSEQ)){
        op = consumedTk;
        Ret right;
        Instr *lastLeft=lastInstr(owner->fn.instr);
        addRVal(&owner->fn.instr,r->lval,&r->type);
        if(exprAdd(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for <, <=, >,>=");
            addRVal(&owner->fn.instr, right.lval, &right.type);
            insertConvIfNeeded(lastLeft, &r->type, &tDst);
            insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
            switch (op->code) {
                case LESS:
                    switch (tDst.tb) {
                        case TB_INT:
                            addInstr(&owner->fn.instr, OP_LESS_I);
                            break;
                        case TB_DOUBLE:
                            addInstr(&owner->fn.instr, OP_LESS_F);
                            break;
                    }
                    break;
            }
            *r = (Ret){{TB_INT, NULL, -1}, false, true};
            if(exprRelPrim(r)){
                return true;
            }
        }else tkerr("missing variable after <=");
    }
    if(consume(GREATER)){
        op = consumedTk;
        Ret right;
        Instr *lastLeft=lastInstr(owner->fn.instr);
        addRVal(&owner->fn.instr,r->lval,&r->type);
        if(exprAdd(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for <, <=, >,>=");
            addRVal(&owner->fn.instr, right.lval, &right.type);
            insertConvIfNeeded(lastLeft, &r->type, &tDst);
            insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
            switch (op->code) {
                case LESS:
                    switch (tDst.tb) {
                        case TB_INT:
                            addInstr(&owner->fn.instr, OP_LESS_I);
                            break;
                        case TB_DOUBLE:
                            addInstr(&owner->fn.instr, OP_LESS_F);
                            break;
                    }
                    break;
            }
            *r = (Ret){{TB_INT, NULL, -1}, false, true};
            if(exprRelPrim(r)){
                return true;
            }
        }else tkerr("missing variable after >");
    }
    if(consume(GREATEREQ)){
        op = consumedTk;
        Ret right;
        Instr *lastLeft=lastInstr(owner->fn.instr);
        addRVal(&owner->fn.instr,r->lval,&r->type);
        if(exprAdd(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for <, <=, >,>=");
            addRVal(&owner->fn.instr, right.lval, &right.type);
            insertConvIfNeeded(lastLeft, &r->type, &tDst);
            insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
            switch (op->code) {
                case LESS:
                    switch (tDst.tb) {
                        case TB_INT:
                            addInstr(&owner->fn.instr, OP_LESS_I);
                            break;
                        case TB_DOUBLE:
                            addInstr(&owner->fn.instr, OP_LESS_F);
                            break;
                    }
                    break;
            }
            *r = (Ret){{TB_INT, NULL, -1}, false, true};
            if(exprRelPrim(r)){
                return true;
            }
        }else tkerr("missing variable after >=");
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return true;
}


// exprAdd: exprMul exprAddPrim
bool exprAdd(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(exprMul(r)){
        if(exprAddPrim(r)){
            return true;
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// exprAddPrim: ( ADD | SUB ) exprMul exprAddPrim | e
bool exprAddPrim(Ret* r){
    Token *op;
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(ADD)){
        Ret right;
        op = consumedTk;
        Instr *lastLeft=lastInstr(owner->fn.instr);
        addRVal(&owner->fn.instr,r->lval,&r->type);
        if(exprMul(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for + or -");
            addRVal(&owner->fn.instr,right.lval,&right.type);
            insertConvIfNeeded(lastLeft,&r->type,&tDst);
            insertConvIfNeeded(lastInstr(owner->fn.instr),&right.type,&tDst);
            switch(op->code){
                case ADD:
                    switch(tDst.tb){
                        case TB_INT:addInstr(&owner->fn.instr,OP_ADD_I);break;
                        case TB_DOUBLE:addInstr(&owner->fn.instr,OP_ADD_F);break;
                    }
                    break;
                case SUB:
                    switch(tDst.tb){
                        case TB_INT:addInstr(&owner->fn.instr,OP_SUB_I);break;
                        case TB_DOUBLE:addInstr(&owner->fn.instr,OP_SUB_F);break;
                    }
                    break;
            }
            *r = (Ret){tDst, false, true};
            if(exprAddPrim(r)){
                return true;
            }
        }else tkerr("missing variable after +");
    }
    if(consume(SUB)){
        Ret right;
        op = consumedTk;
        Instr *lastLeft=lastInstr(owner->fn.instr);
        addRVal(&owner->fn.instr,r->lval,&r->type);
        if(exprMul(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for + or -");
            addRVal(&owner->fn.instr,right.lval,&right.type);
            insertConvIfNeeded(lastLeft,&r->type,&tDst);
            insertConvIfNeeded(lastInstr(owner->fn.instr),&right.type,&tDst);
            switch(op->code){
                case ADD:
                    switch(tDst.tb){
                        case TB_INT:addInstr(&owner->fn.instr,OP_ADD_I);break;
                        case TB_DOUBLE:addInstr(&owner->fn.instr,OP_ADD_F);break;
                    }
                    break;
                case SUB:
                    switch(tDst.tb){
                        case TB_INT:addInstr(&owner->fn.instr,OP_SUB_I);break;
                        case TB_DOUBLE:addInstr(&owner->fn.instr,OP_SUB_F);break;
                    }
                    break;
            }
            *r = (Ret){tDst, false, true};
            if(exprAddPrim(r)){
                return true;
            }
        }else tkerr("missing variable after -");
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return true;
}


// exprMul: exprCast exprMulPrim
bool exprMul(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(exprCast(r)){
        if(exprMulPrim(r)){
            return true;
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// exprMulPrim: ( MUL | DIV ) exprCast exprMulPrim | e
bool exprMulPrim(Ret* r){
    Token *op;
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(MUL)){
        Ret right;
        op = consumedTk;
        Instr *lastLeft = lastInstr(owner->fn.instr);
        addRVal(&owner->fn.instr, r->lval, &r->type);
        if(exprCast(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for * or /");
            addRVal(&owner->fn.instr,right.lval,&right.type);
            insertConvIfNeeded(lastLeft,&r->type,&tDst);
            insertConvIfNeeded(lastInstr(owner->fn.instr),&right.type,&tDst);
            switch(op->code) {
                case MUL:
                    switch (tDst.tb) {
                        case TB_INT:
                            addInstr(&owner->fn.instr, OP_MUL_I);
                            break;
                        case TB_DOUBLE:
                            addInstr(&owner->fn.instr, OP_MUL_F);
                            break;
                    }
                    break;
                case DIV:
                    switch (tDst.tb) {
                        case TB_INT:
                            addInstr(&owner->fn.instr, OP_DIV_I);
                            break;
                        case TB_DOUBLE:
                            addInstr(&owner->fn.instr, OP_DIV_F);
                            break;
                    }
                    break;
            }
            *r = (Ret){tDst, false, true};
            if(exprMulPrim(r)){
                return true;
            }
        }else tkerr("missing variable after *");
    }
    if(consume(DIV)){
        Ret right;
        op = consumedTk;
        Instr *lastLeft = lastInstr(owner->fn.instr);
        addRVal(&owner->fn.instr, r->lval, &r->type);
        if(exprCast(&right)){
            Type tDst;
            if(!arithTypeTo(&r->type, &right.type, &tDst)) tkerr("invalid operand type for * or /");
            addRVal(&owner->fn.instr, right.lval, &right.type);
            insertConvIfNeeded(lastLeft, &r->type, &tDst);
            insertConvIfNeeded(lastInstr(owner->fn.instr), &right.type, &tDst);
            switch (op->code) {
                case MUL:
                    switch (tDst.tb) {
                        case TB_INT:
                            addInstr(&owner->fn.instr, OP_MUL_I);
                            break;
                        case TB_DOUBLE:
                            addInstr(&owner->fn.instr, OP_MUL_F);
                            break;
                    }
                    break;
                case DIV:
                    switch (tDst.tb) {
                        case TB_INT:
                            addInstr(&owner->fn.instr, OP_DIV_I);
                            break;
                        case TB_DOUBLE:
                            addInstr(&owner->fn.instr, OP_DIV_F);
                            break;
                    }
                    break;
            }
            *r = (Ret){tDst, false, true};
            if(exprMulPrim(r)){
                return true;
            }
        }else tkerr("missing variable after /");
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return true;
}


// exprPostfix: exprPrimary exprPostfixPrim
bool exprPostfix(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(exprPrimary(r)){
        if(exprPostfixPrim(r)){
            return true;
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// exprPostfixPrim: LBRACKET expr RBRACKET exprPostfixPrim | DOT ID exprPostfixPrim | e
bool exprPostfixPrim(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(LBRACKET)){
        Ret idx;
        if(expr(&idx)){
            if(consume(RBRACKET)){
                if(r->type.n < 0) tkerr("only an array can be indexed");
                Type tInt = {TB_INT, NULL, -1};
                if(!convTo(&idx.type, &tInt)) tkerr("the index is not convertible to int");
                r->type.n = -1;
                r->lval = true;
                r->ct = false;
                if(exprPostfixPrim(r)){
                    return true;
                }
            }else tkerr("missing ] after expression");
        }else tkerr("missing value after [");
    }
    if(consume(DOT)){
        if(consume(ID)){
            Token *tkName = consumedTk;
            if(r->type.tb != TB_STRUCT) tkerr("a field can only be selected from a structure");
            Symbol *s = findSymbolInList(r->type.s->structMembers, tkName->text);
            if(!s) tkerr("the structure %s does not have a field%s", r->type.s->name, tkName->text);
            *r = (Ret){s->type, true, s->type.n >= 0};
            if(exprPostfixPrim(r)){
                return true;
            }
        }else tkerr("missing expression after .");
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return true;
}


// exprPrimary: ID ( LPAR ( expr ( COMMA expr )* )? RPAR )? | INT | DOUBLE | CHAR | STRING | LPAR expr RPAR ????????????????????? AT
bool exprPrimary(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(ID)){
        Token *tkName = consumedTk;
        Symbol *s = findSymbol(tkName->text);
        if(!s) tkerr("undefined id: %s", tkName->text);
        if(consume(LPAR)){
            if(s->kind != SK_FN) tkerr("only a function can be called");
            Ret rArg;
            Symbol *param = s->fn.params;
            if(expr(&rArg)){
                if(!param) tkerr("too many arguments in function call");
                if(!convTo(&rArg.type, &param->type)) tkerr("in call, cannot convert the argument type to the parameter type");
                addRVal(&owner->fn.instr,rArg.lval,&rArg.type); insertConvIfNeeded(lastInstr(owner->fn.instr),&rArg.type,&param->type);
                param = param->next;
                for(;;){
                    if(consume(COMMA)){
                        if(expr(&rArg)){
                            if(!param) tkerr("too many arguments in function call");
                            if(!convTo(&rArg.type, &param->type)) tkerr("in call, cannot convert the argument type to the parameter type");
                            addRVal(&owner->fn.instr, rArg.lval, &rArg.type);
                            insertConvIfNeeded(lastInstr(owner->fn.instr), &rArg.type, &param->type);
                            param = param->next;
                        }else tkerr("missing expression after ,");
                    }else break;
                }
            }
            if(consume(RPAR)){
                if(s->fn.extFnPtr){
                    addInstr(&owner->fn.instr,OP_CALL_EXT)->arg.extFnPtr=s->fn.extFnPtr;
                }else{
                    addInstr(&owner->fn.instr,OP_CALL)->arg.instr=s->fn.instr;
                }
                if(param) tkerr("too few arguments in function call");
                *r = (Ret){s->type, false, true};
                return true;
            }
        }else{
            if(s->kind == SK_FN) tkerr("a function can only be called");
            *r = (Ret){s->type, true, s->type.n >= 0};
            if(s->kind==SK_VAR){
                if(s->owner==NULL){ // global variables
                    addInstr(&owner->fn.instr,OP_ADDR)->arg.p=s->varMem;
                }else{ // local variables
                    switch(s->type.tb){
                        case TB_INT:addInstrWithInt(&owner->fn.instr,OP_FPADDR_I,s->varIdx+1);break;
                        case TB_DOUBLE:addInstrWithInt(&owner->fn.instr,OP_FPADDR_F,s->varIdx+1);break;
                    }
                }
            }
            if(s->kind==SK_PARAM){
                switch(s->type.tb){
                    case TB_INT:
                        addInstrWithInt(&owner->fn.instr,OP_FPADDR_I,s->paramIdx-symbolsLen(s->owner->fn.params)-
                                                                     1); break;
                    case TB_DOUBLE:
                        addInstrWithInt(&owner->fn.instr,OP_FPADDR_F,s->paramIdx-symbolsLen(s->owner->fn.params)-
                                                                     1); break;
                }
            }
            return true;
        }
    }
    if(consume(INT)){
        Token *ct = consumedTk;
        addInstrWithInt(&owner->fn.instr,OP_PUSH_I,ct->i);
        *r = (Ret){{TB_INT, NULL, -1}, false, true};
        return true;
    }
    if(consume(DOUBLE)){
        Token *ct = consumedTk;
        addInstrWithDouble(&owner->fn.instr,OP_PUSH_F,ct->d);
        *r = (Ret){{TB_DOUBLE, NULL, -1}, false, true};
        return true;
    }
    if(consume(CHAR)){
        Token *ct = consumedTk;
        *r = (Ret){{TB_CHAR, NULL, -1}, false, true};
        return true;
    }
    if(consume(STRING)){
        Token *ct = consumedTk;
        *r = (Ret){{TB_CHAR, NULL, 0}, false, true};
        return true;
    }
    if(consume(LPAR)){
        if(expr(r)){
            if(consume(RPAR)){
                return true;
            }else tkerr("missing ) after expression");
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// exprCast: LPAR typeBase arrayDecl? RPAR exprCast | exprUnary
bool exprCast(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(LPAR)){
        Type t;
        Ret op;
        if(typeBase(&t)){
            if(arrayDecl(&t)){}
            if(consume(RPAR)){
                if(exprCast(&op)){
                    if(t.tb == TB_STRUCT) tkerr("cannot convert to a structure type");
                    if(op.type.tb == TB_STRUCT) tkerr("cannot convert a structure");
                    if(op.type.n >= 0 && t.n < 0) tkerr("an array can be converted only to another array");
                    if(op.type.n < 0 && t.n >= 0) tkerr("a scalar can be converted only to another scalar");
                    *r = (Ret){t, false, true};
                    return true;
                }
            }else tkerr("missing ) after type and/or array declaration");
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    if(exprUnary(r)){
        return true;
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}

// exprUnary: ( SUB | NOT ) exprUnary | exprPostfix
bool exprUnary(Ret* r){
    Token *start = iTk;
    Instr *startInstr = owner?lastInstr(owner->fn.instr):NULL;
    if(consume(SUB)){
        if(exprUnary(r)){
            if(!canBeScalar(r)) tkerr("unary - or ! must have a scalar operand");
            r->lval = false;
            r->ct = true;
            return true;
        }
    }
    if(consume(NOT)){
        if(exprUnary(r)){
            if(!canBeScalar(r)) tkerr("unary - or ! must have a scalar operand");
            r->lval = false;
            r->ct = true;
            return true;
            return true;
        }
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    if(exprPostfix(r)){
        return true;
    }
    iTk = start;
    if(owner)delInstrAfter(startInstr);
    return false;
}


void parse(Token *tokens){
	iTk = tokens;
	if(!unit())tkerr("syntax error");
}

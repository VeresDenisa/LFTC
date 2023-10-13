#include <stdio.h>

#include "vm.h"
#include "ad.h"
#include "utils.h"

Instr *addInstr(Instr **list,Opcode op){
    Instr *i=(Instr*)safeAlloc(sizeof(Instr));
    i->op=op;
    i->next=NULL;
    if(*list){
        Instr *p=*list;
        while(p->next)p=p->next;
        p->next=i;
    }else{
        *list=i;
    }
    return i;
}

Instr *addInstrWithInt(Instr **list,Opcode op,int argVal){
    Instr *i=addInstr(list,op);
    i->arg.i=argVal;
    return i;
}

Instr *addInstrWithFloat(Instr **list,Opcode op,float argVal){
    Instr *i=addInstr(list,op);
    i->arg.f=argVal;
    return i;
}

Instr *addInstrWithDouble(Instr **list,Opcode op,double argVal){
    Instr *i=addInstr(list,op);
    i->arg.f=argVal;
    return i;
}


Instr *insertInstr(Instr *before,int op){
	Instr *i=(Instr*)safeAlloc(sizeof(Instr));
	i->op=op;
	i->next=before->next;
	before->next=i;
	return i;
	}

void delInstrAfter(Instr *instr){
	if(!instr)return;
	for(Instr *next=instr->next,*i=next;i;i=next){
		next=i->next;
		free(i);
		}
	instr->next=NULL;
}

Instr *lastInstr(Instr *list){
	if(list){
		while(list->next)list=list->next;
		}
	return list;
	}

#define MAXSTACK 10000
Val stack[10000];		// the stack
Val *SP=stack-1;		// Stack pointer - the stack's top - points to the value from the top of the stack
Val *FP=NULL;		// the initial value doesn't matter

void pushv(Val v){
    if(SP+1==stack+10000)err("trying to push into a full stack");
    *++SP=v;
}

Val popv(){
    if(SP==stack-1)err("trying to pop from empty stack");
    return *SP--;
}

void pushi(int i){
    if(SP+1==stack+10000)err("trying to push into a full stack");
    (++SP)->i=i;
}

int popi(){
    if(SP==stack-1)err("trying to pop from empty stack");
    return SP--->i;
}

void pushp(void *p){
    if(SP+1==stack+10000)err("trying to push into a full stack");
    (++SP)->p=p;
}

void *popp(){
    if(SP==stack-1)err("trying to pop from empty stack");
    return SP--->p;
}

void put_i(){
    printf("=> %d",popi());
}

void pushf(double f){
    if(SP+1==stack+10000)err("trying to push into a full stack");
    (++SP)->f=f;
}

double popf(){
    if(SP==stack-1)err("trying to pop from empty stack");
    return SP--->f;
}

void put_d(){
    printf("=> %lg",popf());
}

void vmInit(){
    Symbol *fn1=addExtFn("put_i",put_i,(Type){TB_VOID,NULL,-1});
    addFnParam(fn1,"i",(Type){TB_INT,NULL,-1});

    Symbol *fn2=addExtFn("put_d",put_d,(Type){TB_VOID,NULL,-1});
    addFnParam(fn2,"d",(Type){TB_DOUBLE,NULL,-1});
}

void run(Instr *IP){
    Val v;
    int iArg,iTop,iBefore;
    double fTop,dTop,dBefore;
	void *pTop;
    void(*extFnPtr)();
    for(;;){
        printf("%p/%d\t",IP,(int)(SP-stack+1));
        switch(IP->op){
            case OP_HALT:
                printf("HALT");
                return;
            case OP_PUSH_I:
                printf("PUSH.i\t%d",IP->arg.i);
                pushi(IP->arg.i);
                IP=IP->next;
                break;
            case OP_PUSH_F:
                printf("PUSH.f\t%lf",IP->arg.f);
                pushf(IP->arg.f);
                IP=IP->next;
                break;
            case OP_CALL:
                pushp(IP->next);
                printf("CALL\t%p",IP->arg.instr);
                IP=IP->arg.instr;
                break;
            case OP_CALL_EXT:
                extFnPtr=IP->arg.extFnPtr;
                printf("CALL_EXT\t%p\n",extFnPtr);
                extFnPtr();
                IP=IP->next;
                break;
            case OP_ENTER:
                pushp(FP);
                FP=SP;
                SP+=IP->arg.i;
                printf("ENTER\t%d",IP->arg.i);
                IP=IP->next;
                break;
			case OP_NOP:
				printf("NOP");
				IP=IP->next;
				break;
			case OP_DROP:
				popv();
				printf("DROP");
				IP=IP->next;
				break;
			case OP_RET:
				v=popv();
				iArg=IP->arg.i;
				printf("RET\t%d\t// i:%d, f:%g",iArg,v.i,v.f);
				IP=FP[-1].p;
				SP=FP-iArg-2;
				FP=FP[0].p;
				pushv(v);
				break;
            case OP_RET_VOID:
                iArg=IP->arg.i;
                printf("RET_VOID\t%d",iArg);
                IP=FP[-1].p;
                SP=FP-iArg-2;
                FP=FP[0].p;
                break;
            case OP_JMP:
                printf("JMP\t%p",IP->arg.instr);
                IP=IP->arg.instr;
                break;
            case OP_JF:
                iTop=popi();
                printf("JF\t%p\t// %d",IP->arg.instr,iTop);
                IP=iTop ? IP->next : IP->arg.instr;
                break;
            case OP_FPLOAD:
                v=FP[IP->arg.i];
                pushv(v);
                printf("FPLOAD\t%d\t// i:%d, f:%g",IP->arg.i,v.i,v.f);
                IP=IP->next;
                break;
            case OP_FPSTORE:
                v=popv();
                FP[IP->arg.i]=v;
                printf("FPSTORE\t%d\t// i:%d, f:%g",IP->arg.i,v.i,v.f);
                IP=IP->next;
                break;
			case OP_LOAD_I:
				pTop=popp();
				pushi(*(int*)pTop);
				printf("LOAD.i\t// *(int*)%p -> %d",pTop,*(int*)pTop);
				IP=IP->next;
				break;
			case OP_LOAD_F:
				pTop=popp();
				pushf(*(float*)pTop);
				printf("LOAD.f\t// *(float*)%p -> %d",pTop,*(float*)pTop);
				IP=IP->next;
				break;
			case OP_STORE_I:
				iTop=popi();
				v=popv();
				*(int*)v.p=iTop;
				pushi(iTop);
				printf("STORE.i\t// *(int*)%p=%d",v.p,iTop);
				IP=IP->next;
				break;
			case OP_STORE_F:
				fTop=popf();
				v=popv();
				*(float*)v.p=fTop;
				pushf(fTop);
				printf("STORE.f\t// *(float*)%p=%d",v.p,fTop);
				IP=IP->next;
				break;
			case OP_FPADDR_I:
				pTop=&FP[IP->arg.i].i;
				pushp(pTop);
				printf("FPADDR\t%d\t// %p",IP->arg.i,pTop);
				IP=IP->next;
				break;
            case OP_ADD_I:
                iTop=popi();
                iBefore=popi();
                pushi(iBefore+iTop);
                printf("ADD.i\t// %d+%d -> %d",iBefore,iTop,iBefore+iTop);
                IP=IP->next;
                break;
            case OP_ADD_F:
                dTop=popf();
                dBefore=popf();
                pushf(dBefore+dTop);
                printf("ADD.f\t// %lf+%lf -> %lf",dBefore,dTop,dBefore+dTop);
                IP=IP->next;
                break;
            case OP_SUB_I:
				iTop=popi();
				iBefore=popi();
				pushi(iBefore-iTop);
				printf("SUB.i\t// %d-%d -> %d",iBefore,iTop,iBefore-iTop);
				IP=IP->next;
				break;
            case OP_SUB_F:
				fTop=popf();
				dBefore=popf();
				pushf(dBefore-fTop);
				printf("SUB.f\t// %lg-%lg -> %lg",dBefore,fTop,dBefore-fTop);
				IP=IP->next;
				break;
			case OP_MUL_I:
				iTop=popi();
				iBefore=popi();
				pushi(iBefore*iTop);
				printf("MUL.i\t// %d*%d -> %d",iBefore,iTop,iBefore*iTop);
				IP=IP->next;
				break;
            case OP_LESS_I:
                iTop=popi();
                iBefore=popi();
                pushi(iBefore<iTop);
                printf("LESS.i\t// %d<%d -> %d",iBefore,iTop,iBefore<iTop);
                IP=IP->next;
                break;
            case OP_LESS_F:
                dTop=popf();
                dBefore=popf();
                pushi(dBefore<dTop);
                printf("LESS.f\t// %lf<%lf -> %d",dBefore,dTop,dBefore<dTop);
                IP=IP->next;
                break;
			case OP_CONV_I_F:
				iTop=popi();
				pushf((float)iTop);
				printf("CONV.i.f\t// %d -> %g",iTop,(float)iTop);
				IP=IP->next;
				break;
			case OP_CONV_F_I:
				fTop=popf();
				pushi((int)fTop);
				printf("CONV.f.i\t// %g -> %d",fTop,(int)fTop);
				IP=IP->next;
				break;
            default: err("run: instructiune neimplementata: %d",IP->op);
        }
        putchar('\n');
    }
}

/* The program implements the following AtomC source code:
f(2);
void f(int n){		// stack frame: n[-2] ret[-1] oldFP[0] i[1]
	int i=0;
	while(i<n){
		put_i(i);
		i=i+1;
		}
	}
*/

Instr *genTestProgram(){
    Instr *code=NULL;
    addInstrWithInt(&code,OP_PUSH_I,2);
    Instr *callPos=addInstr(&code,OP_CALL);
    addInstr(&code,OP_HALT);
    callPos->arg.instr=addInstrWithInt(&code,OP_ENTER,1);
    // int i=0;
    addInstrWithInt(&code,OP_PUSH_I,0);
    addInstrWithInt(&code,OP_FPSTORE,1);
    // while(i<n){
    Instr *whilePos=addInstrWithInt(&code,OP_FPLOAD,1);
    addInstrWithInt(&code,OP_FPLOAD,-2);
    addInstr(&code,OP_LESS_I);
    Instr *jfAfter=addInstr(&code,OP_JF);
    // put_i(i);
    addInstrWithInt(&code,OP_FPLOAD,1);
    Symbol *s=findSymbol("put_i");
    if(!s)err("undefined: put_i");
    addInstr(&code,OP_CALL_EXT)->arg.extFnPtr=s->fn.extFnPtr;
    // i=i+1;
    addInstrWithInt(&code,OP_FPLOAD,1);
    addInstrWithInt(&code,OP_PUSH_I,1);
    addInstr(&code,OP_ADD_I);
    addInstrWithInt(&code,OP_FPSTORE,1);
    // } ( the next iteration)
    addInstr(&code,OP_JMP)->arg.instr=whilePos;
    // returns from function
    jfAfter->arg.instr=addInstrWithInt(&code,OP_RET_VOID,1);
    return code;
}

/* The program implements the following AtomC source code:
f(2.0);
void f(double n){
	double i=0.0;
	while(i<n){
		put_d(i);
		i=i+0.5;
	}
*/

Instr *genTestProgramHomework(){
	// f(2.0);
	Instr *code=NULL;
	addInstrWithDouble(&code,OP_PUSH_F,2);
	Instr *callPos=addInstr(&code,OP_CALL);
	addInstr(&code,OP_HALT);
	callPos->arg.instr=addInstrWithInt(&code,OP_ENTER,1);
	// double i=0.0;
	addInstrWithDouble(&code,OP_PUSH_F,0.0);
	addInstrWithInt(&code,OP_FPSTORE,1);
	// while(i<n){
	Instr *whilePos=addInstrWithInt(&code,OP_FPLOAD,1);
	addInstrWithInt(&code,OP_FPLOAD,-2);
	addInstr(&code,OP_LESS_F);
	Instr *jfAfter=addInstr(&code,OP_JF);
	// put_d(i);
	addInstrWithInt(&code,OP_FPLOAD,1);
	Symbol *s=findSymbol("put_d");
	if(!s)err("undefined: put_d");
	addInstr(&code,OP_CALL_EXT)->arg.extFnPtr=s->fn.extFnPtr;
	// i=i+0.5;
	addInstrWithInt(&code,OP_FPLOAD,1);
	addInstrWithDouble(&code,OP_PUSH_F,0.5);
	addInstr(&code,OP_ADD_F);
	addInstrWithInt(&code,OP_FPSTORE,1);
	// } ( the next iteration)
	addInstr(&code,OP_JMP)->arg.instr=whilePos;
	// returns from function
	jfAfter->arg.instr=addInstrWithInt(&code,OP_RET_VOID,1);
	return code;
	}
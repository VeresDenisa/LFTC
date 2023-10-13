#include <stdio.h>

#include "utils.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"
#include "ad.h"
#include "at.h"
#include "gc.h"

int main()
{
    printf("AtomC:\n");
    char *inbuf = loadFile("tests/fisierulmeu.c");
    puts(inbuf);

    printf("\nAnaliza lexicala:\n");
    Token *tokens = tokenize(inbuf);
    //showTokens(tokens);

    printf("\nAnaliza sintactica, de domeniu si de tip:\n");
    pushDomain();

    vmInit();

    printf("\n");
    parse(tokens);
    printf("\n\n");

    //showDomain(symTable, "global");

    /*
    Instr *test=genTestProgramHomework();
    run(test);
    printf("\n\n");
    */

    Symbol*symMain=findSymbolInDomain(symTable,"main");
    if(!symMain)err("missing_main_function");
    Instr*entryCode=NULL;
    addInstr(&entryCode,OP_CALL)->arg.instr=symMain->fn.instr;
    addInstr(&entryCode,OP_HALT);
    run(entryCode);

    printf("\n");
    dropDomain();
    return 0;
}

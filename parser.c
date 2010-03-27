#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
 
#include "codes.h"
#include "symtable.h"
 
#define USAGE "Usage: parser [-q] infile"
 
/*===============*/
//---Globals-----//
 
// I/O vars
FILE* fin;
int verbose = 1;
 
//Parsing status vars
token currentToken;
union{
  int numeric;
  char string[MAX_IDENT_LENGTH + 1];
} tokenVal;
 
//The symbol table
symTableList** symTable;
int scope = 0;
 
//The code variables
int pc = 0;
instruction code[MAX_CODE_LENGTH];
 
//----------------//
/*================*/
 
//Input function
void readToken();
 
//Code generation functions
void genCode(opcode op, int l, int m);
void backPatch(int location, opcode op, int l, int m);
int genLabel();
int reserveCode();
void printCode();
 
//Non-terminal parsing functions
void program();
void block();
void statement();
void condition();
void expression();
void term();
void factor();
void throwError(errorCode code);
 
int main(int argc, char* argv[]){
 
  //Checking command-line arguments
  if(argc > 3){
    printf("%s\n", USAGE);
    return 1;
  }else if(argc < 2){
    fin = stdin;
  }else if(argc == 2){
    if(strcmp(argv[1], "-q") == 0){
      verbose = 0;
      fin = stdin;
    }else{
      fin = fopen(argv[1], "r");
      if(!fin){
        printf("%s\n", USAGE);
        return 1;
      }
    }
  }else if(argc == 2){
    if(strcmp("-q", argv[1]) != 0){
      printf("%s\n", USAGE);
      return 1;
    }
    verbose = 0;
    fin = fopen(argv[2], "r");
  }
  
  //Initializing the symbol table
  symTable = newTable();
  
  //Fetching the first token
  readToken();
  
  //Parsing the program
  program();
 
  //Generating code
  printCode();

  //END:

  deleteTable(symTable);

  return 0;
  
}
  
/***********************/
/* Grammar Functions   */
/***********************/
 
/***************************/
/* program ::= block "."   */
/***************************/
void program(){
  block();
  
  if(currentToken != periodsym){
    if(currentToken != nulsym)
      throwError(WRONG_SYM_AFTER_STATE);
    else
      throwError(PERIOD_EXPEC);
  }
}
 
/***********************************************************/
/* block ::= const-declaration var-declaration statement   */
/***********************************************************/
void block(){
 
  int numVars = 0;
  scope++;
  char tempSymbol[MAX_IDENT_LENGTH + 1];
 
  if(currentToken == constsym){
    /***************************************************************/
    /* const-declaration ::= [ "const" ident "=" number            */
    /*                               {"," ident "=" number} ";"]   */
    /***************************************************************/
    do{
      readToken();
      
      if(currentToken != identsym){
        throwError(ID_FOLLOW_CONST_VAR_PROC);
      }else{
        strcpy(tempSymbol, tokenVal.string);
      }
 
      readToken();
      
      if(currentToken != eqsym){
        if(currentToken == becomessym)
          throwError(EQ_NOT_BECOMES);
        else
          throwError(EQ_FOLLOW_ID);
      }
      
      readToken();
    
      if(currentToken != numbersym){
        throwError(NUM_FOLLOW_EQ);
      }else{
        insertSymbol(symTable, newSymbol(CONST, tempSymbol, scope, 0,
                                         tokenVal.numeric));
      }
    
      readToken();
    }while(currentToken == commasym);
    
    if(currentToken != semicolonsym)
      throwError(SEMICOL_COMMA_MISS);
 
    readToken();
  }
  
  if(currentToken == intsym){
    /*******************************************************/
    /* var-declaration ::= ["int" ident {"," ident} ";"] */
    /*******************************************************/
    do{
      readToken();
      
      if(currentToken != identsym){
        throwError(ID_FOLLOW_CONST_VAR_PROC);
      }else{
        insertSymbol(symTable, newSymbol(VAR, tokenVal.string,
                                         scope, BASE_OFFSET + numVars++, 0));
      }
        
      readToken();
    }while(currentToken == commasym);
    
    if(currentToken != semicolonsym)
      throwError(SEMICOL_COMMA_MISS);
      
    readToken();
  }
  
  genCode(INC, 0, BASE_OFFSET + numVars);
 
  statement();
  
  genCode(OPR, 0, RET);
 
  return;
}
 
/******************************************************************************/
/* statement ::= [ ident ":=" expression                                      */
/*               | "snga'i" statement {";" statement} "fpe'"                  */
/*               | "txo" condition "tsakrr" statement ["txokefyaw" statement] */
/*               | "tengkrr" condition "si" statement                         */
/*               | e ]                                                        */
/******************************************************************************/
void statement(){
  symTableEntry* symbol;
  int tempLabels[2];
 
  if(currentToken == identsym){
    symbol = findSymbol(symTable, VAR, tokenVal.string, scope);
    if(!symbol){
      if(findSymbol(symTable, CONST, tokenVal.string, scope))
        throwError(CANNOT_ASSIGN_TO_CONST_OR_PROC);
      else
        throwError(UNDEC_ID);
    }
    
    readToken();
    
    if(currentToken != becomessym)
      throwError(ASSIGN_EXPEC);
    
    if(symbol->type == CONST)
      throwError(CANNOT_ASSIGN_TO_CONST_OR_PROC);
    
    readToken();
    
    expression();
 
    genCode(STO, 0, symbol->offset);
  }
  else if(currentToken == sngaisym){ // 'beginsym'
    readToken();
    
    statement();
    
    while(currentToken == semicolonsym){
      readToken();
      
      statement();
    }
      
    if(currentToken != fpesym && 
        (currentToken != identsym && currentToken != sngaisym
        && currentToken != txosym && currentToken != tengkrrsym)) // 'endsym'
      //throwError(WRONG_SYM_AFTER_STATE);
      throwError(SEMICOL_OR_RBRACK_EXPEC);
    else if(currentToken == identsym || currentToken == sngaisym
        || currentToken == txosym || currentToken == tengkrrsym)
      throwError(SEMICOL_BW_STATE_MISS);
    
    readToken();
  }
  else if(currentToken == txosym){ // 'ifsym'
    readToken();
    
    condition();
    
    tempLabels[0] = reserveCode(); //Conditional jump will go here
 
    if(currentToken != tsakrrsym) // 'thensym'
      throwError(TSAKRR_EXPEC);
    
    readToken();
    
    statement();
 
    backPatch(tempLabels[0], JPC, 0, genLabel());
  }
  else if(currentToken == tengkrrsym){ // 'whilesym'
 
    readToken();
    
    tempLabels[0] = genLabel(); //Jump back up to here at the end of the loop
 
    condition();
 
    tempLabels[1] = reserveCode(); //Stick the conditional jump here
 
    if(currentToken != sisym) // 'dosym'
      throwError(SI_EXPEC);
    
    readToken();
      
    statement();
    
    genCode(JMP, 0, tempLabels[0]);
    backPatch(tempLabels[1], JPC, 0, genLabel());
  }
  else if(currentToken == fpesym){
    return;
  }
  else{
    if(currentToken == periodsym)
      throwError(RBRACK_EXPEC_AT_END);
    else
      throwError(STATEMENT_EXPEC);
  }
  
  return;
}
 
/***************************************************/
/* condition ::= "odd" expression                  */
/*               | expression rel-op expression    */
/***************************************************/
void condition(){
  token operation;
 
  if(currentToken == oddsym){
    readToken();
    
    expression();
  }
  else{
    expression();
    
    /*******************************************/
    /* rel-op ::= "="|"!="|"<"|"<="|">"|">="   */
    /*******************************************/
    if(currentToken != eqsym
       && currentToken != neqsym
       && currentToken != lessym
       && currentToken != leqsym
       && currentToken != gtrsym
       && currentToken != geqsym){

      if(currentToken == becomessym)
        throwError(EQ_NOT_BECOMES);
 
      throwError(REL_OP_EXPEC);
 
    }else{
      operation = currentToken;
    }
    
    readToken();
    
    expression();
 
    if(operation == eqsym)
      genCode(OPR, 0, EQL);
    else if(operation == neqsym)
      genCode(OPR, 0, NEQ);
    else if(operation == lessym)
      genCode(OPR, 0, LSS);
    else if(operation == leqsym)
      genCode(OPR, 0, LEQ);
    else if(operation == gtrsym)
      genCode(OPR, 0, GTR);
    else if(operation == geqsym)
      genCode(OPR, 0, GEQ);
 
  }
  
  return;
}
 
/******************************************************/
/* expression ::= [ "+"|"-" ] term {("+"|"-") term}   */
/******************************************************/
void expression(){
  int minus = 0; //A flag to negate the expression if we need to
  token operator;
 
  if(currentToken != plussym && currentToken != minussym
     && currentToken != lparentsym && currentToken != identsym
     && currentToken != numbersym)
    throwError(SYMBOL_CANNOT_BEGIN_THIS_EXP);

  if(currentToken == minussym)
    minus = 1;
 
  if(currentToken == plussym || currentToken == minussym)
    readToken();
  
  if(currentToken == procsym)
    throwError(EXP_CANNOT_CONTAIN_PROC_IDENT);
  
  term();
  
  while(currentToken == plussym || currentToken == minussym){
    operator = currentToken;
    
    readToken();
    
    if(currentToken == procsym)
      throwError(EXP_CANNOT_CONTAIN_PROC_IDENT);
    
    term();
 
    if(operator == plussym)
      genCode(OPR, 0, ADD);
    else if(operator == minussym)
      genCode(OPR, 0, SUB);
  }
 
  if(minus)
    genCode(OPR, 0, NEG);
 
  return;
}
 
/****************************************/
/* term ::= factor {("*"|"/") factor}   */
/****************************************/
void term(){
  token tempToken;
  factor();
  
  while(currentToken == multsym || currentToken == slashsym){
    tempToken = currentToken;
 
    readToken();
    
    factor();
 
    if(tempToken == multsym)
      genCode(OPR, 0, MUL);
    else if(tempToken == slashsym)
      genCode(OPR, 0, DIV);
  }
  
  return;
}
 
/****************************************************/
/* factor ::= ident | number | "(" expression ")"   */
/****************************************************/
void factor(){
  symTableEntry* symbol;
  if(currentToken == identsym){
    //First looking for a constant that matches
    symbol = findSymbol(symTable, CONST, tokenVal.string, scope);
    if(symbol){
      genCode(LIT, 0, symbol->value);
    }else{
      //If that fails, looking for a variable
      symbol = findSymbol(symTable, VAR, tokenVal.string, scope);
      if(symbol){
        genCode(LOD, 0, symbol->offset);
      }else{
        throwError(UNDEC_ID);
      }
    }
    readToken();
  }else if(currentToken == numbersym){
    genCode(LIT, 0, tokenVal.numeric);
    readToken();
  }else if(currentToken == lparentsym){
    readToken();
    
    expression();
    
    if(currentToken != rparentsym)
      throwError(RPAREN_MISS);
    else
      readToken();
  }
  else
    throwError(PREC_FACTOR_CANNOT_BEGIN_SYM);
  
  return;
}
 
void readToken(){
  
  //Making sure we still have input to read
  if(feof(fin)){
    currentToken = nulsym;
    return;
  }
 
  //Reading the new token in
  fscanf(fin, "%d ", (int*)(&currentToken));
 
  //Checking to see if we need to read anything else in
  if(currentToken == identsym){
    fscanf(fin, "%s ", tokenVal.string);
  }
 
  if(currentToken == numbersym){
    fscanf(fin, "%d ", &tokenVal.numeric);
  }
 
}
 
void throwError(errorCode code){
  printf("Error #%d: ", code);

  switch(code){
  case(EQ_NOT_BECOMES): // Used
    printf("Use = instead of :=.\n");
    break;
  case(NUM_FOLLOW_EQ): // Used
    printf("= must be followed by a number.\n");
    break;
  case(EQ_FOLLOW_ID): // Used
    printf("Identifier must be followed by a number.\n");
    break;
  case(ID_FOLLOW_CONST_VAR_PROC): // Used
    printf("const, var, procedure must be followed by identifier.\n");
    break;
  case(SEMICOL_COMMA_MISS): // Used
    printf("Semicolon or comma missing.\n");
    break;
  case(WRONG_SYM_AFTER_PROC): // NO
    printf("Incorrect symbol after procedure declaration.\n");
    break;
  case(STATEMENT_EXPEC): // Used
    printf("Statement expected.\n");
    break;
  case(WRONG_SYM_AFTER_STATE): // Used
    printf("Incorrect symbol after statement part in block.\n");
    break;
  case(PERIOD_EXPEC): // Used
    printf("Period expected.\n");
    break;
  case(SEMICOL_BW_STATE_MISS): // Used
    printf("Semicolon between statements missing.\n");
    break;
  case(UNDEC_ID): // Used
    printf("Undeclared identifier.\n");
    break;
  case(CANNOT_ASSIGN_TO_CONST_OR_PROC): // Used
    printf("Assignment to constant or procedure is not allowed.\n");
    break;
  case(ASSIGN_EXPEC): // Used
    printf("Assignment operator expected.\n");
    break;
  case(ID_FOLLOW_SYAW): // NO
    printf("syaw must be followed by an identifier.\n");
    break;
  case(CONST_OR_VAR_CALL_USELESS): // NO
    printf("Call of a constant or variable is meaningless.\n");
    break;
  case(TSAKRR_EXPEC): // Used
    printf("tsakrr expected.\n");
    break;
  case(SEMICOL_OR_RBRACK_EXPEC): // Used
    printf("Semicolon or } expected.\n");
    break;
  case(SI_EXPEC): // Used
    printf("si expected.\n");
    break;
  case(WRONG_SYM_FOLLOWING_STATE): // Used parallel to SEMICOL_OR_RBRACK_EXPEC
    printf("Incorrect symbol following statement.\n");
    break;
  case(REL_OP_EXPEC): // Used
    printf("Relational operator expected.\n");
    break;
  case(EXP_CANNOT_CONTAIN_PROC_IDENT): // Used
    printf("Expression must not contain a procedure identifier.\n");
    break;
  case(RPAREN_MISS): // Used
    printf("Right parenthesis missing.\n");
    break;
  case(PREC_FACTOR_CANNOT_BEGIN_SYM): // Used
    printf("The preceding factor cannot begin with this symbol.\n");
    break;
  case(SYMBOL_CANNOT_BEGIN_THIS_EXP):
    printf("An expression cannot begin with this symbol.\n");
    break;
  case(NUMBER_TOO_LARGE): // NO?
    printf("This number is too large.\n");
    break;
  case(RBRACK_EXPEC_AT_END):
    printf("} expected at the end of the program.\n");
    break;
  default:
    printf("Improper error code.\n");
    break;
  }
  deleteTable(symTable);
  abort();
}
 
void genCode(opcode op, int l, int m){
 
  //Checking pc size for overflow
  if(pc == MAX_CODE_LENGTH){
    printf("ERROR: Code too long\n");
    return;
  }
 
  //Inserting code and incrementing pc
  code[pc].op = (int)op;
  code[pc].l = l;
  code[pc].m = m;
  pc++;
 
}
 
void backPatch(int location, opcode op, int l, int m){
  
  if(location >= MAX_CODE_LENGTH){
    printf("ERROR: Invalid code index\n");
    return;
  }
 
  code[location].op = (int)op;
  code[location].l = l;
  code[location].m = m;
 
}
 
int genLabel(){
 
  return pc;
 
}
 
int reserveCode(){
  
  //Checking to make sure we have room
  if(pc >= MAX_CODE_LENGTH){
    printf("ERROR: Code too long\n");
    return -1;
  }
 
  return pc++;
 
}
 
void printCode(){
  
  int i;
  for(i = 0; i < pc; i++)
    printf("%d %d %d\n", (int)code[i].op, code[i].l, code[i].m);
}
 

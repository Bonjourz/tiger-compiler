#include "prog1.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define max(a, b) a > b ? a : b

typedef struct table *Table_;

struct table {string id; int value; Table_ tail;};


Table_ Table(string id, int value, struct table *tail) {
	Table_ t = malloc(sizeof(*t));
	t->id = id;
	t->value = value;
	t->tail = tail;
	return t;
}

Table_ t = NULL;

int getFromExpList(A_expList exps);

int getFromExp(A_exp exp);

int maxargs(A_stm stm)
{
	//TODO: put your code here.
    if (stm == NULL)
        return 0;

    int result = 0;

    if (stm->kind == A_compoundStm)
        result = max(maxargs(stm->u.compound.stm1), maxargs(stm->u.compound.stm2));

    else if (stm->kind == A_printStm) {
        int tmp = getFromExpList(stm->u.print.exps);
        result = result > tmp ? result : tmp;
    }

    else {
        int tmp = getFromExp(stm->u.assign.exp);
        result = result > tmp ? result : tmp;
    }

	return result;
}

int getFromExpList(A_expList exps) {
    if (exps == NULL)
        return 0;

	if (exps->kind == A_lastExpList)
		return 1;

	else 
		return 1 + getFromExpList(exps->u.pair.tail);
}

int getFromExp(A_exp exp) {
    if (exp == NULL)
        return 0;

    if (exp->kind == A_eseqExp) 
        return maxargs(exp->u.eseq.stm);

    else
        return 0;
}

int parseExp(A_exp);

int getValFromTable(string, Table_);

void modifyTable(string, int, Table_);

void parsePrint(A_expList);

void interp(A_stm stm)
{
	//TODO: put your code here.
    //
    
    if (stm == NULL)
        return;

    if (stm->kind == A_compoundStm) {
        interp(stm->u.compound.stm1);
        interp(stm->u.compound.stm2);
    }

    else if (stm->kind == A_assignStm) {
        int result = parseExp(stm->u.assign.exp);
        modifyTable(stm->u.assign.id, result, t);
    }

    else if (stm->kind == A_printStm)
        parsePrint(stm->u.print.exps);
}

void modifyTable(string id, int val, Table_ table) {
    Table_ table_tmp = Table(id, val, table);
    t = table_tmp;
}

int getValFromTable(string id, Table_ table) {
    Table_ table_tmp = table;
    while (table_tmp != NULL) {
        if (!strcmp(id, table_tmp->id))
            return table_tmp->value;

        table_tmp = table_tmp->tail;
    }
    
    return 0;
}

void parsePrint(A_expList expList) {
    if (expList == NULL)
        return;
    
    A_expList list_tmp = expList;

    while (list_tmp->kind != A_lastExpList) {
        printf("%d ", parseExp(list_tmp->u.pair.head));
        list_tmp = list_tmp->u.pair.tail;
    }

    printf("%d\n", parseExp(list_tmp->u.last));
}

int parseExp(A_exp exp) {
    int left, right, result;

    switch (exp->kind) {
        case A_idExp:
            result = getValFromTable(exp->u.id, t);
            break;

        case A_numExp:
            result = exp->u.num;
            break;

        case A_opExp: 
            left = parseExp(exp->u.op.left);
            right = parseExp(exp->u.op.right);
            switch (exp->u.op.oper) {
                case A_plus:
                    result = left + right;
                    break;
                
                case A_minus:
                    result = left - right;
                    break;

                case A_times:
                    result = left * right;
                    break;

                case A_div:
                    result = left / right;
                    break;
            }
            break;

        case A_eseqExp:
            interp(exp->u.eseq.stm);
            result = parseExp(exp->u.eseq.exp);
            break;
    }

    return result;
}

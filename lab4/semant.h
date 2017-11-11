#ifndef __SEMANT_H_
#define __SEMANT_H_

#include "absyn.h"
#include "symbol.h"

struct expty;

struct expty transVar(S_table venv, S_table tenv, A_var v);
struct expty transExp(S_table venv, S_table tenv, A_exp a);
void		 transDec(S_table venv, S_table tenv, A_dec d);
void		 transTy (              S_table tenv, A_namety n);

void SEM_transProg(A_exp exp);

#endif

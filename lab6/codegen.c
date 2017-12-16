#include <stdio.h>
#include <stdlib.h>
#include "util.h"
#include "symbol.h"
#include "absyn.h"
#include "temp.h"
#include "errormsg.h"
#include "tree.h"
#include "assem.h"
#include "frame.h"
#include "codegen.h"
#include "table.h"

#define STRLEN 20
//Lab 6: put your code here
static Temp_temp munchExp(T_exp e);
static void munchStm(T_stm s);

static AS_instrList iList = NULL, last = NULL;

static void emit(AS_instr inst) {
	if (last != NULL) 
    	last = last->tail = AS_InstrList(inst, NULL);
  	else 
    	last = iList = AS_InstrList(inst, NULL);
  
}

static Temp_tempList L(Temp_temp h, Temp_tempList t) {
	return Temp_TempList(h, t);
}

AS_instrList F_codegen(F_frame f, T_stmList stmList) {
	AS_instrList list;
	T_stmList sl;
	// TO DO: caller save and callee save
	for (sl = stmList; sl; sl = sl->tail)
		munchStm(sl->head);
	list = iList;
	iList = last = NULL;
	return list;
}

static Temp_temp emitBinOp(T_exp left, T_exp right, T_binOp op) {
	Temp_temp tmp = Temp_newtemp();
	Temp_temp l = munchExp(left);
	Temp_temp r =  munchExp(right);
	char* out1 = (char *)checked_malloc(STRLEN);
	char* out2 = (char *)checked_malloc(STRLEN);
	char* str;
	switch(op) {
		case T_plus: {str = "addl"; break;}
		case T_minus: {str = "subl"; break;}
		case T_mul: {str = "imul"; break;}
		case T_div:
		case T_and:
		case T_or:
		case T_lshift:
		case T_rshift:
		case T_arshift:
		case T_xor:
			assert(0);
	}
	sprintf(out1, "movl `s0, `d0");
	emit(AS_Move(out1, L(tmp, NULL), L(l, NULL)));
	sprintf(out2, "%s `s0, `d0", str);
	emit(AS_Oper(out2, L(tmp, NULL), L(r, L(tmp, NULL)), NULL));
	return tmp;
}

static int munchArgs(T_expList args) {
	if (args == NULL)
		return 0;

	else {
		int i = munchArgs(args->tail);
		Temp_temp t = munchExp(args->head);
		emit(AS_Oper("pushl `s0", NULL, L(t, NULL), NULL));
		return ++i;
	}
}

static Temp_temp munchExp(T_exp e) {
	switch(e->kind) {
		case T_BINOP: {
			//printf("binop\n");
			T_binOp op = e->u.BINOP.op;
			switch(op) {
				case T_plus: {
					if (e->u.BINOP.left->kind == T_CONST && e->u.BINOP.right->kind == T_CONST) {
						// binop(+, const, const)
						Temp_temp tmp = Temp_newtemp();
						int lv = e->u.BINOP.left->u.CONST, rv = e->u.BINOP.right->u.CONST;
						char* out1 = (char*)checked_malloc(STRLEN);
						char* out2 = (char*)checked_malloc(STRLEN);
						sprintf(out1, "movl $%d, `d0", lv);
						emit(AS_Oper(out1, L(tmp, NULL), NULL, NULL));
						sprintf(out2, "addl $%d, `d0", rv);
						emit(AS_Oper(out2, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}

					else if (e->u.BINOP.left->kind == T_CONST) {
						// binop(+, const, e)
						int i = e->u.BINOP.left->u.CONST;
						Temp_temp tmp = Temp_newtemp();
						Temp_temp r = munchExp(e->u.BINOP.right);
						char* out1 = (char*)checked_malloc(STRLEN);
						char* out2 = (char*)checked_malloc(STRLEN);
						sprintf(out1, "movl `s0, `d0");
						emit(AS_Move(out1, L(tmp, NULL), L(r, NULL)));
						sprintf(out2, "addl $%d, `d0", i);
						emit(AS_Oper(out2, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}

					else if (e->u.BINOP.right->kind == T_CONST) {
						// binop(+, e, const)
						int i = e->u.BINOP.right->u.CONST;
						Temp_temp tmp = Temp_newtemp();
						Temp_temp l = munchExp(e->u.BINOP.left);
						char* out1 = (char*)checked_malloc(STRLEN);
						char* out2 = (char*)checked_malloc(STRLEN);
						sprintf(out1, "movl `s0, `d0");
						emit(AS_Move(out1, L(tmp, NULL), L(l, NULL)));
						sprintf(out2, "addl $%d, `d0", i);
						emit(AS_Oper(out2, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}

					else 
						return emitBinOp(e->u.BINOP.left, e->u.BINOP.right, op);
				}

				case T_minus: {
					if (e->u.BINOP.right->kind == T_CONST) {
						Temp_temp l = munchExp(e->u.BINOP.left);
						Temp_temp tmp = Temp_newtemp();
						int i = e->u.BINOP.right->u.CONST;
						char* out1 = (char*)checked_malloc(STRLEN);
						char* out2 = (char*)checked_malloc(STRLEN);
						sprintf(out1, "movl `s0, `d0");
						emit(AS_Move(out1, L(tmp, NULL), L(l, NULL)));
						sprintf(out2, "subl $%d, `d0", i);
						emit(AS_Oper(out2, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}
					else 
						return emitBinOp(e->u.BINOP.left, e->u.BINOP.right, op);
				}

				case T_mul: {
					if (e->u.BINOP.left->kind == T_CONST && e->u.BINOP.right->kind == T_CONST) {
						// BINOP(T_mul, CONST, CONST);
						Temp_temp tmp = Temp_newtemp();
						int lv = e->u.BINOP.left->u.CONST, rv = e->u.BINOP.right->u.CONST;
						char* out1 = (char*)checked_malloc(STRLEN);
						char* out2 = (char*)checked_malloc(STRLEN);
						sprintf(out1, "movl $%d, `d0", lv);
						emit(AS_Oper(out1, L(tmp, NULL), NULL, NULL));
						sprintf(out2, "imul $%d, `d0", rv);
						emit(AS_Oper(out2, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}

					else if (e->u.BINOP.left->kind == T_CONST) {
						// BINOP(T_mul, CONST, exp);
						int i = e->u.BINOP.left->u.CONST;
						Temp_temp tmp = Temp_newtemp();
						Temp_temp r = munchExp(e->u.BINOP.right);
						char* out1 = (char*)checked_malloc(STRLEN);
						char* out2 = (char*)checked_malloc(STRLEN);
						sprintf(out1, "movl `s0, `d0");
						emit(AS_Move(out1, L(tmp, NULL), L(r, NULL)));
						sprintf(out2, "imul $%d, `d0", i);
						emit(AS_Oper(out2, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}

					else if (e->u.BINOP.right->kind == T_CONST &&
						e->u.BINOP.left->kind == T_MEM &&
						e->u.BINOP.left->u.MEM->kind == T_BINOP &&
						e->u.BINOP.left->u.MEM->u.BINOP.op == T_plus &&
						e->u.BINOP.left->u.MEM->u.BINOP.left->kind == T_TEMP &&
						e->u.BINOP.left->u.MEM->u.BINOP.left->u.TEMP == F_FP() &&
						e->u.BINOP.left->u.MEM->u.BINOP.right->kind == T_CONST) {
						// BINOP(*, MEM(BINOP(+, TEMP, CONST)), CONST)	// optimize
						Temp_temp tmp = Temp_newtemp();
						char* out1 = (char*)checked_malloc(STRLEN);
						int num1 = e->u.BINOP.left->u.MEM->u.BINOP.right->u.CONST;
						sprintf(out1, "movl %d(`s0), `d0", num1);
						emit(AS_Oper(out1, L(tmp, NULL), L(F_FP(), NULL), NULL));

						int num2 = e->u.BINOP.right->u.CONST;
						char* out2 = (char*)checked_malloc(STRLEN);
						sprintf(out2, "imul $%d, `d0", num2);
						emit(AS_Oper(out2, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}

					else if (e->u.BINOP.right->kind == T_CONST) {
						// BINOP(T_mul, exp, CONST);
						int i = e->u.BINOP.right->u.CONST;
						Temp_temp tmp = Temp_newtemp();
						Temp_temp l = munchExp(e->u.BINOP.left);
						char* out1 = (char*)checked_malloc(STRLEN);
						char* out2 = (char*)checked_malloc(STRLEN);
						sprintf(out1, "movl `s0, `d0");
						emit(AS_Move(out1, L(tmp, NULL), L(l, NULL)));
						sprintf(out2, "imul $%d, `d0", i);
						emit(AS_Oper(out2, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}

					else 
						return emitBinOp(e->u.BINOP.left, e->u.BINOP.right, op);
				}

				case T_div: {
					Temp_temp l = munchExp(e->u.BINOP.left);
					Temp_temp r = munchExp(e->u.BINOP.right);
					Temp_temp tmp = Temp_newtemp();
					char *out1 = (char *)checked_malloc(STRLEN);
					sprintf(out1, "movl `s0, `d0");
					char *out2 = (char *)checked_malloc(STRLEN);
					sprintf(out2, "cltd");
					char *out3 = (char *)checked_malloc(STRLEN);
					sprintf(out3, "idivl `s0");
					char *out4 = (char *)checked_malloc(STRLEN);
					sprintf(out4, "movl `s0, `d0");
					emit(AS_Move(out1, L(F_EAX(), NULL), L(l, NULL)));
					emit(AS_Oper(out2, L(F_EDX(), NULL), NULL, NULL));
					emit(AS_Oper(out3, L(F_EDX(), L(F_EAX(), NULL)), L(r, NULL), NULL));
					emit(AS_Move(out4, L(tmp, NULL), L(F_EAX(), NULL)));
					return tmp;
				}

				case T_and:
				case T_or:
				case T_lshift:
				case T_rshift:
				case T_arshift:
				case T_xor:
					assert(0);
			}
		}
		case T_MEM: {
			//printf("mem\n");
			if (e->u.MEM->kind == T_BINOP) {
				T_exp binexp = e->u.MEM;
				if (binexp->u.BINOP.right->kind == T_CONST && 
						(binexp->u.BINOP.op == T_plus || binexp->u.BINOP.op == T_minus)) {
					// MEM(BINOP(+, e, CONST)), MEM(BINOP(-, e, CONST))
					Temp_temp l = munchExp(binexp->u.BINOP.left), tmp = Temp_newtemp();
					int i = binexp->u.BINOP.right->u.CONST;
					if (binexp->u.BINOP.op == T_minus) 
						i = -i;
					char* out = (char*)checked_malloc(STRLEN);
					sprintf(out, "movl %d(`s0), `d0", i);
					emit(AS_Oper(out, L(tmp, NULL), L(l, NULL), NULL));
					return tmp;
				}

				else if (binexp->u.BINOP.left->kind == T_CONST && binexp->u.BINOP.op == T_plus) {
					// MEM(BINOP(+, CONST, e))
					Temp_temp r = munchExp(binexp->u.BINOP.right), tmp = Temp_newtemp();
					int i = binexp->u.BINOP.left->u.CONST;
					char* out = (char*)checked_malloc(STRLEN);
					sprintf(out, "movl %d(`s0), `d0", i);
					emit(AS_Oper(out, L(tmp, NULL), L(r, NULL), NULL));
					return tmp;
				}
			}

			else if (e->u.MEM->kind == T_CONST) {
				Temp_temp tmp = Temp_newtemp();
				int i = e->u.MEM->u.CONST;
				char* out = (char*)checked_malloc(STRLEN);
				sprintf(out, "movl %d, `d0", i);
				emit(AS_Oper(out, L(tmp, NULL), NULL, NULL));
				return tmp;
			}

			/* Default */
			Temp_temp tmp = Temp_newtemp(), r = munchExp(e->u.MEM);
			char* out = (char*)checked_malloc(STRLEN);
			sprintf(out, "movl (`s0), `d0");
			emit(AS_Oper(out, L(tmp, NULL), L(r, NULL), NULL));
			return tmp;
		}

		case T_TEMP: {
			//printf("temp\n");
			return e->u.TEMP;
		}

		case T_ESEQ: {
			//printf("eseq\n");
			if (e->u.ESEQ.stm)
				munchStm(e->u.ESEQ.stm);
			return munchExp(e->u.ESEQ.exp);
		}

		case T_NAME: {
			//printf("name\n");
			Temp_temp tmp = Temp_newtemp();
			char* out = (char*)checked_malloc(STRLEN);
			sprintf(out, "movl $.%s, `d0", Temp_labelstring(e->u.NAME));
			// TO DO: src ??
			emit(AS_Oper(out, L(tmp, NULL), NULL, NULL));
			return tmp;
		}

		case T_CONST: {
			//printf("const\n");
			Temp_temp tmp = Temp_newtemp();
			char* out = (char*)checked_malloc(STRLEN);
			sprintf(out, "movl $%d, `d0", e->u.CONST);
			emit(AS_Oper(out, L(tmp, NULL), NULL, NULL));
			return tmp;
		}

		case T_CALL: {
			//printf("call\n");
			// TO DO: src and dst
			emit(AS_Oper("pushl %ebx", NULL, NULL, NULL));
			emit(AS_Oper("pushl %ecx", NULL, NULL, NULL));
			int argNum = munchArgs(e->u.CALL.args);
			char* out1 = (char*)checked_malloc(STRLEN);
			sprintf(out1, "call %s", Temp_labelstring(e->u.CALL.fun->u.NAME));
			// TO DO
			emit(AS_Oper(out1, L(F_EAX(), NULL), NULL, NULL));
			char* out2 = (char *)checked_malloc(STRLEN);
			sprintf(out2, "addl $%d, `s0", argNum * 4);
			emit(AS_Oper(out2, NULL, L(F_ESP(), NULL), NULL));
			emit(AS_Oper("popl %ecx", NULL, NULL, NULL));
			emit(AS_Oper("popl %ebx", NULL, NULL, NULL));
			Temp_temp tmp = Temp_newtemp();
			return F_EAX();
		}
	}
}

static void munchStm(T_stm s) {
	switch(s->kind) {
		case T_MOVE: {
			//printf("move\n");
			if (s->u.MOVE.dst->kind == T_MEM &&
				s->u.MOVE.dst->u.MEM->kind == T_BINOP &&
				s->u.MOVE.dst->u.MEM->u.BINOP.op == T_plus &&
				s->u.MOVE.dst->u.MEM->u.BINOP.left->kind == T_TEMP &&
				s->u.MOVE.dst->u.MEM->u.BINOP.right->kind == T_CONST &&
				s->u.MOVE.src->kind == T_CONST) {
				// move(mem(binop(+, temp, const(i))), const(i))
				int srci = s->u.MOVE.src->u.CONST;
				int dsti = s->u.MOVE.dst->u.MEM->u.BINOP.right->u.CONST;
				Temp_temp dst = s->u.MOVE.dst->u.MEM->u.BINOP.left->u.TEMP;
				char* out = (char*)checked_malloc(STRLEN);
				sprintf(out, "movl $%d, %d(`s0)", srci, dsti);
				emit(AS_Oper(out, NULL, L(dst, NULL), NULL));
				return;
			}

			else if (s->u.MOVE.dst->kind == T_MEM &&
					s->u.MOVE.dst->u.MEM->kind == T_BINOP &&
					(s->u.MOVE.dst->u.MEM->u.BINOP.op == T_plus || 
						s->u.MOVE.dst->u.MEM->u.BINOP.op == T_minus) &&
					s->u.MOVE.dst->u.MEM->u.BINOP.right->kind == T_CONST) {
				// move(mem(binop(+/-, e1, const(i))), e2)
				int i = s->u.MOVE.dst->u.MEM->u.BINOP.right->u.CONST;
				if (s->u.MOVE.dst->u.MEM->u.BINOP.op == T_minus)
					i = -i;
				Temp_temp dst = munchExp(s->u.MOVE.dst->u.MEM->u.BINOP.left);
				Temp_temp src = munchExp(s->u.MOVE.src);
				char* out = (char*)checked_malloc(STRLEN);
				sprintf(out, "movl `s0, %d(`s1)", i);
				emit(AS_Oper(out, NULL, L(src, L(dst, NULL)), NULL));
				return;
			}

			else if (s->u.MOVE.dst->kind == T_MEM &&
				s->u.MOVE.dst->u.MEM->kind == T_BINOP &&
				s->u.MOVE.dst->u.MEM->u.BINOP.op == T_plus &&
				s->u.MOVE.dst->u.MEM->u.BINOP.left->kind == T_CONST) {
				// move (mem(binop(+, const(i), e1)), e2)
				int i = s->u.MOVE.dst->u.MEM->u.BINOP.left->u.CONST;
				Temp_temp dst = munchExp(s->u.MOVE.dst->u.MEM->u.BINOP.right);
				Temp_temp src = munchExp(s->u.MOVE.src);
				char* out = (char*)checked_malloc(STRLEN);
				sprintf(out, "movl `s0, %d(`s1)", i);
				emit(AS_Oper(out, NULL, L(src, L(dst, NULL)), NULL));
				return;
			}

			else if (s->u.MOVE.dst->kind == T_MEM) {
				// move (mem(e1), e2)	// bug fix
				Temp_temp src = munchExp(s->u.MOVE.src);
				Temp_temp dst = munchExp(s->u.MOVE.dst->u.MEM);
				char* out1 = (char *)checked_malloc(STRLEN);
				sprintf(out1, "movl `s0, (`s1)");
				emit(AS_Oper(out1, NULL, L(src, L(dst, NULL)), NULL));
				return;
			}

			else if (s->u.MOVE.src->kind == T_BINOP &&
				s->u.MOVE.src->u.BINOP.op == T_plus &&
				s->u.MOVE.src->u.BINOP.left->kind == T_TEMP &&
				s->u.MOVE.src->u.BINOP.right->kind == T_CONST &&
				s->u.MOVE.dst->kind == T_TEMP) {
				// move(temp, binop(+, temp, const(i))) new
				int i = s->u.MOVE.src->u.BINOP.right->u.CONST;
				Temp_temp src = s->u.MOVE.src->u.BINOP.left->u.TEMP;
				Temp_temp dst = s->u.MOVE.dst->u.TEMP;
				char* out1 = (char *)checked_malloc(STRLEN);
				sprintf(out1, "movl `s0, `d0");
				emit(AS_Move(out1, L(dst, NULL), L(src, NULL)));
				char* out2 = (char *)checked_malloc(STRLEN);
				sprintf(out2, "addl $%d, `d0", i);
				emit(AS_Oper(out2, L(dst, NULL), L(dst, NULL), NULL));
				return;
			}

			else if (s->u.MOVE.src->kind == T_MEM &&
				s->u.MOVE.src->u.MEM->kind == T_BINOP &&
				(s->u.MOVE.src->u.MEM->u.BINOP.op == T_plus ||
					s->u.MOVE.src->u.MEM->u.BINOP.op == T_minus) &&
				s->u.MOVE.src->u.MEM->u.BINOP.right->kind == T_CONST) {
				// move(e2, mem(binop(+/-, e1, const(i))))
				int i = s->u.MOVE.src->u.MEM->u.BINOP.right->u.CONST;
				if (s->u.MOVE.src->u.MEM->u.BINOP.op == T_minus)
					i = -i;
				Temp_temp dst = munchExp(s->u.MOVE.dst);
				Temp_temp src = munchExp(s->u.MOVE.src->u.MEM->u.BINOP.left);
				char* out = (char*)checked_malloc(STRLEN);
				sprintf(out, "movl %d(`s0), `d0", i);
				emit(AS_Oper(out, L(dst, NULL), L(src, NULL), NULL));
				return;
			}

			else if (s->u.MOVE.src->kind == T_MEM &&
				s->u.MOVE.src->u.MEM->kind == T_BINOP &&
				s->u.MOVE.src->u.MEM->u.BINOP.op == T_plus &&
				s->u.MOVE.src->u.MEM->u.BINOP.left->kind == T_CONST) {
				// move(e2, mem(binop(+, const(i), e1)))
				int i = s->u.MOVE.src->u.MEM->u.BINOP.left->u.CONST;
				Temp_temp dst = munchExp(s->u.MOVE.dst);
				Temp_temp src = munchExp(s->u.MOVE.src->u.MEM->u.BINOP.right);
				char* out = (char*)checked_malloc(STRLEN);
				sprintf(out, "movl %d(`s0), `d0", i);
				emit(AS_Oper(out, L(dst, NULL), L(src, NULL), NULL));
				return;
			}

			else if (s->u.MOVE.src->kind == T_MEM &&
					s->u.MOVE.src->u.MEM->kind == T_CONST) {
				// move(e1, mem(const(i)))
				int i = s->u.MOVE.src->u.MEM->u.CONST;
				Temp_temp dst = munchExp(s->u.MOVE.dst);
				char* out = (char*)checked_malloc(STRLEN);
				sprintf(out, "movl %d, `d0", i);
				emit(AS_Oper(out, L(dst, NULL), NULL, NULL));
				return;
			}

			else if (s->u.MOVE.src->kind == T_MEM) {
				// move(e1, mem(e2))
				Temp_temp src = munchExp(s->u.MOVE.src->u.MEM);
				Temp_temp dst = munchExp(s->u.MOVE.dst);
				char* out = (char*)checked_malloc(STRLEN);
				sprintf(out, "movl (`s0), `d0");
				emit(AS_Oper(out, L(dst, NULL), L(src, NULL), NULL));
				return;
			}

			else if (s->u.MOVE.dst->kind == T_MEM) {
				// move(e2, mem(e1))
				Temp_temp src = munchExp(s->u.MOVE.src);
				Temp_temp dst = munchExp(s->u.MOVE.dst->u.MEM);
				char* out = (char*)checked_malloc(STRLEN);
				sprintf(out, "movl `s0, (`s1)");
				emit(AS_Oper(out, NULL, L(src, L(dst, NULL)), NULL));
				return;
			}

			else if (s->u.MOVE.src->kind == T_CONST) {
				// move(e1, const(i))
				int i = s->u.MOVE.src->u.CONST;
				Temp_temp dst = munchExp(s->u.MOVE.dst);
				char* out = (char*)checked_malloc(STRLEN);
				sprintf(out, "movl $%d, `d0", i);
				emit(AS_Oper(out, L(dst, NULL), NULL, NULL));
				return;
			}

			else if (s->u.MOVE.dst->kind == T_TEMP &&
				s->u.MOVE.src->kind == T_BINOP &&
				s->u.MOVE.src->u.BINOP.left->kind == T_TEMP &&
				s->u.MOVE.src->u.BINOP.left->u.TEMP == s->u.MOVE.dst->u.TEMP &&
				s->u.MOVE.src->u.BINOP.right->kind == T_CONST) {
				/* for loop
				**  move(temp a,binop(+, temp a, const 1))
				*/
				int i = s->u.MOVE.src->u.BINOP.right->u.CONST;
				Temp_temp r =  s->u.MOVE.dst->u.TEMP;
				char* out = (char *)checked_malloc(STRLEN);
				sprintf(out, "addl $%d, `d0", i);
				emit(AS_Oper(out, L(r, NULL), NULL, NULL));
				return; 
			}

			else {
				Temp_temp src = munchExp(s->u.MOVE.src);
				Temp_temp dst = munchExp(s->u.MOVE.dst);
				char* out = (char*)checked_malloc(STRLEN);
				sprintf(out, "movl `s0, `d0");
				emit(AS_Move(out, L(dst, NULL), L(src, NULL)));
				return;
			}
		}

		case T_SEQ: {
			assert(0);
		}

		case T_LABEL: {
			char* out = (char*)checked_malloc(STRLEN);
			sprintf(out, "%s", Temp_labelstring(s->u.LABEL));
			//printf("%s\n", out);
			emit(AS_Label(out, s->u.LABEL));
			return;
		}

		case T_JUMP: {
			//printf("jump\n");
			Temp_label label = s->u.JUMP.exp->u.NAME;
			char* out = (char*)checked_malloc(STRLEN);
			sprintf(out, "jmp %s", Temp_labelstring(label));
			emit(AS_Oper(out, NULL, NULL, AS_Targets(s->u.JUMP.jumps)));
			return;
		}

		case T_CJUMP: {
			//printf("cjump\n");
			//struct {T_relOp op; T_exp left, right; Temp_label true, false;} CJUMP;
			char *str = NULL;
			switch (s->u.CJUMP.op) {
				case T_eq: { str = "je"; break;}
				case T_ne: { str = "jne"; break;}
				case T_lt: { str = "jl"; break;}
				case T_gt: { str = "jg"; break;}
				case T_le: { str = "jle" ; break;}
				case T_ge: { str = "jge"; break;}
				case T_ult:
				case T_ule:
				case T_ugt:
				case T_uge: 
					assert(0);
			}
			Temp_temp l = munchExp(s->u.CJUMP.left);
			Temp_temp r = munchExp(s->u.CJUMP.right);
			char* out1 = (char*)checked_malloc(STRLEN);
			sprintf(out1, "cmpl `s1, `s0");
			emit(AS_Oper(out1, NULL, L(l, L(r, NULL)), NULL));
			char* out2 = (char*)checked_malloc(STRLEN);
			sprintf(out2, "%s %s", str, Temp_labelstring(s->u.CJUMP.true));
			emit(AS_Oper(out2, NULL, NULL, AS_Targets(
				Temp_LabelList(s->u.CJUMP.true, Temp_LabelList(s->u.CJUMP.false, NULL)))));
			return;
		}

		case T_EXP: {
			munchExp(s->u.EXP);
			return;
		}

		assert(0);
	}
}
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

//Lab 6: put your code here
static Temp_temp munchExp(T_exp e);
static void munchStm(T_stm s);

static AS_instrList iList = NULL, last = NULL;

static void emit(AS_instr inst) {
	if (last != NULL) {
		last->tail = AS_InstrList(inst, NULL);
		last = last->tail;
	}
	else {
		iList = AS_InstrList(inst, NULL);
		last = iList;
	}
}

static Temp_tempList L(Temp_temp h, Temp_tempList t) {
	return Temp_TempList(h, t);
}

AS_instrList F_codegen(F_frame f, T_stmList stmList) {
	AS_instrList list;
	T_stmList sl;
	// ...
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
	char out[20];
	char* str;
	switch(op) {
		case T_plus: {str = "addl"; break;}
		case T_minus: {str = "subl"; break;}
		case T_mul: {str = "imul"; break;}
		case T_div: {str = "idivl"; break;}
		case T_and:
		case T_or:
		case T_lshift:
		case T_rshift:
		case T_arshift:
		case T_xor:
			assert(0);
	}
	sprintf(out, "movl `s0 `d0\n");
	emit(AS_Oper(out, L(tmp, NULL), L(l, NULL), NULL));
	sprintf(out, "%s `s0, `d0\n", str);
	emit(AS_Oper(out, L(tmp, NULL), L(r, L(tmp, NULL)), NULL));
	return tmp;
}

static Temp_tempList munchArgs(T_expList args) {
	if (args == NULL)
		return NULL;

	else {
		Temp_tempList l = munchArgs(args->tail);
		Temp_temp t = munchExp(args->head);
		emit(AS_Oper("pushl `s0", NULL, L(t, NULL), NULL));
		return L(t, l);
	}
}

static Temp_temp munchExp(T_exp e) {
	char out[30];
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
						sprintf(out, "movl $%d, `d0\n", lv);
						emit(AS_Oper(out, L(tmp, NULL), NULL, NULL));
						sprintf(out, "addl $%d, `d0\n", rv);
						emit(AS_Oper(out, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}

					else if (e->u.BINOP.left->kind == T_CONST) {
						// binop(+, const, e)
						int i = e->u.BINOP.left->u.CONST;
						Temp_temp tmp = Temp_newtemp();
						Temp_temp r = munchExp(e->u.BINOP.right);
						sprintf(out, "movl `s0, `d0\n");
						emit(AS_Oper(out, L(tmp, NULL), L(r, NULL), NULL));
						sprintf(out, "addl $%d, `d0\n", i);
						emit(AS_Oper(out, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}

					else if (e->u.BINOP.right->kind == T_CONST) {
						// binop(+, e, const)
						int i = e->u.BINOP.right->u.CONST;
						Temp_temp tmp = Temp_newtemp();
						Temp_temp l = munchExp(e->u.BINOP.left);
						sprintf(out, "movl `s0, `d0\n");
						emit(AS_Oper(out, L(tmp, NULL), L(l, NULL), NULL));
						sprintf(out, "addl $%d, `d0\n", i);
						emit(AS_Oper(out, L(tmp, NULL), L(tmp, NULL), NULL));
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
						sprintf(out, "movl `s0, `d0\n");
						emit(AS_Oper(out, L(tmp, NULL), L(l, NULL), NULL));
						sprintf(out, "subl $%d, `d0\n", i);
						emit(AS_Oper(out, L(tmp, NULL), L(tmp, NULL), NULL));
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
						sprintf(out, "movl $%d, `d0\n", lv);
						emit(AS_Oper(out, L(tmp, NULL), NULL, NULL));
						sprintf(out, "imul $%d, `d0\n", rv);
						emit(AS_Oper(out, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}

					else if (e->u.BINOP.left->kind == T_CONST) {
						// BINOP(T_mul, CONST, exp);
						int i = e->u.BINOP.left->u.CONST;
						Temp_temp tmp = Temp_newtemp();
						Temp_temp r = munchExp(e->u.BINOP.right);
						sprintf(out, "movl `s0, `d0\n");
						emit(AS_Oper(out, L(tmp, NULL), L(r, NULL), NULL));
						sprintf(out, "imul $%d, `d0\n", i);
						emit(AS_Oper(out, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}

					else if (e->u.BINOP.right->kind == T_CONST) {
						// BINOP(T_mul, CONST, exp);
						int i = e->u.BINOP.right->u.CONST;
						Temp_temp tmp = Temp_newtemp();
						Temp_temp l = munchExp(e->u.BINOP.left);
						sprintf(out, "movl `s0, `d0\n");
						emit(AS_Oper(out, L(tmp, NULL), L(l, NULL), NULL));
						sprintf(out, "imul $%d, `d0\n", i);
						emit(AS_Oper(out, L(tmp, NULL), L(tmp, NULL), NULL));
						return tmp;
					}
					else
						return emitBinOp(e->u.BINOP.left, e->u.BINOP.right, op);
				}

				case T_div: {
					// TO DO
					return emitBinOp(e->u.BINOP.left, e->u.BINOP.right, op);
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
					sprintf(out, "movl %d(`s0), `d0\n", i);
					emit(AS_Oper(out, L(tmp, NULL), L(l, NULL), NULL));
					return tmp;
				}

				else if (binexp->u.BINOP.left->kind == T_CONST && binexp->u.BINOP.op == T_plus) {
					// MEM(BINOP(+, CONST, e))
					Temp_temp r = munchExp(binexp->u.BINOP.right), tmp = Temp_newtemp();
					int i = binexp->u.BINOP.left->u.CONST;
					sprintf(out, "movl %d(`s0), `d0\n", i);
					emit(AS_Oper(out, L(tmp, NULL), L(r, NULL), NULL));
					return tmp;
				}
			}

			else if (e->u.MEM->kind == T_CONST) {
				Temp_temp tmp = Temp_newtemp();
				int i = e->u.MEM->u.CONST;
				sprintf(out, "movl %d, `d0\n", i);
				emit(AS_Oper(out, L(tmp, NULL), NULL, NULL));
				return tmp;
			}

			/* Default */
			Temp_temp tmp = Temp_newtemp(), r = munchExp(e->u.MEM);
			sprintf(out, "movl (`s0), `d0\n");
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
			sprintf(out, "movl $.%s, `d0", Temp_labelstring(e->u.NAME));
			// TO DO: src ??
			emit(AS_Oper(out, L(tmp, NULL), NULL, NULL));
			return tmp;
		}

		case T_CONST: {
			//printf("const\n");
			Temp_temp tmp = Temp_newtemp();
			sprintf(out, "movl $%d, `d0", e->u.CONST);
			emit(AS_Oper(out, L(tmp, NULL), NULL, NULL));
			return tmp;
		}

		case T_CALL: {
			//printf("call\n");
			// TO DO: src and dst
			Temp_temp r = munchExp(e->u.CALL.fun);
			Temp_tempList l = munchArgs(e->u.CALL.args);
			sprintf(out, "call %s\n", Temp_labelstring(e->u.CALL.fun->u.NAME));
			// TO DO
			Temp_tempList calldefs = NULL;
			emit(AS_Oper(out, calldefs, L(r, l), NULL));
			return F_RV()->u.TEMP;
		}
	}
}

static void munchStm(T_stm s) {
	char out[30];
	switch(s->kind) {
		case T_MOVE: {
			//printf("move\n");
			if (s->u.MOVE.dst->kind == T_MEM &&
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
				sprintf(out, "movl `s0, %d(`d0)\n", i);
				emit(AS_Oper(out, L(dst, NULL), L(src, NULL), NULL));
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
				sprintf(out, "movl `s0, %d(`d0)\n", i);
				emit(AS_Oper(out, L(dst, NULL), L(src, NULL), NULL));
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
				sprintf(out, "movl %d(`s0), `d0\n", i);
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
				sprintf(out, "movl %d(`s0), `d0\n", i);
				emit(AS_Oper(out, L(dst, NULL), L(src, NULL), NULL));
				return;
			}

			else if (s->u.MOVE.src->kind == T_MEM &&
					s->u.MOVE.src->u.MEM->kind == T_CONST) {
				// move(e1, mem(const(i)))
				int i = s->u.MOVE.src->u.MEM->u.CONST;
				Temp_temp dst = munchExp(s->u.MOVE.dst);
				sprintf(out, "movl %d, `d0\n", i);
				emit(AS_Oper(out, L(dst, NULL), NULL, NULL));
				return;
			}

			else if (s->u.MOVE.src->kind == T_MEM) {
				// move(e1, mem(e2))
				Temp_temp src = munchExp(s->u.MOVE.src->u.MEM);
				Temp_temp dst = munchExp(s->u.MOVE.dst);
				sprintf(out, "movl (`s0), `d0\n");
				emit(AS_Oper(out, L(dst, NULL), L(src, NULL), NULL));
				return;
			}

			else if (s->u.MOVE.dst->kind == T_MEM) {
				// move(e2, mem(e1))
				Temp_temp src = munchExp(s->u.MOVE.src);
				Temp_temp dst = munchExp(s->u.MOVE.dst->u.MEM);
				sprintf(out, "movl `s0, (`d0)\n");
				emit(AS_Oper(out, L(dst, NULL), L(src, NULL), NULL));
				return;
			}

			else if (s->u.MOVE.src->kind == T_CONST) {
				// move(e1, const(i))
				int i = s->u.MOVE.src->u.CONST;
				Temp_temp dst = munchExp(s->u.MOVE.dst);
				sprintf(out, "movl $%d, `d0\n", i);
				emit(AS_Oper(out, L(dst, NULL), NULL, NULL));
				return;
			}

			else {
				Temp_temp src = munchExp(s->u.MOVE.src);
				Temp_temp dst = munchExp(s->u.MOVE.dst);
				sprintf(out, "movl `s0, `d0\n");
				emit(AS_Move(out, L(dst, NULL), L(src, NULL)));
				return;
			}
		}

		case T_SEQ: {
			assert(0);
		}

		case T_LABEL: {
			//printf("label\n");
			sprintf(out, "%s:\n", Temp_labelstring(s->u.LABEL));
			emit(AS_Label(out, s->u.LABEL));
			return;
		}

		case T_JUMP: {
			//printf("jump\n");
			Temp_label label = s->u.JUMP.exp->u.NAME;
			printf("%s\n", Temp_labelstring(label));
			sprintf(out, "jmp %s\n", Temp_labelstring(label));
			emit(AS_Label(out, AS_Targets(s->u.JUMP.jumps)));
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
			sprintf(out, "cmp `s0`, `s1`\n");
			emit(AS_Oper(out, NULL, L(r, L(r, NULL)), NULL));
			sprintf(out, "%s %s\n", str, Temp_labelstring(s->u.CJUMP.true));
			emit(AS_Oper(out, NULL, NULL, AS_Targets(Temp_LabelList(s->u.CJUMP.true, NULL))));
			return;
		}

		case T_EXP: {
			munchExp(s->u.EXP);
			return;
		}

		assert(0);
	}
}
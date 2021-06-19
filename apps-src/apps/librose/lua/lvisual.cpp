/*
** $Id: ldump.c $
** save precompiled Lua chunks
** See Copyright Notice in lua.h
*/

#define lvisual_c
#define LUA_CORE

#include <SDL.h> // linker/include/SDL2/SDL_stdinc.h:426:5: Use of undeclared identifier 'memset_pattern4'

#include "lprefix.h"

#include <stddef.h>

#include "lua.h"

#include "lobject.h"
#include "lstate.h"
#include "lundump.h"

#include "lopcodes.h"
#include "lopnames.h"

#include "util.hpp"
#include <SDL_log.h>
#include <iomanip>
#include "wml_exception.hpp"

typedef struct {
  lua_State *L;
  lua_Writer writer;
  void *data;
  int level;
} DumpState;


struct tincrease_lock
{
    tincrease_lock(DumpState& D)
        : D(D)
    {
        D.level ++;
    }
    ~tincrease_lock()
    {
        D.level --;
    }

    DumpState& D;
};

static std::string indent(DumpState *D)
{
    std::stringstream ss;
    for (int i = 0; i < D->level; ++i) {
        ss << '\t';
    }
    return ss.str();
}

static void to_log_data(DumpState *D, const char* data, int size)
{
    if (size > 0) {
        lua_unlock(D->L);
        const std::string prefix = indent(D);
        if (!prefix.empty()) {
            (*D->writer)(D->L, prefix.c_str(), prefix.size(), D->data);
        }
        (*D->writer)(D->L, data, size, D->data);
        lua_lock(D->L);
  }
}

static void to_log_str_str(DumpState *D, const std::string& key, const std::string& val)
{
    std::stringstream ss;
    ss << key << ": " << val << "\n";
    to_log_data(D, ss.str().c_str(), ss.str().size());
}

static void to_log_str_int64(DumpState *D, const std::string& key, int64_t val)
{
    std::stringstream ss;
    ss << key << ": " << val << "\n";
    to_log_data(D, ss.str().c_str(), ss.str().size());
}

std::string TSTRING_2_str(const TString *s)
{
  if (s == NULL) {
    return null_str;
  }
  size_t size = tsslen(s);
  const char *str = getstr(s);
  return std::string(str, size);
}

static void to_log_str_TSTRING(DumpState *D, const std::string& key, const TString *s)
{
    to_log_str_str(D, key, TSTRING_2_str(s));
}



static void to_log_instruction(DumpState* D, int index, Instruction op, int lineno)
{
    int opcode = GET_OPCODE(op);
    // lu_byte mode = luaP_opmodes[opcode];

    std::stringstream ss;
    ss << std::setiosflags(std::ios::left);
    ss << std::setw(6) << index << "L#";
    ss << std::setw(4) << lineno;
    char buf[32];
    SDL_snprintf(buf, sizeof(buf), "%s(%i)", opnames[opcode], opcode);
    ss << std::setw(15) << buf;

    // enum OpMode {iABC, iABx, iAsBx, iAx, isJ};  /* basic instruction formats */
    static const char *const rose_opmodenames[] = {
        "iABC", "iABx", "iAsBx", "iAx", "isJ", NULL};
    int op3mode = getOpMode(opcode);
    SDL_snprintf(buf, sizeof(buf), "(%s)", rose_opmodenames[op3mode]);
    ss << std::setw(9) << buf;
    switch (op3mode) {
    case iABC:
        ss << "[A]" << GETARG_A(op) << " ";
        ss << "[k]" << GETARG_k(op) << " ";
        ss << "[B]" << GETARG_B(op) << " ";
        ss << "[C]" << GETARG_C(op);
        break;
    case iABx:
        ss << "[A]" << GETARG_A(op) << " ";
        ss << "[Bx]" << GETARG_Bx(op);
        break;
    case iAsBx:
        ss << "[A]" << GETARG_A(op) << " ";
        ss << "[sBx]" << GETARG_sBx(op);
        break;
    case iAx:
        ss << "[Ax]" << GETARG_Ax(op);
        break;
    case isJ:
        ss << "[sJ]" << GETARG_sJ(op);
        break;
    default:
        break;
    }


    ss << "\n";

    to_log_data(D, ss.str().c_str(), ss.str().size());
}

static void dumpCode (DumpState *D, const Proto *f)
{
    to_log_str_int64(D, "sizecode", f->sizecode);
    int asfar_last = 0;
    for (int n = 0; n < f->sizecode; n ++) {
        Instruction op = f->code[n];
        to_log_instruction(D, n, op, f->linedefined + asfar_last + f->lineinfo[n]);
        asfar_last += f->lineinfo[n];
    }
}


static void to_log_constant(DumpState* D, int index, const TValue *o)
{
    std::stringstream ss;

    ss << std::setw(3) << index << "    ";
    int tt = ttypetag(o);
    switch (tt) {
    case LUA_VNUMFLT:
        ss << "[VNUMFLT]" << fltvalue(o);
        break;
    case LUA_VNUMINT:
        ss << "[VNUMINT]" << ivalue(o);
        break;
    case LUA_VSHRSTR:
        ss << "[VSHRSTR]" << TSTRING_2_str(tsvalue(o));
        break;
    case LUA_VLNGSTR:
        ss << "[VLNGSTR]" << TSTRING_2_str(tsvalue(o));
        break;
    case LUA_VNIL:
        ss << "[NIL]";
        break;
    default:
        lua_assert(tt == LUA_VFALSE || tt == LUA_VTRUE);
        ss << (tt == LUA_VFALSE? "[VFALSE]": "[VTRUE]");
    }

    ss << "\n";
    to_log_data(D, ss.str().c_str(), ss.str().size());
}

static void dumpConstants (DumpState *D, const Proto *f) {
  int i;
  int n = f->sizek;
  to_log_str_int64(D, "sizek", n);
  for (i = 0; i < n; i++) {
    const TValue *o = &f->k[i];
    to_log_constant(D, i, o);
  }
}

static void dumpFunction (DumpState *D, const Proto *f, const std::string& tag);

static void dumpProtos (DumpState *D, const Proto *f)
{
    int i;
    int n = f->sizep;
    to_log_str_int64(D, "sizep", n);
    for (i = 0; i < n; i++) {
        tincrease_lock lock(*D);
        std::stringstream tag_ss;
        tag_ss << "[" << i + 1 << "/" << n << "]";
        dumpFunction(D, f->p[i], tag_ss.str());
    }
}

static void to_log_Upvaldesc(DumpState* D, int index, const Upvaldesc& upval)
{
    std::stringstream ss;

    ss << std::setw(3) << index << "    ";
    ss << TSTRING_2_str(upval.name) << "  ";
    ss << "[instack]" << (upval.instack? "true": "false") << "  ";
    ss << "[idx]" << (int)(upval.idx) << "  ";
    ss << "[kind]" << (int)(upval.kind);

    ss << "\n";
    to_log_data(D, ss.str().c_str(), ss.str().size());
}

static void dumpUpvalues (DumpState *D, const Proto *f) {
  int i, n = f->sizeupvalues;
  to_log_str_int64(D, "sizeupvalues", n);
  for (i = 0; i < n; i++) {
      to_log_Upvaldesc(D, i, f->upvalues[i]);
  }
}

static void to_log_lineinfo(DumpState* D, int index, int lineinfo, int asfar_last)
{
    std::stringstream ss;

    ss << std::setw(3) << index << "    ";
    ss << lineinfo << "->" << (asfar_last + lineinfo);

    ss << "\n";
    to_log_data(D, ss.str().c_str(), ss.str().size());
}

static void to_log_abslineinfo(DumpState* D, int index, const AbsLineInfo& lineinfo)
{
    std::stringstream ss;

    ss << std::setw(3) << index << "    ";
    ss << "[pc]" << (int)(lineinfo.pc) << "  ";
    ss << "[line]" << (int)(lineinfo.line);

    ss << "\n";
    to_log_data(D, ss.str().c_str(), ss.str().size());
}

static void to_log_LocVar(DumpState* D, int index, const LocVar& var)
{
    std::stringstream ss;

    ss << std::setw(3) << index << "    ";
    ss << TSTRING_2_str(var.varname) << "  ";
    ss << "[startpc]" << var.startpc << "  ";
    ss << "[endpc]" << var.endpc;

    ss << "\n";
    to_log_data(D, ss.str().c_str(), ss.str().size());
}

static void to_log_name(DumpState* D, int index, int lineinfo)
{
    std::stringstream ss;

    ss << std::setw(3) << index << "    ";
    ss << lineinfo;

    ss << "\n";
    to_log_data(D, ss.str().c_str(), ss.str().size());
}

static void dumpDebug (DumpState *D, const Proto *f) {
  int i, n;
/*
  n = f->sizelineinfo;
  to_log_str_int64(D, "sizelineinfo", n);
  int asfar_last = 0;
  for (i = 0; i < n; i ++) {
      to_log_lineinfo(D, i, f->lineinfo[i], asfar_last);
      asfar_last += f->lineinfo[i];
  }
*/
  n = f->sizeabslineinfo;
  to_log_str_int64(D, "sizeabslineinfo", n);
  for (i = 0; i < n; i++) {
      to_log_abslineinfo(D, i, f->abslineinfo[i]);
  }

  n = f->sizelocvars;
  to_log_str_int64(D, "sizelocvars", n);
  for (i = 0; i < n; i++) {
      to_log_LocVar(D, i, f->locvars[i]);
  }
}

static void dumpFunction(DumpState *D, const Proto *f, const std::string& tag)
{
    std::stringstream start_ss;
    start_ss << tag << "---source:" << TSTRING_2_str(f->source) << "---\n";
    to_log_data(D, start_ss.str().c_str(), start_ss.str().size());

    to_log_str_int64(D, "linedefined", f->linedefined);
    to_log_str_int64(D, "lastlinedefined", f->lastlinedefined);
    to_log_str_int64(D, "numparams", f->numparams);
    to_log_str_int64(D, "is_vararg", f->is_vararg);
    to_log_str_int64(D, "maxstacksize", f->maxstacksize);

    dumpCode(D, f);
    dumpConstants(D, f);
    dumpUpvalues(D, f);
    dumpProtos(D, f);
    dumpDebug(D, f);

    const std::string end_str = "============\n\n";
    to_log_data(D, end_str.c_str(), end_str.size());
}

/*
** dump Lua function as precompiled chunk
*/
static void luaU_visual(lua_State *L, const Proto *f, lua_Writer w, void *data)
{
  DumpState D;
  D.L = L;
  D.writer = w;
  D.data = data;
  D.level = 0;
  dumpFunction(&D, f, null_str);
}

LUA_API int lua_visual(lua_State *L, lua_Writer writer, void *data)
{
  int status = 1;
  TValue *o;
  lua_lock(L);
  // api_checknelems(L, 1);
  o = s2v(L->top - 1);
  if (isLfunction(o)) {
    luaU_visual(L, getproto(o), writer, data);
    status = 0;
  }
  lua_unlock(L);
  return status;
}

#include "ltable.h"
// extern const Table* main_lexstate_h;

void lua_visual_Table(lua_State *L, Table* t)
{
    int nsize = sizenode(t);
    // SDL_Log("======table(%p)'s sizenode:%i======main_lexstate_h(%p)", t, nsize, main_lexstate_h);
    SDL_Log("======table(%p)'s sizenode:%i======", t, nsize);
    for (int i = 0; i < nsize; i++) {
        std::string key;
        Node *n = gnode(t, i);
        TValue k;
        getnodekey(L, &k, n);
        if (ttypetag(&k) == LUA_VSHRSTR) {
            TString* ts = tsvalue(&k);
            key = TSTRING_2_str(ts);
        } else if (ttypetag(&k) == LUA_TNONE) {
            key = "LUA_TNONE";
        } else if (ttypetag(&k) == LUA_TNIL) {
            key = "LUA_TNIL";
        } else {
            key = "key's type isn't LUA_VSHRSTR";
        }

        char value_buf[32];
        std::string value_str;
        TValue* val = gval(n);
        if (ttype(val) == LUA_TBOOLEAN) {
            value_str = "LUA_TBOOLEAN";
        } else if (ttype(val) == LUA_TLIGHTUSERDATA) {
            value_str = "LUA_TLIGHTUSERDATA";
        } else if (ttype(val) == LUA_TNUMBER) {
            if (ttypetag(val) == LUA_VNUMINT) {
                value_str = str_cast(ivalue(val));
            } else {
                double f = fltvalue(val);
                SDL_snprintf(value_buf, sizeof(value_buf), "%f", f);
                value_str = value_buf;
            }
        } else if (ttype(val) == LUA_TSTRING) {
            if (ttypetag(val) == LUA_VSHRSTR) {
                TString* ts = tsvalue(val);
                value_str = TSTRING_2_str(ts);
            } else {
                value_str = "LUA_TSTRING";
            }
        } else if (ttype(val) == LUA_TTABLE) {
            value_str = "LUA_TTABLE";
        } else if (ttype(val) == LUA_TFUNCTION) {
            value_str = "LUA_TFUNCTION";
        } else if (ttype(val) == LUA_TUSERDATA) {
            value_str = "LUA_TUSERDATA";
        } else if (ttype(val) == LUA_TTHREAD) {
            value_str = "LUA_TTHREAD";
        } else if (ttype(val) == LUA_TNIL) {
            value_str = "LUA_TNIL";
        } else {
            value_str = str_cast(ttype(val));
        }
        SDL_Log("[%u/%u]%s --> (%p)%s", i, nsize, key.c_str(), val, value_str.c_str());
    }
    SDL_Log("=============================");
}

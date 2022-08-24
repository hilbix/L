/*
 * This is free software as in free beer, free speech and free baby.
 *
 * GCCFLAGS:	-Wno-unused-function
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* Structures ********************************************************/
/* Structures ********************************************************/
/* Structures ********************************************************/

typedef struct L	*L;

typedef	union Lval	*Lval;
typedef	struct Lptr	*Lptr;
typedef	struct Lbuf	*Lbuf;
typedef	struct Lnum	*Lnum;
typedef	struct Lrun	*Lrun;
typedef struct Lio	*Lio;
typedef struct Llist	*Llist;
typedef struct Ltmp	*Ltmp;

typedef struct Lstack	*Lstack;
typedef	struct Lmem	*Lmem;
typedef union Larg	Larg;

typedef void (		*Lfn)(Lrun, Larg);

struct L
  {
    Lstack		stack[1+'Z'-'A'+1];
    Lstack		run, wait, end;
    Lstack		unused;
    Lval		use, free, overuse;

    Lio			read, write;
    Lio			*io;
    int			ios;

    void		*user;

    unsigned		allocated:1;
  };

struct Lptr
  {
    L			_;
    Lval		next, *prev;
    unsigned		type:4, sub:4;
    unsigned		size:8;
    unsigned		use:15;
    unsigned		overuse:1;
  };

struct Lbuf	/* immutable (after created)	*/
  {
    struct Lptr		ptr;
    Lmem		first, last;
    size_t		total;
  };

struct Lnum	/* immutable (after created)	*/
  {
    struct Lptr		ptr;
    long long		num;	/* WRONG!  This must become BigNum!	*/
  };

struct Lrun	/* immutable (after created)	*/
  {
    struct Lptr		ptr;
    struct Lstep	*steps;
    int			pos, cnt;
  };

struct Lio	/* global	*/
  {
    struct Lptr		ptr;
    Lio			next;
    Lbuf		buf;		/* Data kept in the buffer	*/
    int			pos;		/* position in buf		*/
    int			fd;
    unsigned		write:1;	/* write set = output; else input	*/
    unsigned		open:1;		/* open set = need close() on free, else must not close() nor free() on free	*/
    unsigned		eof:1;		/* eof set = stream is on EOF or error	*/
  };

struct Llist	/* mutable buffer	*/
  {
    struct Lptr		ptr;
    Lmem		first;
    int			num;
  };

struct Ltmp	/* mutable extensible data buffer	*/
  {
    struct Lptr		ptr;
    int			pos;
    int			size;
    char		*buf;
  };

/* Value
 * value -> type
 */
union Lval
  {
    struct	Lptr	ptr;
    struct	Lbuf	buf;	/* immutable	*/
    struct	Lnum	num;	/* immutable	*/
    struct	Lrun	run;	/* immutable	*/
    struct	Lio	io;	/* immutable	*/
    struct	Llist	list;	/* mutable	*/
    struct	Ltmp	tmp;	/* mutable	*/
  };

/* Argument (pointer to Value)
 * This is used a bit like the inverse to Lval:
 * type -> value
 */
union Larg
  {
    int			i;
    unsigned		c;

    Lval		val;

    Lptr		ptr;
    Lbuf		buf;
    Lnum		num;
    Lrun		run;
    Lio			io;
    Llist		list;
    Ltmp		ltmp;
  };

struct Lstack
  {
    Lstack		next;
    Larg		arg;
  };

struct Lstep
  {
    Lfn		fn;
    Larg	arg;
  };

struct Lmem
  {
    struct Lmem		*next;
    size_t		len;
    char		data[0];
  };


/* Formatting ********************************************************/
/* Formatting ********************************************************/
/* Formatting ********************************************************/

typedef struct FormatArg
  {
    const char	*s;
    va_list	l;
  } FormatArg;

typedef struct Format
  {
    void	(*out)(struct Format *, const void *, size_t len);
    int		fd;
    void	*user;
    char	buf[256];
  } Format;

enum
  {
    F_NULL,
    F_A,
    F_U,
    F_I,
    F_C,
    F_V,
  };


/* Auxiliary *********************************************************/
/* Auxiliary *********************************************************/
/* Auxiliary *********************************************************/

#define	FORMAT_A(A)	(char *)F_A, &(A)
#define	FORMAT_U(U)	(char *)F_U, (unsigned long long)(U)
#define	FORMAT_I(I)	(char *)F_I, (long long)(I)
#define	FORMAT_C(C)	(char *)F_C, (unsigned)(C)
#define	FORMAT_V(V)	(char *)F_V, V

static void Loops(L, const char *, ...);
#define	LFATAL(X,...)	do { if (X) Loops(NULL, "FATAL ERROR: ", __FILE__, " ", FORMAT_I(__LINE__), " ", __func__, ": ", #X, ##__VA_ARGS__, NULL); } while (0)

enum Ltype
  {
    LPTR,
    /* immutable types	*/
    LBUF,
    LNUM,
    LRUN,
    LIO,
    /* mutable types	*/
    LLIST,
    LTMP,
  };

static const char * const Ltypes[] =
  {
    "PTR",
    "BUF",
    "NUM",
    "RUN",
    "IO",
    "LIST",
    "TMP",
  };

static const int LSIZE[] =
  {
    sizeof (struct Lptr),
    sizeof (struct Lbuf),
    sizeof (struct Lnum),
    sizeof (struct Lrun),
    sizeof (struct Lio),
    sizeof (struct Llist),
    sizeof (struct Ltmp),
  };

static Lstack Lstack_new(L);

/* never call this yourself!	*/
static void _Lptr_free(Lptr ptr) { LFATAL(ptr, "unsupported call to free()"); }
static void _Lbuf_free(Lbuf);
static void _Lnum_free(Lnum);
static void _Lrun_free(Lrun);
static void _Lio_free(Lio);
static void _Llist_free(Llist);
static void _Ltmp_free(Ltmp);

static void (* const LFREE[])() =
  {
    _Lptr_free,
    _Lbuf_free,
    _Lnum_free,
    _Lrun_free,
    _Lio_free,
    _Llist_free,
    _Ltmp_free,
  };

static int
write_all(int fd, const void *s, size_t len)
{
  int	pos;

  pos	= 0;
  while (pos<len)
    {
      int	put;

      put	= write(fd, ((char *)s)+pos, len-pos);
      if (put<=0)
        break;		/* EOF	*/
      pos	+= put;
    }
  return pos;
}

static void
writer(Format *f, const void *s, size_t len)
{
  write_all(f->fd, s, len);
}

static void
FORMAT(struct Format *f, struct FormatArg *v)
{
  const char	*s;

  for (s=v->s; s; s=va_arg(v->l, const char *))
    {
      switch ((unsigned long long)s)
        {
        unsigned u;

        case F_A:	FORMAT(f, va_arg(v->l, struct FormatArg *)); continue;
        case F_U:	s=f->buf; snprintf(f->buf, sizeof f->buf, "%llu", va_arg(v->l, unsigned long long));	break;
        case F_I:	s=f->buf; snprintf(f->buf, sizeof f->buf, "%lld", va_arg(v->l, long long));		break;
        case F_C:	s=f->buf; u = va_arg(v->l, unsigned); snprintf(f->buf, sizeof f->buf, "%c(%02x)", isprint(u) ? u : '.', u);			break;
        case F_V:	s=Ltypes[va_arg(v->l, Lval)->ptr.type];							break;
        }
      f->out(f, s, strlen(s));
    }
}

#define	FORMAT_INIT(F,OUT,FD,U)	do { F.out = OUT; F.fd = FD; F.user = U; } while (0)
#define	FORMAT_START(A,S)	do { A.s = S; va_start(A.l, S); } while (0)
#define	FORMAT_END(A)		do { va_end(A.l); } while (0)

static L
L_stderr(L _, const char *s, ...)
{
  FormatArg	a;
  Format	f;

  FORMAT_INIT(f, writer, 2, _);
  FORMAT_START(a, s);
  FORMAT(&f, &a);
  FORMAT_END(a);
  return _;
}

static void
Loops(L _, const char *s, ...)
{
  int			e=errno;
  struct FormatArg	a;

  FORMAT_START(a, s);
  L_stderr(_, "OOPS: ", FORMAT_A(a), NULL);
  FORMAT_END(a);
  if (e)
    L_stderr(_, ": ", strerror(errno), NULL);
  L_stderr(_, "\n", NULL);
  exit(23); abort(); for(;;);
}

static void
Lunknown(Lrun run, const char *s, ...)
{
  struct FormatArg	a;

  FORMAT_START(a, s);
  L_stderr(run->ptr._, "unknown operation: ", FORMAT_A(a), NULL);
  FORMAT_END(a);
}


/* Memory ************************************************************/
/* Memory ************************************************************/
/* Memory ************************************************************/
/* XXX TODO XXX this must be improved	*/

static void *
Lrealloc(L _, void *ptr, size_t len)
{
  void	*r;

  r	= ptr ? realloc(ptr, len) : malloc(len);
  if (!r)
    Loops(_, "out of memory");
  return r;
}

static void *
Lalloc(L _, size_t len)
{
  return Lrealloc(_, NULL, len);
}

static void *
Lalloc0(L _, size_t len)
{
  void	*p;

  p	= Lalloc(_, len);
  memset(p, 0, len);
  return p;
}

static L
L_free(L _, void *ptr)
{
  free(ptr);
  return _;
}


/* New+Free **********************************************************/
/* New+Free **********************************************************/

static Lval
Lval_new(L _, enum Ltype type)
{
  Lval	ptr;

  ptr	= Lalloc0(_, LSIZE[type]);
  ptr->ptr._	= _;
  ptr->ptr.use	= 1;
  ptr->ptr.type	= type;
  return ptr;
}

static Lbuf	Lbuf_new(L _)	{ return &Lval_new(_, LBUF )->buf;  }
static Lnum	Lnum_new(L _)	{ return &Lval_new(_, LNUM )->num;  }
static Lrun	Lrun_new(L _)	{ return &Lval_new(_, LRUN )->run;  }
static Lio	Lio_new(L _)	{ return &Lval_new(_, LIO  )->io;   }
static Llist	Llist_new(L _)	{ return &Lval_new(_, LLIST)->list; }
static Ltmp	Ltmp_new(L _)	{ return &Lval_new(_, LTMP )->tmp;  }

static void _Lnum_free(Lnum _) { }

#if 0
static void _Lrun_free(Lrun _) { }
static void _Lio_free (Lio  _) { }
#endif


/* Setup *************************************************************/
/* XXX TODO XXX this is terribly incomplete	*/

static L
L_init(L _, void *user)
{
  int	alloc;
  int	i;
  Lio	io;

  if (!_)
    {
      _		= Lalloc(_, sizeof *_);
      alloc	= 1;
    }
  else
    alloc	= _->allocated;
  memset(_, 0, sizeof *_);
  _->allocated	= alloc;
  _->user	= user;

  /* setup IO
   */
  _->read	= Lio_new(_);
  /* everything set to 0 already which is perfect here	*/

  _->write = io	= Lio_new(_);
  io->fd	= 1;
  io->write	= 1;

  /* initialize registers A to Z	*/
  for (i=sizeof _->stack/sizeof *_->stack; --i>0; )
    _->stack[i]	= Lstack_new(_);

  return _;
}

static int
Lexit(L _)
{
  /* XXX TODO XXX implement this
  */
  return 0;
}


/* Lnum **************************************************************/
/* Lnum **************************************************************/

static Lnum Lnum_set_c(Lnum _, char c)			{ _->num	= (unsigned char)c; return _; }
static Lnum Lnum_set_s(Lnum _, short v)			{ _->num	= v; return _; }
static Lnum Lnum_set_i(Lnum _, int v)			{ _->num	= v; return _; }
static Lnum Lnum_set_l(Lnum _, long v)			{ _->num	= v; return _; }
static Lnum Lnum_set_u(Lnum _, unsigned v)		{ _->num	= v; return _; }
static Lnum Lnum_set_ll(Lnum _, long long v)		{ _->num	= v; return _; }
static Lnum Lnum_set_ul(Lnum _, unsigned long v)	{ _->num	= v; return _; }
static Lnum Lnum_set_ull(Lnum _, unsigned long long v)	{ _->num	= v; if (_->num<0) Loops(_->ptr._, "number too large: ", FORMAT_U(v), NULL); return _; }
static Lnum Lnum_set_size(Lnum _, size_t len)		{ return Lnum_set_ull(_, (unsigned long long)len); }


/* Lmem **************************************************************/
/* Lmem **************************************************************/

static Lmem
Lmem_dup(L _, const void *ptr, size_t len)
{
  struct Lmem	*mem;

  mem		= Lalloc(_, len+sizeof *mem);
  mem->next	= 0;
  mem->len	= len;
  memcpy(mem->data, ptr, len);
  return mem;
}

static Lmem
Lmem_new(L _, size_t len)
{
  struct Lmem	*mem;

  mem		= Lalloc0(_, len+sizeof *mem);
  mem->len	= len;
  return mem;
}

static L
L_mem_free(L _, Lmem mem)
{
  return L_free(_, mem);
}


/* Lval Lptr inc/dec *************************************************/
/* Lval Lptr inc/dec *************************************************/

/* Never call this yourself if you are unsure
 * Leave that to _dec() and _inc()
 */
static L
L_ptr_free(Lptr ptr)
{
  L	_ = ptr->_;

  *ptr->prev	= ptr->next;
  if (ptr->next)
    ptr->next->ptr.prev	= ptr->prev;
  LFREE[ptr->type](_);
  L_free(_, ptr);
  return _;
}

static Lptr
Lptr_move(Lptr _, Lval *list)
{
  Lval	next, *prev;

  next			= _->next;
  prev			= _->prev;
  if (prev)
    *prev		= next;
  if (next)
    next->ptr.prev	= prev;

  _->prev		= list;
  next			= *list;
  _->next		= next;
  if (next)
    next->ptr.prev	= &_->next;
  return _;
}

/* inc reference count
 * This moves to overuse-list if count goes too high
 */
static Lptr
Lptr_inc(Lptr _)
{
  if (_->overuse)
    return _;
  if (!_->use)
    Lptr_move(_, &_->_->use);
  if (++_->use)
    return _;
  _->use	= 1;
  _->overuse	= 1;
  return Lptr_move(_, &_->_->overuse);
}

/* dec reference count
 * This moves it to freelist if count drops to 0
 */
static Lptr
Lptr_dec(Lptr _)
{
  if (_->overuse)
    return _;
  if (--_->use)
    return _;
  return Lptr_move(_, &_->_->free);	/* real free done in L_step()	*/
  return 0;
}

static Lbuf Lbuf_dec(Lbuf _) { Lptr_dec(&_->ptr); return _; }
static Lnum Lnum_dec(Lnum _) { Lptr_dec(&_->ptr); return _; }
static Lrun Lrun_dec(Lrun _) { Lptr_dec(&_->ptr); return _; }
static Lio  Lio_dec (Lio  _) { Lptr_dec(&_->ptr); return _; }

static Lval
Lval_dec(Lval _)
{
  if (_)
    Lptr_dec(&_->ptr);
  return _;
}

static Lval
Lval_inc(Lval _)
{
  Lptr_inc(&_->ptr);
  return _;
}

/* push+pop **********************************************************/

static Lstack
Lstack_new(L _)
{
  Lstack	s;

  s	= _->unused;
  if (s)
    _->unused	= s->next;
  else
    s	= Lalloc(_, sizeof *s);
  s->next	= 0;
  s->arg.val	= 0;
  return s;
}

static Lptr
Lptr_push_stack_dec(Lptr v, Lstack *stack)
{
  Lstack	s;

  s		= Lstack_new(v->_);
  s->arg.ptr	= v;
  s->next	= *stack;
  *stack	= s;
  return v;
}

static Lval
Lval_pop_stack_inc(L _, Lstack *stack)
{
  Lstack	s;
  Lval		v;

  s		= *stack;
  if (!s)
    return 0;
  *stack	= s->next;
  v		= s->arg.val;
  s->arg.val	= 0;

  s->next	= _->unused;
  _->unused	= s;

  return v;
}

static Lval
Lval_push_dec(Lval v, int nr)
{
  Lptr_push_stack_dec(&v->ptr, &v->ptr._->stack[nr]);
  return v;
}

static Lval
Lval_pop_inc(L _, int nr)
{
  return Lval_pop_stack_inc(_, &_->stack[nr]);
}

static Lval
Ldec(Larg _)
{
  Lptr_dec(&_.val->ptr);
  return _.val;
}

static Lval
Linc(Larg _)
{
  Lptr_inc(&_.val->ptr);
  return _.val;
}

/* This includes the call to Lptr_inc()
 */
static Lval
Lval_push_inc(Lval v, int nr)
{
  return Lval_push_dec(Lval_inc(v), nr);
}

/* This include a call to Lptr_dec()
 */
static Lval
Lval_pop_dec(L _, int nr)
{
  return Lval_dec(Lval_pop_inc(_, nr));
}

static Lval
Lpush_inc(Lval v)
{
  return Lval_push_inc(v, 0);
}

static Lval
Lpush_dec(Lval v)
{
  return Lval_push_dec(v, 0);
}

static Lval
Lpop_inc(L _)
{
  return Lval_pop_inc(_, 0);
}

static Lval
Lpop_dec(L _)
{
  return Lval_pop_dec(_, 0);
}

#if 0
static void
Lpush_arg_inc(L _, Larg a)
{
  Lpush_inc(a.val);
}

static void
Lpush_arg_dec(L _, Larg a)
{
  Lpush_dec(a.val);
}

static Lptr
Lptr_push(Lptr _, Lstack *stack)
{
  return _;
}
#endif

/* Lbuf **************************************************************/
/* Lbuf **************************************************************/

/* adds (appends) the memory buffer to buf
 */
static Lmem
Lbuf_mem_put(Lbuf _, Lmem mem)
{
  LFATAL(!mem || mem->next);
  if (!_->last)
    {
      LFATAL(_->first);
      _->first	= _->last = mem;
    }
  else
    {
      LFATAL(_->last->next);
      _->last->next	= mem;
      _->last		= mem;
    }
  _->total	+= mem->len;
  return mem;
}

/* extracts (removes) the first memory buffer from buf
 */
static Lmem
Lbuf_mem_get(Lbuf _)
{
  Lmem	mem;

  mem	= _->first;
  LFATAL(!mem);
  _->total	=- mem->len;
  _->first	= mem->next;
  if (!_->first)
    {
      LFATAL(_->last != mem || _->total);
      _->last	= 0;
    }
  mem->next	= 0;
  return mem;
}

static void
_Lbuf_free(Lbuf buf)
{
  L	_ = buf->ptr._;

  while (buf->first)
    L_mem_free(_, Lbuf_mem_get(buf));
  LFATAL(buf->total || buf->first || buf->last);
}

static Lmem
Lbuf_mem_new(Lbuf _, size_t len)
{
  return Lbuf_mem_put(_, Lmem_new(_->ptr._, len));
}


static Lbuf
Lbuf_add_ptr(Lbuf _, const void *ptr, size_t len)
{
  memcpy(Lbuf_mem_new(_, len)->data, ptr, len);
  return _;
}

static Lbuf
Lbuf_add_buf(Lbuf _, Lbuf buf)
{
  Lmem		mem, m;
  size_t	offset;

  mem	= Lbuf_mem_new(_, buf->total);
  offset= 0;
  for (m=buf->first; m; m=m->next)
    {
      memcpy(mem->data+offset, m->data, m->len);
      offset	+= m->len;
    }
  LFATAL(offset != buf->total);
  return _;
}

static Lbuf
Lbuf_move_mem(Lbuf _, Lbuf src)
{
  Lbuf_mem_put(_, Lbuf_mem_get(src));
  return _;
}

static Lbuf
Lbuf_add_readn(Lbuf _, int fd, int max)
{
  char	buf[BUFSIZ];
  int	loops;

  if (max<=0 || max>=sizeof buf)
    max	= sizeof buf;
  for (loops=0; loops<10000; loops++)
    {
      int got;

      got	= read(fd, buf, (size_t)max);
      if (got>0)
        return Lbuf_add_ptr(_, buf, got);
      if (!got)
        return 0;
      if (errno != EAGAIN)
        Loops(_->ptr._, "read error on input", NULL);
    }
  Loops(_->ptr._, "too many loops", NULL);
  return 0;
}

static Lbuf
Lbuf_add_readall(Lbuf _, int fd)
{
  while (Lbuf_add_readn(_, fd, 0));
  return _;
}

static void
Lbuf_writer(Format *f, const void *s, size_t len)
{
  Lbuf	_ = f->user;

  LFATAL(_->ptr.type != LBUF);
  Lbuf_add_ptr(_, s, len);
}

static Lbuf
Lbuf_add_format(Lbuf _, const char *s, ...)
{
  FormatArg	a;
  Format	f;

  FORMAT_INIT(f, Lbuf_writer, 2, _);
  FORMAT_START(a, s);
  FORMAT(&f, &a);
  FORMAT_END(a);
  return _;
}

/* toString(value)     */
static Lbuf
Lbuf_from_val(Lval v)
{
  L    _ = v->ptr._;

  switch (v->ptr.type)
    {
    case LNUM: return Lbuf_add_format(Lbuf_dec(Lbuf_new(_)), FORMAT_I(v->num.num), NULL);
    case LBUF: return &v->buf;
    }
  return 0;
}

static Lbuf
Lbuf_add_tmp(Lbuf _, Ltmp tmp, int (*proc)(Ltmp, void *buf, int max))
{
  int	len;
  Lmem	mem;

  len	= proc(tmp, NULL, 0);
  mem	= Lbuf_mem_new(_, len);
  proc(tmp, mem->data, len);
  return _;
}


/* Liter *************************************************************/

typedef struct Liter
  {
    Lbuf	buf;
    Lmem	mem;
    size_t	cnt;
    int		pos;
  } *Liter;

static Lbuf
Lbuf_iter(Lbuf _, Liter l)
{
  l->buf	= _;
  l->mem	= _->first;
  l->cnt	= _->total;
  l->pos	= 0;
  return _;
}

static int
Liter_getc(Liter _)
{
  unsigned	c;	/* we perhaps allow buffers to return Unicode in some future	*/
  Lmem		mem;

  if (!_->cnt)
    return -1;

  mem	= _->mem;
  c	= mem->data[_->pos];
  if (++_->pos >= mem->len)
    {
      _->mem	= mem->next;
      _->pos	= 0;
    }
  _->cnt--;
  return c; 
}

/* Llist *************************************************************/
/* Llist *************************************************************/

static void
_Llist_free(Llist list)
{
  L	_ = list->ptr._;
  Lmem	mem;

  while ((mem = list->first) != 0)
    {
      list->first	= mem->next;
      L_mem_free(_, mem);
      list->num--;
    }
  LFATAL(list->num);
}

static Llist
Llist_push(Llist list, const void *ptr, size_t len)
{
  L	_ = list->ptr._;
  Lmem	mem;

  mem		= Lmem_dup(_, ptr, len);
  mem->next	= list->first;
  list->first	= mem;
  list->num++;
  return list;
}

static Llist
Llist_pop(Llist list, void *ptr, size_t len)
{
  L	_ = list->ptr._;
  Lmem	mem;

  mem		= list->first;
  list->first	= mem->next;

  LFATAL(!mem || mem->len != len);
  memcpy(ptr, mem->data, len);
  L_mem_free(_, mem);
  list->num--;
  return list;
}

/* Ltmp **************************************************************/
/* Ltmp **************************************************************/

static void
_Ltmp_free(Ltmp tmp)
{
  L	_ = tmp->ptr._;
  char	*buf;

  buf		= tmp->buf;
  tmp->buf	= 0;
  L_free(_, buf);
}

static Ltmp
Ltmp_add_c(Ltmp _, char c)
{
  if (_->pos >= _->size)
    _->buf	= Lrealloc(_->ptr._, _->buf, (_->size += BUFSIZ) * sizeof *_->buf);
  _->buf[_->pos++]	= c;
  return _;
}

static int
Ltmp_proc_copy(Ltmp _, void *ptr, int len)
{
  if (ptr)
    {
      LFATAL(len != _->pos);
      memcpy(ptr, _->buf, len);
    }
  return _->pos;
}

static int
unhex(char c)
{
  if (c>='0' && c<='9')
    return c-'0';
  if (c>='a' && c<='f')
    return c-'a'+10;
  if (c>='A' && c<='F')
    return c-'A'+10;
  return -1;
}

static int
Ltmp_proc_hex(Ltmp _, void *_ptr, int len)
{
  LFATAL(_->pos & 1, "hex strings must come in HEX pairs");
  if (_ptr)
    {
      char		*i = _->buf;
      unsigned char	*o = _ptr;
      int		n;

      LFATAL(len != _->pos/2);
      for (n=len; --n>=0; )
        {
          int	x;
          unsigned char c1, c2;

          c1	= *i++;
          c2	= *i++;
          x	= (unhex(c1)<<4) | unhex(c2);
          LFATAL(x<0, "non-hex digit in [HEX] constant: ", FORMAT_C(c1), FORMAT_C(c2));
          *o++	= x;
        }
    }
  return _->pos / 2;
}


/* Lio ***************************************************************/
/* Lio ***************************************************************/

static void
_Lio_free(Lio _)
{
  if (_->open)
    close(_->fd);
  _->fd	= -1;
}

static Lbuf
Lio_get_buf(Lio io, size_t max)
{
  Lbuf	buf;

  if (!io->pos)
    {
      if (io->buf->total <= max)
        {
          buf		= io->buf;
          io->buf	= 0;
          io->pos	= 0;
          return buf;	/* input buffer fits query, so return it	*/
        }
      if (io->buf->first->len <= max)
        {
          buf		= Lbuf_move_mem(Lbuf_new(io->ptr._), io->buf);
          LFATAL(!io->buf->first);
          io->pos	= 0;
          return buf;	/* input buffer's first mem fits query, so return it	*/
        }
    }
 
  L	_ = io->ptr._;
  Lmem	mem;
  int	tmp;
 
  /* input buffer is not directly suitable,
   * so copy portion into new output buffer
   */
  mem	= io->buf->first;
  tmp	= mem->len;
  LFATAL(io->pos > tmp);
  if (io->pos + max > tmp)
    max	= tmp - io->pos;

  buf	= Lbuf_add_ptr(buf = Lbuf_new(_), mem->data+io->pos, max);

  io->pos	+= max;
  if (io->pos >= mem->len)
    {
      L_mem_free(_, Lbuf_mem_get(io->buf));
      io->pos	= 0;
    }
  return buf;
}


/* Lrun **************************************************************/
/* Lrun **************************************************************/

static void
_Lrun_free(Lrun _)
{
  L_free(_->ptr._, _->steps);
  _->steps	= 0;
  _->cnt	= 0;
}

static Lrun
Lrun_add(Lrun _, Lfn fn, union Larg arg)
{
  struct Lstep	*step;

  if (_->pos >= _->cnt)
    _->steps	= Lrealloc(_->ptr._, _->steps, (_->cnt += 1024) * sizeof *_->steps);
  step		= &_->steps[_->pos++];
  step->fn	= fn;
  step->arg	= arg;
  return _;
}

static Lrun
Lrun_finish(Lrun _)
{
  _->steps	= Lrealloc(_->ptr._, _->steps, (_->cnt = _->pos) * sizeof *_->steps);
  _->pos	= 0;
  return _;
}





/* Lfn ***************************************************************/
/* Lfn ***************************************************************/
/* Lfn ***************************************************************/

static void
Lpush_arg_inc(Lrun _, Larg a)
{
  Lpush_inc(a.val);
}

static void
Lpush_reg(Lrun run, Larg a)
{
  L		_ = run->ptr._;
  Larg		s;

  s	= _->stack[1+a.i]->arg;
  if (!s.val)
    {
      s.buf	= Lbuf_new(_);
      Ldec(s);
    }
  Lpush_inc(s.val);
}

static void
Lpop_into_reg(Lrun run, Larg a)
{
  L		_ = run->ptr._;
  Lstack	s;
  Lval		v;

  s		= _->stack[1+a.i];
  v		= s->arg.val;
  s->arg.val	= 0;
  Lval_dec(v);
  s->arg.val	= Lval_pop_inc(_, 0);
}

static void
Ladd(Lrun run, Larg a)	/* a unused	*/
{
  L		_ = run->ptr._;
  Lval		v1, v2, v;

  v1	= Lpop_dec(_);
  v2	= Lpop_dec(_);
  v	= Lpush_dec(Lval_new(_, v2->ptr.type));
  switch (v2->ptr.type)
    {
    default:	return Lunknown(run, FORMAT_V(v2), "+", FORMAT_V(v1), NULL);
    case LNUM:
      switch (v1->ptr.type)
        {
        default:	return Lunknown(run, FORMAT_V(v2), "+", FORMAT_V(v1), NULL);
        case LNUM:	v->num.num	= v2->num.num+v1->num.num;			break;
        }
      break;
    case LBUF:
      switch (v1->ptr.type)
        {
        default:	return Lunknown(run, FORMAT_V(v2), "+", FORMAT_V(v1), NULL);
        case LBUF:	Lbuf_add_buf(Lbuf_add_buf(&v->buf, &v2->buf), &v1->buf);	break;
        }
      break;
    }
}

static void
Lmul(Lrun run, Larg a)	/* a unused	*/
{
  L		_ = run->ptr._;
  Lval	v1, v2, v;

  v1	= Lpop_dec(_);
  v2	= Lpop_dec(_);
  v	= Lpush_dec(Lval_new(_, v2->ptr.type));
  switch (v2->ptr.type)
    {
    default:	return Lunknown(run, FORMAT_V(v2), ".", FORMAT_V(v1), NULL);
    case LNUM:
      switch (v1->ptr.type)
        {
        default:	return Lunknown(run, FORMAT_V(v2), ".", FORMAT_V(v1), NULL);
        case LNUM:	v->num.num	= v2->num.num*v1->num.num;			break;
        }
      break;
    }
}

static void
Ldiv(Lrun run, Larg a)	/* a unused	*/
{
  L		_ = run->ptr._;
  Lval	v1, v2, va, vb;

  v1	= Lpop_dec(_);
  v2	= Lpop_dec(_);
  va	= Lpush_dec(Lval_new(_, v2->ptr.type));
  vb	= Lpush_dec(Lval_new(_, v2->ptr.type));
  switch (v2->ptr.type)
    {
    default:	return Lunknown(run, FORMAT_V(v2), ":", FORMAT_V(v1), NULL);
    case LNUM:
      switch (v1->ptr.type)
        {
        default:	return Lunknown(run, FORMAT_V(v2), ":", FORMAT_V(v1), NULL);
        case LNUM:
          /* v2 * va + vb == v1	*/
          if (v2->num.num == 0)
            {
              va->num.num	= 0;
              vb->num.num	= v1->num.num;
            }
          else if (v1->num.num == 0)
            {
              long long tmp;

              tmp	= v2->num.num;
              if (v1->num.num < 0)
                tmp	= -tmp;
              va->num.num	= tmp;
              /* va := 1 + ( v1 - vb) / v2	*/
              /* vb := v1 - v2 * va	with vb < 0 */

              va->num.num	= v1->num.num - v2->num.num;
              vb->num.num	= -v1->num.num;
              /* vb < 0	*/
            }
          else
            {
              va->num.num	= v2->num.num/v1->num.num;
              vb->num.num	= v2->num.num%v1->num.num;
            }
          break;
        }
      break;
    }
}

static void
Lsub(Lrun run, Larg a)	/* a unused	*/
{
  L		_ = run->ptr._;
  Lval	v1, v2, v;

  v1	= Lpop_dec(_);
  v2	= Lpop_dec(_);
  v	= Lpush_dec(Lval_new(_, v2->ptr.type));
  switch (v2->ptr.type)
    {
    default:	return Lunknown(run, FORMAT_V(v2), "-", FORMAT_V(v1), NULL);
    case LNUM:
      switch (v1->ptr.type)
        {
        default:	return Lunknown(run, FORMAT_V(v2), "-", FORMAT_V(v1), NULL);
        case LNUM:	v->num.num	= v2->num.num-v1->num.num;			break;
        }
      break;
    }
}

/* XXX TODO XXX
 * This should be nonblocking and asynchronous and left to the main loop
 */
static void
Li(Lrun run, Larg a)
{
  L		_ = run->ptr._;
  Lio		input;
  long long	max;
  Lval		v;
  Larg		ret;

  input	= _->read;
  LFATAL(!input);

  v	= Lpop_dec(_);
  switch (v->ptr.type)
    {
    default:	return Lunknown(run, FORMAT_V(v), "<", NULL);
    case LNUM:
      break;
    }

  max	= v->num.num;
  if (max<0)
    max	= -max;

  if (!input->eof && (!input->buf || input->pos >= input->buf->total))
    {
      /* no cached input yet	*/
      LFATAL(a.i<0, ": nonblocking read not yet implemented");
      LFATAL(input->pos);

      if (!input->buf)
        input->buf	= Lbuf_new(_);
      if (!Lbuf_add_readn(input->buf, input->fd, max < BUFSIZ ? BUFSIZ : max))
        {
          /* kick buffer on EOF	*/
          Lptr_dec(&input->buf->ptr);
          input->buf	= 0;
          input->eof	= 1;
        }
    }
  if (input->eof)
    {
      LFATAL(input->buf || input->pos);
      Lpush_dec(Lval_new(_, max ? LBUF : LNUM));	/* Lnum_new() is 0	*/
      return;		/* return empty buffer (or 0) on EOF	*/
    }
  LFATAL(!input->buf);

  if (!max)
    ret.num	= Lnum_set_size(Lnum_new(_), input->buf->total - input->pos);	/* return number of bytes available	*/
  else
    ret.buf	= Lio_get_buf(input, max);					/* return buffer with wanted data	*/
  Lpush_dec(ret.val);
}

static Lbuf
Lrun_buf_from_val(Lrun run, Lval v)
{
  Lbuf	buf;

  buf	= Lbuf_from_val(v);
  if (!buf)
    Lunknown(run, FORMAT_V(v), " has no ASCII representation", NULL);
  return buf;
}

/* XXX TODO XXX
 * This should be nonblocking and asynchronous and left to the main loop
 */
static void
Lo(Lrun run, Larg a)
{
  L		_ = run->ptr._;
  Lio	out;
  Lbuf	buf;

  out	= _->write;
  LFATAL(!out);
  if (out->eof)
    return;

  buf	= Lrun_buf_from_val(run, Lpop_dec(_));
  if (!buf)
    return;

  /* Following should use Liter,
   * but this is not prepared yet
   */
  for (Lmem m=buf->first; m; m=m->next)
    {
      int	put;

      put	= write_all(out->fd, m->data, m->len);
      if (put != m->len)
        {
          out->eof	= 1;
          break;
        }
    }
}

static void
Lfunc(Lrun run, Larg a)
{
  LFATAL(1, "$ not yet implemented");
}

static int
Lnum_cmp(Lnum a, Lnum b)
{
  LFATAL(a->ptr.type != LNUM || b->ptr.type != LNUM);
  return  a->num == b->num ? 0 : a->num < b->num ? -1 : 1;
}

static int
Lbuf_cmp(Lbuf a, Lbuf b)
{
  int	x, y;
  Lmem	m, n;

  LFATAL(a->ptr.type != LBUF || b->ptr.type != LBUF);
  /* XXX TODO XXX this should use Liter
   */
  x	= 0;
  y	= 0;
  for (m=a->first, n=b->first; m && n; )
    {
      int	max, tmp;

      max	= m->len - x;
      tmp	= n->len - y;
      if (max > tmp)
        max	= tmp;
      LFATAL(!max);
      tmp	= memcmp(m->data+x, n->data+y, max);
      if (tmp)
        return tmp < 0 ? -1 : 1;

      if ((x+=max) >= m->len)
        {
          m	= m->next;
          x	= 0;
        }
      if ((y+=max) >= n->len)
        {
          n	= n->next;
          y	= 0;
        }
    }
  return m ? -1 : n ? 1 : 0;
}

static void
Lcmp(Lrun run, Larg a)
{
  L		_ = run->ptr._;
  Larg	a1, a2;
  Larg	ret;
  int	cmp;

  a1.val	= Lpop_dec(_);
  a2.val	= Lpop_dec(_);

  if (a1.ptr->type != a2.ptr->type)
    {
      a1.buf	= Lrun_buf_from_val(run, a1.val);
      if (!a1.buf) return;
      a2.buf	= Lrun_buf_from_val(run, a2.val);
      if (!a2.buf) return;
    }
  switch (a1.ptr->type)
    {
    default:	return Lunknown(run, FORMAT_V(a1.val), " type ", FORMAT_V(a1.val), " cannot be compared (yet)", NULL);
    case LNUM:	cmp	= Lnum_cmp(a1.num, a2.num);	break;
    case LBUF:	cmp	= Lbuf_cmp(a1.buf, a2.buf);	break;
    }
  ret.num	= Lnum_set_i(Lnum_new(_), cmp);
  Lpush_dec(ret.val);
}

static void
Lnot(Lrun run, Larg a)
{
  L		_ = run->ptr._;
  Larg	a1, ret;

  a1.val	= Lpop_dec(_);
  switch (a1.ptr->type)
    {
    default:	return Lunknown(run, FORMAT_V(a1.val), " cannot apply function not to type ", FORMAT_V(a1.val), " (yet?)", NULL);
    case LNUM:	ret.num	= Lnum_set_i(Lnum_new(_), !a1.num->num);	break;
    }
  Lpush_dec(ret.val);
}

static void
Lneg(Lrun run, Larg a)
{
  L		_ = run->ptr._;
  Larg		a1, ret;

  a1.val	= Lpop_dec(_);
  switch (a1.ptr->type)
    {
    default:	return Lunknown(run, FORMAT_V(a1.val), " cannot apply function not to type ", FORMAT_V(a1.val), " (yet?)", NULL);
    case LNUM:	ret.num	= Lnum_set_i(Lnum_new(_), ~a1.num->num);	break;
    }
  Lpush_dec(ret.val);
}

static int
Lrun_tos_cond(Lrun run)
{
  L		_ = run->ptr._;
  Larg		a1;

  a1.val	= Lpop_dec(_);
  if (a1.val)
    switch (a1.ptr->type)
      {
      default:	Lunknown(run, FORMAT_V(a1.val), " unsuitable ", FORMAT_V(a1.val), " condition for {_}", NULL); return -1;
      case LNUM: if (a1.num->num) return 1; break;
      case LBUF: if (a1.buf->total) return 1; break;
      }
  return 0;
}

/* a.i: loop start
 */
static void
Lrun_loop(Lrun run, Larg a)
{
  if (Lrun_tos_cond(run) == 1)
    run->pos	= a.i;		/*  take the loop again	*/
}

/* a.i: Loop end
 */
static void
Lrun_loop0(Lrun run, Larg a)
{
  if (Lrun_tos_cond(run) == 0)
    run->pos	= a.i+1;	/* do not take the loop	*/
}

/* a.i: Loop start
 */
static void
Lrun_loop1(Lrun run, Larg a)
{
  if (Lrun_tos_cond(run) == 1)
    run->pos	= a.i+1;	/* take the loop again	*/
}

static void ___here___(void) { 000; }	/* Add mode Lfn here	*/






/* XXX TODO XXX wrong implementation *********************************/
/* XXX TODO XXX wrong implementation *********************************/
/* XXX TODO XXX wrong implementation *********************************/

static Lbuf
Lbuf_load(Lbuf _, const char *s)
{

  int	fd;

  if ((fd=open(s, O_RDONLY))<0)
    Loops(_->ptr._, "cannot load file", s, NULL);
  Lbuf_add_readall(_, fd);
  close(fd);
  return _;
}

static L L_parse(L, Lbuf);

/* load a file as program
 *
 * In future this must be re-implenented in L
 * by reading the program from INPUT into BUFFER
 * and then putting it into the program itself
 */
static L
L_load(L _, const char *s)
{
  struct Lbuf	*buf;

  buf	= Lbuf_load(Lbuf_new(_), s);
  L_parse(_, buf);
  Lptr_dec(&buf->ptr);
  return _;
}


/* PARSING ***********************************************************/
/* PARSING ***********************************************************/
/* PARSING ***********************************************************/

/*
        _       NOP, does nothing\n"
        0 9     push number to the stack\n"
        A Z     push register to stack\n"
        a z     pop TOS into register\n"
        [xx]    push buffer to stack, xx is a sequence of hex digits\n"
        'xx'    push string to stack as buffer\n"
        \"x\"   push string to stack as buffer\n"
        'x'     push string to stack as buffer\n"

DONE:
        >       pop TOS (Top Of Stack) and output it\n"
        <       pop TOS giving the number of bytes to read from input into TOS\n"
        =       compare: AB=: 0:A==B, -1:A<B, +1:A>B\n"
        +       add to TOS.  5_3+ leaves 8 on stack\n"
        -       substract from TOS.  5_3- leaves 2 on stack\n"
        .       multiply TOS. 5_3x leaves 15 on stack\n"
        :       multiply TOS. 5_3x leaves 15 on stack\n"

TODO:
        !       not.  empty stack, empty buffer and 0 become 1, everything else 0\n"
        ~       invert.  Inverts all bits.  On Numbers it negates, so 1 becomes -1\n"
        | & ^   or, and, xor, works for numbers and buffer\n"

        $       call builtin: 1_2'hello world'$ calls builtin 'hello world'\n"

        @       pop TOS and advance the given bytes (copy input to output)	=> Not needed

 *  We cannot use / ? % # as those have special meaning in URLs.
 *  \\ { } * ` ; are free, SPC is synonumous for + traditionally
 *  adding/substracting numbers to Buffers changes all bytes indivindually, with rollover
 *  adding/substracting buffer to numbers adds/subtracts the sum of all bytes in the buffer
 *  multiplying number with buffer repeats buffer: 'a'5* fives 'aaaaa'\n"
 *  dividing buffers: divides a buffer into two (negative: from end). 'abcde'3: gives 'abc' 'de' <-TOS\n"
 *  adding two buffer concatenates them: 'abc''de'+ gives 'abcde'\n"
 *  subtracting buffers concatenates the other way round: 'abc''de'- gives 'deabc'\n"
 *  multiplying buffers does a pattern search: 'abcde''d'. gives 4 where 'abcde''x'. gives 0\n"
 *  dividing buffers counts occurance of pattern 'abcabcde''c': gives 2 while 'abc''de': gives 0\n"
 */

/* This should be vastly implemented in L itself in future .. somehow
 */
static L
L_parse(L _, Lbuf buf)
{
  struct Liter	iter;
  Lrun		run;
  int		c;
  Ltmp		tmp;
  int		match, start;
  Llist		list;
  size_t	pos;
  struct data
    {
      int	pos;
      int	c;
      Lfn	fn;
    }		data;

  tmp	= Ltmp_new(_);
  list	= Llist_new(_);
  pos	= 0;
  run	= Lrun_new(_);
  start	= 0;
  match	= 0;
  for (Lbuf_iter(buf, &iter); (c = Liter_getc(&iter))>=0; )
    {
      union Larg	a;
      Lfn		f;

      pos++;
      if (start)
        {
          if (match == c || (c>='0' && c<='9' && match==-1))
            {
              Ltmp_add_c(tmp, c);
              continue;
            }
          a.buf	= Lbuf_new(_);
          switch (match)
            {
            default:	LFATAL(1, "internal error, unknown match: ", FORMAT_C(match), NULL);
            case '"':
            case '\'':
              Lbuf_add_tmp(a.buf, tmp, Ltmp_proc_copy);
              break;
            case ']':
              Lbuf_add_tmp(a.buf, tmp, Ltmp_proc_hex);
              break;
            }
          Lrun_add(run, Lpush_arg_inc, a);
        }
      if (c>='a' && c<='z')		{ f = Lpop_into_reg;	a.i = c-'a'; }
      else if (c>='A' && c<='Z')	{ f = Lpush_reg;	a.i = c-'A'; }
      else if (c>='0' && c<='9')
        {
          start	= 1;
          match	= -1;
          continue;
        }
      else
        switch (a.i=-1, c)	/* do not use other than a.i in this switch!	*/
          {
          default:
          case '_':	continue;
          case '"':	start=1; match=':'; continue;
          case '\'':	start=1; match='\''; continue;
          case '[':	start=1; match=']'; continue;
          case '+':	f = Ladd;	break;
          case '-':	f = Lsub;	break;
          case '.':	f = Lmul;	break;
          case ':':	f = Ldiv;	break;
          case '<':	f = Li;		break;
          case '>':	f = Lo;		break;
          case '$':	f = Lfunc;	break;
          case '=':	f = Lcmp;	break;
          case '!':	f = Lnot;	break;
          case '~':	f = Lneg;	break;
#if 0
          case '@':	f = LstackCount;break;
#endif
          case '(':
            data.c	= ')';
            data.fn	= Lrun_loop;
            data.pos	= run->pos;
            Llist_push(list, &data, sizeof data);
            continue;
          case '{':
            data.c	= '}';
            data.fn	= Lrun_loop1;
            data.pos	= run->pos;
            Llist_push(list, &data, sizeof data);
            f	= Lrun_loop0;
            break;
          case ')':
          case '}':
            Llist_pop(list, &data, sizeof data);
            if (c!=data.c)
              Loops(_, "encountered unpaired ", FORMAT_C(c), ", expected: ", FORMAT_C(data.c), NULL);
            f	= data.fn;
            a.i	= data.pos;
            for (int i=a.i; i<run->pos; i++)
              if (run->steps[a.i].arg.i == -1)
                run->steps[a.i].arg.i	= run->pos;	/* set the break/continue position	*/
            break;
          }
      Lrun_add(run, f, a);
    }

  Lptr_dec(&tmp->ptr);
  Lptr_dec(&list->ptr);

  run->cnt	= run->pos;
  run->pos	= 0;
  Lptr_push_stack_dec(&run->ptr, &_->run);

  return _;
}

static L
L_step(L _)
{
  Lstack	*last, stack;

  /* Cleanup freed structures	*/
  while (_->free)
    L_ptr_free(&_->free->ptr);
  
  last	= &_->run;
  while ((stack = *last)!=0)
    {
      struct Lstep	*step;
      Lrun		run;

      run	= stack->arg.run;
      if (run->pos >= run->cnt)
        {
	  /* program has reached it's end	*/
          *last		= stack->next;
          stack->next	= _->end;
          _->end	= stack;
          continue;
        }
      step	= &run->steps[run->pos++];
      step->fn(run, step->arg);
      last	= &(*last)->next;
    }
  return _;
}

static L
L_loop(L _)
{
  while (_->run)
    L_step(_);
  return _;
}

int
main(int argc, char **argv)
{
  L _;

  _	= L_init(NULL, NULL);
  L_load(_, argv[1]);
  L_loop(_);
  return Lexit(_);
}


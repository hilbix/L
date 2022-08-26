/*
 * This is free software as in free beer, free speech and free baby.
 *
 * GCCFLAGS:	-Wno-unused-function
 *
 * vim:	ts=8
 *
 * I am a bit curious about
 * who the first idiot was to think that it perhaps is a
 * good idea to redefine TABs to some other default than 8:
 *	https://www.imdb.com/title/tt0387808/
 * And here a reference to the proper default, the only one I accept:
 *	https://www.ibm.com/docs/de/aix/7.1?topic=t-tabs-command
 *	https://en.wikipedia.org/wiki/Tab_key#Tab_characters
 * Please do not mix TAB with indentation.
 * I use indentation of 2 for over 35 years now
 * with braces which belong together being either on the same row or column
 * (else I am unable to spot which braces belong together,
 *  sorry, this is some of my human limitations I cannot overcome).
 * And I expect TAB to stay at column 8 for much longer!
 *
 * egrep -w 'FORMAT|Loops' l.c | fgrep -v ', NULL);'
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

#include <time.h>
#include <unistd.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#if	1
#define	DP(...)	do { if (Ldebug) L_stderr(NULL, "[[", __FILE__, ":", FORMAT_I(__LINE__), " ", __func__, "]", ##__VA_ARGS__, "]\n", NULL); } while (0)
#else
#define	DP	xDP
#endif
#define	xDP(...)	do {} while (0)


/* Structures ********************************************************/
/* Structures ********************************************************/
/* Structures ********************************************************/

static const char	*Ldebug;

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
typedef struct Lregistry *Lreg;
typedef union Larg	Larg;

typedef void (		*Lfn)(Lrun, Larg);

enum Lwarns
  {
    LW_UNDERRUN,
    LW_OVERUSE,
    LW_LARGESTACK,
    LW_EXTREMESTACK,
    LW_r,		/* cannot open	*/
    LW_MAX_
  };

enum Lcount
  {
    LC_stack,
    LC_alloc,
    LC_use,
    LC_overuse,
    LC_step,
    LC_collect,
    LC_MAX_
  };

const char * const Lcounts[] =
  {
    "stack",
    "alloc",
    "use",
    "overuse",
    "step",
    "collect",
  };

#define	LCOUNT(WHAT)	++_->counts[LC_##WHAT]
#define	LCOUNT__(WHAT)	--_->counts[LC_##WHAT]

struct L
  {
    Lstack		stack[1+'Z'-'A'+1];
    Lstack		run, wait, end;
    Lstack		unused;
    Lval		use, free, overuse;
    Lmem		tmp;

    Lio			read, write, err;
    Lio			*io;
    int			ios;

    Lreg		registry;

    unsigned long	counts[LC_MAX_];

    void		*user;

    unsigned char	warns[(LW_MAX_+7)/8];
    unsigned		allocated:1;
    unsigned		statistics:1;
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

struct Lregistry
  {
    const unsigned char	*part;
    Lfn			fn;
    Lreg		*sub;
    unsigned char	min, max;
  };

#define	LR0(X,H)	{ #X, 0, 0, L##X, H }
#define	LR1(X,C,H)	{ #X, C, 0, L##X, H }
#define	LR2(X,C,D,H)	{ #X, C, D, L##X, H }
struct Lregister
  {
    const char		*name;	/* name of function	*/
    const unsigned	one;	/* unicode	*/
    const unsigned	two;	/* unicode	*/
    Lfn			fn;
    const char		*help;
  };

/* Value
 * value -> type
 */
union Lval
  {
    struct	Lptr	ptr;	/* depends on type	*/
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
    unsigned		c;
    long long		i;

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
    int			depth;
  };

struct Lstep
  {
    Lfn		fn;
    Larg	arg;
  };


/* Formatting ********************************************************/
/* Formatting ********************************************************/
/* Formatting ********************************************************/

typedef struct FormatArg
  {
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
    F_c,
    F_V,
    F_X,
  };


/* Auxiliary *********************************************************/
/* Auxiliary *********************************************************/
/* Auxiliary *********************************************************/

static int
bit(unsigned char *bits, int bit)
{
  return bits[bit/8] & (1<<(bit&7));
}

static void
bit_set(unsigned char *bits, int bit)
{
  bits[bit/8] |= (1<<(bit&7));
}

static void
bit_clr(unsigned char *bits, int bit)
{
  bits[bit/8] &= ~(1<<(bit&7));
}

#define	FORMAT_A(A)	(char *)F_A, &(A)
#define	FORMAT_U(U)	(char *)F_U, (unsigned long long)(U)
#define	FORMAT_I(I)	(char *)F_I, (long long)(I)
#define	FORMAT_C(C)	(char *)F_C, (unsigned)(C)
#define	FORMAT_c(C)	(char *)F_c, (unsigned)(C)
#define	FORMAT_V(V)	(char *)F_V, (const void *)(V)
#define	FORMAT_X(X)	(char *)F_X, (size_t)sizeof (X), (const void *)&(X)	/* hexdump	*/
#define	FORMAT_D(X,S)	(char *)F_X, (size_t)(S), (const void *)(X)		/* hexdump area	*/
#define	FORMAT_P(X)	(char *)F_X, (size_t)sizeof (X), (const void *)&(X)	/* pointer (currently wrong for LSB)	*/

static void Loops(L, ...);
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
static void _Lptr_free(Lptr ptr) { LFATAL(ptr, ": unsupported call to free()"); }
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
  if (s)
    write_all(f->fd, s, len);
}

static void
pFORMAT(Format *f, const void *s, size_t len)
{
  f->out(f, s, len);
}

static void
sFORMAT(Format *f, const void *s)
{
  f->out(f, s, strlen(s));
}

static void
cFORMAT(Format *f, char c)
{
  f->out(f, &c, 1);
}

static char
hex_nibble(unsigned char c)
{
  c	&= 0xf;
  return c<=9 ? '0'+c : 'A'+c-10;
}

static void
xFORMAT(Format *f, const unsigned char *ptr, size_t len)
{
  char	buf[100];

  if (!ptr)
    return sFORMAT(f, "(null)");
  cFORMAT(f, '[');
  for (int i=0; i<len; i++)
    {
      buf[0]	= hex_nibble(ptr[i]>>4);
      buf[1]	= hex_nibble(ptr[i]);
      pFORMAT(f, buf, 2);
    }
  cFORMAT(f, ']');
}

static void
vFORMAT(Format *f, FormatArg *a)
{
  const char	*s;

  while ((s=va_arg(a->l, const char *)) != 0)
    {
      switch ((unsigned long long)s)
        {
        unsigned	u;
        Lval		v;

        case F_A:	vFORMAT(f, va_arg(a->l, FormatArg *)); continue;
        case F_U:	s=f->buf; snprintf(f->buf, sizeof f->buf, "%llu", va_arg(a->l, unsigned long long));	break;
        case F_I:	s=f->buf; snprintf(f->buf, sizeof f->buf, "%lld", va_arg(a->l, long long));		break;
        case F_c:	s=f->buf; u = va_arg(a->l, unsigned); f->buf[0]=u; f->buf[1]=0; if (isprint(u)) break;	/* fallthrough	*/
        case F_C:	s=f->buf; u = va_arg(a->l, unsigned); snprintf(f->buf, sizeof f->buf, "%c(%02x)", isprint(u) ? u : '?', u);			break;
        case F_V:	v=va_arg(a->l, Lval); s=v ? Ltypes[v->ptr.type] : "(null)";				break;
        case F_X:
          {
            const void	*ptr;
            size_t	len;

            len	= va_arg(a->l, size_t);
            ptr	= va_arg(a->l, const void *);
            xFORMAT(f, ptr, len);
            continue;
          }
        }
      f->out(f, s, strlen(s));
    }
}

#define	FORMAT_INIT(F,OUT,FD,U)	do { F.out = OUT; F.fd = FD; F.user = U; } while (0)
#define	FORMAT_FLUSH(F)		do { F.out(&F, NULL, (size_t)0); } while (0)
#define	FORMAT_START(A,S)	do { va_start(A.l, S); } while (0)
#define	FORMAT_END(A)		do { va_end(A.l); } while (0)

static void
FORMAT(Format *f, ...)
{
  FormatArg	a;

  FORMAT_START(a, f);
  vFORMAT(f, &a);
  FORMAT_END(a);
}

static L
L_stderr(L _, ...)
{
  FormatArg	a;
  Format	f;

  FORMAT_INIT(f, writer, _ ? _->err->fd : 2, _);
  FORMAT_START(a, _);
  vFORMAT(&f, &a);
  FORMAT_END(a);
  return _;
}

static L
L_warn(L _, enum Lwarns w, ...)
{
  FormatArg	a;
  if (bit(_->warns, w))
    return _;
  bit_set(_->warns, w);
  FORMAT_START(a, w);
  L_stderr(_, "WARN: ", FORMAT_A(a), "\n", NULL);
  FORMAT_END(a);
  return _;
}

static void
Loops(L _, ...)
{
  int		e=errno;
  FormatArg	a;

  FORMAT_START(a, _);
  L_stderr(_, "OOPS: ", FORMAT_A(a), NULL);
  FORMAT_END(a);
  if (e)
    L_stderr(_, ": ", strerror(errno), NULL);
  L_stderr(_, "\n", NULL);
  if (Ldebug)	/* for GDB	*/
    {
      if (strcmp(Ldebug, "CORE_DUMP"))
        raise(SIGINT);
      else
       abort();
    }
  exit(23);
  abort();
  for(;;);
}

static void
Lunknown(Lrun run, ...)
{
  FormatArg	a;

  FORMAT_START(a, run);
  Loops(run->ptr._, "unknown operation: ", FORMAT_A(a), "\n", NULL);
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
  LFATAL(!r, ": out of memory");
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
  if (ptr)
    free(ptr);
  return _;
}

static L
L_free_const(L _, const void *ptr_const)
{
  return L_free(_, (void *)ptr_const);
}

/* we return type (void *) to support
 * char and unsigned char at the same time
 */
static void *
Lstrdup_n(L _, const void *s, size_t len)
{
  char	*tmp;

  tmp	= Lalloc(_, len+1);
  tmp[len]	= 0;
  memcpy(tmp, s, len);
  return tmp;
}

static void *
Lstrdup(L _, const void *s)
{
  return Lstrdup_n(_, s, strlen(s));
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
  LCOUNT(use);
  ptr->ptr.type	= type;
  return ptr;
}

static Lbuf	Lbuf_new(L _)	{ return &Lval_new(_, LBUF )->buf;  }
static Lnum	Lnum_new(L _)	{ return &Lval_new(_, LNUM )->num;  }
static Lrun	Lrun_new(L _)	{ return &Lval_new(_, LRUN )->run;  }
static Lio	Lio_new(L _)	{ return &Lval_new(_, LIO  )->io;   }
static Llist	Llist_new(L _)	{ return &Lval_new(_, LLIST)->list; }
static Ltmp	Ltmp_new(L _)	{ return &Lval_new(_, LTMP )->tmp;  }

static Lval Lpush_dec(Lval);

static Lval
Lval_push_new(L _, enum Ltype type)
{
  return Lpush_dec(Lval_new(_, type));
}

static Lbuf	Lbuf_push_new(L _)	{ return &Lval_push_new(_, LBUF )->buf;  }
static Lnum	Lnum_push_new(L _)	{ return &Lval_push_new(_, LNUM )->num;  }
static Lrun	Lrun_push_new(L _)	{ return &Lval_push_new(_, LRUN )->run;  }
static Lio	Lio_push_new(L _)	{ return &Lval_push_new(_, LIO  )->io;   }
static Llist	Llist_push_new(L _)	{ return &Lval_push_new(_, LLIST)->list; }
static Ltmp	Ltmp_push_new(L _)	{ return &Lval_push_new(_, LTMP )->tmp;  }

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

  _->err = io	= Lio_new(_);
  io->fd	= 2;
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

#define	LmemNx	next02394239
#define	LmemPr	last0349504
struct Lmem
  {
    Lmem		LmemNx, *LmemPr;
    size_t		len;
    char		data[0];
  };
static Lmem	Lmem_next(Lmem mem)	{ LFATAL(!mem); return mem->LmemNx; }

/* Detach an Lmem from a chain.
 * YOU MUST KNOW WHAT YOU ARE DOING (hence the _ prefix)
 * as we cannot check here
 * if you are about to corrupt some immutable datatype!
 */
static Lmem
_Lmem_detach(Lmem mem)
{
  Lmem	next, *last;

  LFATAL(!mem);

  next		= mem->LmemNx;
  last		= mem->LmemPr;
  xDP(FORMAT_P(mem), FORMAT_P(last), FORMAT_P(next));

  if (last)
    *last		= next;
  if (next)
    next->LmemPr	= last;

  mem->LmemNx		= 0;
  mem->LmemPr		= 0;
  return mem;
}

static Lmem
_Lmem_detach_ptr(Lmem *ptr)
{
  LFATAL(!ptr || (*ptr)->LmemPr != ptr);
  return _Lmem_detach(*ptr);
}

static L
L_mem_free(L _, Lmem mem)
{
  xDP(FORMAT_P(mem));
  _Lmem_detach(mem);
  return L_free(_, mem);
}


/* Free the first Lmem on a Lmem-list
 */
static int
Lmem_free(L _, Lmem *ptr)
{
  Lmem	mem;

  LFATAL(!ptr);
  mem	= *ptr;
  if (!mem)
    return 0;

  LFATAL(ptr != mem->LmemPr);
  L_mem_free(_, mem);
  return 1;
}

/* Get uninitialized buffer (may contain garbage)	*/
static Lmem
Lmem_new_quick(L _, size_t len)
{
  struct Lmem	*mem;

  mem		= Lalloc(_, (len+1)+sizeof *mem);	/* use fast variant	*/
  mem->LmemPr	= 0;
  mem->LmemNx	= 0;
  mem->len	= len;
  mem->data[len]= 0;					/* +1 for Lmem_str() below	*/
  return mem;
}

/* Attach mem	*/
static Lmem
Lmem_attach(Lmem mem, Lmem *ptr)
{
  Lmem	next;

  LFATAL(!ptr || !mem || mem->LmemNx || mem->LmemPr);
  xDP(FORMAT_P(mem), FORMAT_P(ptr), FORMAT_P(*ptr));

  next		= *ptr;
  mem->LmemPr	= ptr;
  mem->LmemNx	= next;
  if (next)
    {
      LFATAL(next->LmemPr != ptr);
      next->LmemPr	= &mem->LmemNx;
    }
  *ptr		= mem;
  return mem;
}

static Lmem
Lmem_attach_to_mem(Lmem mem, Lmem parent)
{
  LFATAL(!parent);
  return Lmem_attach(mem, &parent->LmemNx);
}

/* adds (appends) the memory buffer to buf
 */
static Lmem
Lmem_add_to_buf(Lbuf buf, Lmem mem)
{
  xDP(FORMAT_P(buf), FORMAT_P(mem));
  LFATAL(mem->LmemNx || mem->LmemPr);
  buf->last	= buf->last ? Lmem_attach_to_mem(mem, buf->last) : Lmem_attach(mem, &buf->first);
  buf->total	+= mem->len;
  return mem;
}

/* Get nulled buffer	*/
static Lmem
Lmem_new(L _, size_t len)
{
  struct Lmem	*mem;

  mem		= Lmem_new_quick(_, len);
  memset(mem->data, 0, len);
  return mem;
}

/* resize is only allowed as long as it is not attached to anything
 * As most of our data structures are considered to be immutable
 *
 * WARNING!  ADDRESS OF Lmem MAY CHANGE!
 */
static Lmem
Lmem_resize(L _, Lmem mem, size_t len)
{
  LFATAL(mem->LmemNx);
  if (len != mem->len)
    {
      mem		= Lrealloc(_, mem, (len+1)+sizeof *mem);
      mem->len		= len;
      mem->data[len]	= 0;
      if (mem->LmemPr)
        *mem->LmemPr	= mem;
    }
  return mem;
}

static Lmem
Lmem_dup(L _, const void *ptr, size_t len)
{
  struct Lmem	*mem;

  LFATAL(!ptr);
  mem		= Lmem_new(_, len);
  mem->len	= len;
  memcpy(mem->data, ptr, len);
  return mem;
}

/* note that const is wrong here, it's just a reminder that we act immutably	*/
static Lmem
Lmem_copy_one(L _, const Lmem src)
{
  LFATAL(!src);
  return Lmem_dup(_, src->data, src->len);
}

static Lmem
Lmem_copy(L _, Lmem src, size_t max)
{
  Lmem		mem;
  size_t	offset;

  mem		= Lmem_new_quick(_, max);
  offset	= 0;
  for (; src && max; src=src->LmemNx)
    {
      size_t	n;

      n	= src->len;
      if (n>max)
        n	= max;
      memcpy(mem->data+offset, src->data, n);
      offset	+= n;
      max	-= n;
    }
  return Lmem_resize(_, mem, offset);
}

/* TMP buffers are not allowed to be attached anywhere else	*/
static Lmem
Lmem_tmp(L _, Lmem mem)
{
  LFATAL(mem->LmemNx || mem->LmemPr);
  mem->LmemPr	= &_->tmp;
  mem->LmemNx	= _->tmp;
  _->tmp	= mem;
  return mem;
}

/* quick and dirty make C-String out of mem
 * WARNING! This only is valid until the next L_step()
 */
static const void *
Lmem_str(Lmem mem)
{
#if 1
  LFATAL(mem->data[mem->len], ": suspected BUG: buffer overrun");
#else
  mem->data[mem->len]	= 0;	/* redundant, but it does not hurt	*/
#endif
  return mem->data;
}

/* quick and dirty return a tempoary copy of the C-string of a mem
 * WARNING! This only is valid until the next L_step()
 */
static const void *
Lmem_tmp_str(L _, Lmem mem)
{
  return Lmem_str(Lmem_tmp(_, mem));
}

/* prevent others from trying to access private members	*/
#undef	LmemPr
#undef	LmemNx


/* Lval Lptr inc/dec *************************************************/
/* Lval Lptr inc/dec *************************************************/

/* Never call this yourself if you are unsure
 * Leave that to _dec() and _inc()
 */
static L
L_ptr_free(Lptr ptr)
{
  L	_ = ptr->_;

  LFATAL(!ptr);
  if (ptr->prev)
    *ptr->prev	= ptr->next;
  if (ptr->next)
    ptr->next->ptr.prev	= ptr->prev;
  LFREE[ptr->type](ptr);
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
Lptr_inc(Lptr ptr)
{
  L	_ = ptr->_;

  xDP(FORMAT_P(ptr), FORMAT_V(ptr));
  if (ptr->overuse)
    return ptr;
  if (!ptr->use)
    Lptr_move(ptr, &_->use);

  LCOUNT(use);
  if (++ptr->use)
    return ptr;

  ptr->use	= 1;
  ptr->overuse	= 1;
  LCOUNT(overuse);
  L_warn(_, LW_OVERUSE, FORMAT_V(ptr), " overused", NULL);
  return Lptr_move(ptr, &_->overuse);
}

/* dec reference count
 * This moves it to freelist if count drops to 0
 */
static Lptr
Lptr_dec(Lptr ptr)
{
  L	_ = ptr->_;

  if (ptr->overuse)
    return ptr;
  LCOUNT__(use);
  if (--ptr->use)
    return ptr;
  return Lptr_move(ptr, &_->free);	/* real free done in L_step()	*/
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
    {
      s	= Lalloc(_, sizeof *s);
      switch (LCOUNT(stack))
        {
        case 100000:	L_warn(_, LW_LARGESTACK, "large stack", NULL);			break;
        case 10000000:	L_warn(_, LW_EXTREMESTACK, "extremly large stack", NULL);	break;
        }
    }
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
  s->depth	= s->next ? s->next->depth + 1 : 1;
  *stack	= s;
  return v;
}

static Lptr
Lptr_push_stack_inc(Lptr ptr, int nr)
{
  return Lptr_push_stack_dec(ptr, &ptr->_->stack[nr]);
}

static Lval
Lval_pop_stack_inc(L _, Lstack *stack)
{
  Lstack	s;
  Lval		v;

  s		= *stack;
  if (!s)
    {
      L_warn(_, LW_UNDERRUN, "stack underrun", NULL);
      return 0;
    }
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
  if (!_.ptr)
    return 0;
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


/* Now the convenience functions for datatypes
 *
 * ONLY USE WITH NEWLY CREATED DATATYPES!
 */


/* Lbuf **************************************************************/
/* Lbuf **************************************************************/

#if 1
#define	VERIFY(...)	do {} while (0)
#else
#define	VERIFY(X,...)	do { _verify_##X##_(X, __LINE__, __func__, ##__VA_ARGS__, NULL); } while (0)
#endif

static void
_verify_buf_(Lbuf buf, int line, const char *fn, ...)
{
  FormatArg	a;
  long	cnt;
  Lmem	mem;

  cnt	= 0;
  for (mem = buf->first; mem; mem=Lmem_next(mem))
    cnt	+= mem->len;

  FORMAT_START(a, fn);
  LFATAL(cnt != buf->total, ": ", FORMAT_I(buf->total), " vs. ", FORMAT_I(cnt), " at ", FORMAT_I(line), " ", fn, FORMAT_A(a));
  FORMAT_END(a);
}

/* extracts (removes) the first memory buffer from buf
 * This is only allowed on unused buffers
 */
static Lmem
Lbuf_mem_get_mutable(Lbuf buf)
{
  Lmem	mem;

  LFATAL(!buf);
  LFATAL(!buf->first);

  xDP(FORMAT_P(buf));
  mem	= _Lmem_detach_ptr(&buf->first);
  LFATAL(!mem);

  buf->total	-= mem->len;
  if (!buf->first)
    {
      LFATAL(buf->last != mem);
      LFATAL(buf->total);
      buf->last	= 0;
    }
  return mem;
}

static Lmem
Lbuf_mem_get(Lbuf buf)
{
  LFATAL(!buf);
  LFATAL(buf->ptr.use);
  return Lbuf_mem_get_mutable(buf);
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
  return Lmem_add_to_buf(_, Lmem_new(_->ptr._, len));
}

static Lbuf
Lbuf_add_ptr(Lbuf _, const void *ptr, size_t len)
{
  memcpy(Lbuf_mem_new(_, len)->data, ptr, len);
  return _;
}

static Lbuf
Lbuf_add_mem(Lbuf buf, Lmem mem)
{
  Lmem_add_to_buf(buf, mem);
  return buf;
}

static Lbuf
Lbuf_add_str(Lbuf _, const void *s)
{
  return Lbuf_add_ptr(_, s, strlen(s));
}

static Lmem
Lmem_from_buf(Lbuf buf)
{
  L		_ = buf->ptr._;
  Lmem		mem;

  mem	= Lmem_copy(_, buf->first, buf->total);
  LFATAL(mem->len != buf->total);
  return mem;
}

static Lbuf
Lbuf_add_buf(Lbuf buf, Lbuf add)
{
//  mem	= Lbuf_mem_new(buf, add->total);
  Lmem_add_to_buf(buf, Lmem_from_buf(add));
  return buf;
}

static Lbuf
Lbuf_move_mem(Lbuf _, Lbuf src)
{
  Lmem_add_to_buf(_, Lbuf_mem_get(src));
  return _;
}

static Lbuf
Lbuf_add_readn(Lbuf buf, int fd, int max)
{
  L	_ = buf->ptr._;
  int	loops;
  Lmem	mem;

  if (max<0)
    max	= -max;
  else if (!max)
    max	= BUFSIZ;

  for (loops=0; loops<10000; loops++)
    {
      int got;

      mem	= Lmem_new(_, max);
      got	= read(fd, mem->data, (size_t)max);
      if (got>0)
        {
          LFATAL(got > mem->len);
          return Lbuf_add_mem(buf, Lmem_resize(_, mem, got));
        }
      L_mem_free(_, mem);
      if (!got)
        return 0;
      if (errno != EAGAIN)
        Loops(_, "read error on input", NULL);
    }
  Loops(_, "too many loops", NULL);
  return 0;
}

static Lbuf
Lbuf_add_readall(Lbuf _, int fd)
{
  while (Lbuf_add_readn(_, fd, 0));
  return _;
}

/* Do not forget FORMAT_FLUSH at the end!	*/
static void
Lbuf_writer(Format *f, const void *s, size_t len)
{
  Lbuf	buf = f->user;
  int	max;

  LFATAL(buf->ptr.type != LBUF);
  xDP(FORMAT_P(buf), FORMAT_I(f->fd), FORMAT_P(s),FORMAT_I(len));
  if (s)
    while (len>0)
      {
        Lmem	mem;

        if (!f->fd)
          {
            VERIFY(buf);
            max	= BUFSIZ;
            if (max<len)
              max	= len;
            mem	= Lbuf_mem_new(buf, max);
            LFATAL(buf->last != mem);
            f->fd	= 1;
            xDP(FORMAT_P(buf), FORMAT_P(mem), FORMAT_I(max));
          }

        mem	= buf->last;
        max	= mem->len - (f->fd - 1);
        if (max>len)
          max	= len;

        if (max<=0)
          {
            f->fd	= 0;
            continue;
          }
        memcpy(mem->data+(f->fd-1), s, max);
        len	-= max;
        s	+= max;
        f->fd	+= max;
      }
  else if (f->fd)
    {
      xDP(FORMAT_P(buf), FORMAT_P(buf->last), FORMAT_I(f->fd-1));
      if (buf->last->len > f->fd-1)
        {
          buf->total	-= buf->last->len - (f->fd-1);
          buf->last	= Lmem_resize(buf->ptr._, buf->last, f->fd-1);
        }
      f->fd	= 0;
      VERIFY(buf);
    }
}

static Lbuf
Lbuf_add_format(Lbuf buf, ...)
{
  FormatArg	a;
  Format	f;

  VERIFY(buf);
  FORMAT_INIT(f, Lbuf_writer, 0, buf);
  FORMAT_START(a, buf);
  vFORMAT(&f, &a);
  FORMAT_END(a);
  FORMAT_FLUSH(f);
  VERIFY(buf);
  return buf;
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

static Liter
Lbuf_iter(Lbuf _, Liter l)
{
  LFATAL(!l || _->ptr.type != LBUF);
  l->buf	= _;
  l->mem	= _->first;
  l->cnt	= _->total;
  l->pos	= 0;
  return l;
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
      _->mem	= Lmem_next(mem);
      _->pos	= 0;
    }
  _->cnt--;
  return c;
}

static void *
Liter_read(Liter _, void *_ptr, size_t len)
{
  Lmem		mem;
  size_t	offset;
  char		*ptr = _ptr;

  LFATAL(len > _->cnt);
  offset	= 0;
  while (len)
    {
      size_t	max;

      mem	= _->mem;
      max	= mem->len;
      if (max>len)
        max	= len;
      memcpy(ptr+offset, mem->data+_->pos, max);
      if ((_->pos+=max) >= mem->len)
        {
          _->mem	= Lmem_next(mem);
          _->pos	= 0;
        }
      _->cnt	-= max;
      len	-= max;
    }
  return _ptr;
}

/* create an allocated string from the rest of the iter
 * We must return void *, else compiler warns due to signedness ..
 */
static void *
Liter_strdup(Liter iter)
{
  char		*tmp;
  size_t	len;

  len		= iter->cnt;
  tmp		= Lalloc(iter->buf->ptr._, len+1);
  tmp[len]	= 0;
  return Liter_read(iter, tmp, len);
}


/* toString **********************************************************/
/* toString **********************************************************/
/* For efficiency reasons we are a bit redundant here	*/

static Lbuf
Lbuf_from_val_dec(Lval v)
{
  L    _ = v->ptr._;

  switch (v->ptr.type)
    {
    case LNUM: return Lbuf_add_format(Lbuf_dec(Lbuf_new(_)), FORMAT_I(v->num.num), NULL);
    case LBUF: return &v->buf;
    }
  return 0;
}

/* Warning: Lval should not be access anymore afterwards
 * WARNING! This mem is only valid until the next L_step()
 */
static Lmem
Lmem_from_val(Lval v)
{
  Lbuf	buf;
  Lmem	mem;

  buf	= Lbuf_from_val_dec(v);
  if (buf == &v->buf || buf->ptr.use || Lmem_next(buf->first) || buf->total != buf->first->len)
    return Lmem_from_buf(buf);

  /* Optimize a bit here:
   * Free the unused buffer code
   * and just return it's mem without memory duplication
   * (zero copy)
   *
   * buf->ptr.use == 0, hence nobody is using it
   * HOWEVER there might be still pointers using this here.
   * So beware.  Just do not do this if using this routine.
   */
 mem	= Lbuf_mem_get(buf);
 LFATAL(buf->first);
 L_ptr_free(&buf->ptr);
 return mem;
}

/* Warning: Lval should not be access anymore afterwards
 * WARNING! The returned string is only valid until the next L_step()
 */
static const void *
Lstr_from_val(Lval v)
{
  return Lmem_tmp_str(v->ptr._, Lmem_from_val(v));
}

/* Avoids multiple copies here if possible,
 * this is why we use the iter()
 */
static const void *
Lbuf_strdup(Lbuf buf)
{
  struct Liter iter;

  Lbuf_iter(buf, &iter);
  return Liter_strdup(&iter);
}

static const void *
Lstr_format(L _, ...)
{
  FormatArg	a;
  Lbuf		buf;
  const void	*ret;

  FORMAT_START(a, _);
  buf	= Lbuf_add_format(Lbuf_dec(Lbuf_new(_)), FORMAT_A(a), NULL);
  FORMAT_END(a);

  ret	= Lbuf_strdup(buf);

  L_ptr_free(&buf->ptr);
  return ret;
}

static Lbuf
Lval_buf(Lval v)
{
  LFATAL(v->ptr.type != LBUF, ": string or buffer required but got ", FORMAT_V(v));
  return &v->buf;
}

static long long
Lval_num(Lval v)
{
  LFATAL(v->ptr.type != LNUM, ": number required but got ", FORMAT_V(v));
  return v->num.num;
}

static long long
Lnum_pop(L _)
{
  return Lval_num(Lpop_dec(_));
}

static long long
Lnum_opt(L _, long long def)
{
  return _->stack[0]->arg.ptr->type == LNUM ? Lnum_pop(_) : def;
}

static const void *
Lstr_pop(L _)
{
  return Lstr_from_val(Lpop_dec(_));
}

static const void *
Lstr_opt(L _, const void *def)
{
  return _->stack[0]->arg.ptr->type != LNUM ? Lstr_pop(_) : def;
}

static Lbuf
Lbuf_push(Lbuf buf)
{
  Lptr_push_stack_inc(&buf->ptr, 0);
  return buf;
}



/* Registry **********************************************************/
/* Registry **********************************************************/
/* Registry **********************************************************/

static void
_Lreg_free(L _, Lreg reg)
{
  Lreg	regs[100];

  /* this is like a recursion, but a bit more iterative	*/
  regs[0]	= reg;
  for (int pos=0; pos>=0; )
    {
      Lreg	reg;

      /* top done?  Then prceed to the previous one	*/
      if ((reg = regs[pos]) == 0)
        {
          pos--;
          continue;
        }
      /* renumber sub-paths to start at 0
       * so ->max points to the uppermost sub-path directly
       */
      if (reg->min)
        {
          /* note that ->min usually is above 0, as C-strings do not contain NULs
           * But we do not count on this for stability reasons
           */
          LFATAL(reg->max < reg->min);
          reg->max	-= reg->min;
          reg->min	= 0;
        }
      /* the uppermost sub-path of the top (if exists) becomes the new top
       */
      if (reg->sub)
        {
          Lreg	sub;

          while ((sub=reg->sub[reg->max]) == 0 && reg->max)	/* we must not decrement ->max when it is 0	*/
            reg->max--;						/* we hit an empty sub	*/
          if (sub)
            {
              LFATAL(pos >= (sizeof regs/sizeof *regs)-1);	/* this should not happen.  If so we need something like a recursion here	*/
              regs[++pos]		= sub;			/* set the new top	*/
              reg->sub[reg->max]	= 0;			/* reduce the current top by its uppermost sub-path	*/

              if (reg->max--)
                continue;
              reg->max			= 0;			/* not needed, but does no harm	*/

              /* Small optimization:  When we became a leaf, free us early	*/
              regs[pos-1]		= sub;			/* needed	*/
              regs[pos]			= reg;			/* not needed, but does no harm	*/
            }
        }
      /* apparently the top has no sub-path anymore, so it is (reduced to) a leaf.
       * We can free it
       */
      if (reg->sub)
        {
          L_free(_, reg->sub);
          reg->sub	= 0;
        }
      if (reg->part)
        {
          L_free_const(_, reg->part);
          reg->part	= 0;
        }
      L_free(_, reg);
      regs[pos]	= 0;	/* decrement skipped, it will be done in the next loop anyway	*/
    }
}

/* Throw away the registry	*/
static L
Lreg_reset(L _)
{
  Lreg	reg;

  reg		= _->registry;
  _->registry	= 0;

  _Lreg_free(_, reg);
  return _;
}

/* XXX TODO XXX This routine is too long
 */
static Lreg
Lreg_find(L _, Liter name, int create)
{
  Lreg		*last;
  int		c;
  int		pos;

  last	= &_->registry;
  pos	= 0;
  for (;;)
    {
      Lreg			reg, add;
      const unsigned char	*part, *tmp;
      unsigned char		cc, lo, hi;

      reg	= *last;
      if (!reg)
        {
          /* we hit a nonexisting leaf	*/
          if (create)
            {
              /* create new leaf	*/
              reg	= Lalloc0(_, sizeof *reg);
              reg->part	= Liter_strdup(name);
              DP("new: '", reg->part, "'");

              *last	= reg;
            }
          return reg;
        }

      /* get the next character	*/
      c		= Liter_getc(name);
      LFATAL(!c);	/* must not happen	*/

      part	= reg->part;
      cc	= part[pos];

      /* found the match, continue	*/
      if (cc == c)
        {
          /* cc != 0	*/
          pos++;
          continue;
        }

      DP(FORMAT_I(pos), "'", part, "'", FORMAT_C(c), FORMAT_C(cc));
      /* are we at the end of the name?	*/
      if (c<0)
        {
          if (!cc)
            return reg;		/* we hit the exact node	*/
          if (!create)
            return 0;

          /* we are in the middle of a node.
           * Split it
           */
          add		= Lalloc0(_, sizeof *add);
          add->part	= Lstrdup_n(_, part, pos);	/* can be empty string	*/
          add->sub	= Lalloc0(_, sizeof *add->sub);
          add->min	= cc;
          add->max	= cc;
          add->sub[0]	= reg;

          tmp		= Lstrdup(_, part+pos+1);
          L_free_const(_, part);

          reg->part	= tmp;
          *last		= add;

          DP("split", FORMAT_I(pos), add->part, FORMAT_C(cc), reg->part);

          pos		= 0;
          return add;
        }

      /* are we at the end of the node?  Enter ->sub	*/
      if (!cc)
        {
          /* c > 0	*/
          Lreg	*tmp, *ext;

          lo	= reg->min;
          hi	= reg->max;
          /* are we outside of the ->sub table?	*/
          if (c>hi)
            {
              /* c > 0.  At init ->min == 0 and ->max == 0, so we always go here	*/
              hi	= c;
              if (!lo)
                lo	= c;
            }
          else if (c<lo)
            {
               /* hi >= c > 0.  So lo never goes to 0 here
               */
              lo	= c;
            }
          else
            {
              /* we found an existing slot within sub-node range ->min <= c <= ->max
               * The slot may be unused (NULL), though
               */
              last	= &reg->sub[c-lo];
              continue;
            }
          if (!create)
            return 0;

          /* add node as we are in create mode	*/
          ext		= Lalloc0(_, (1+hi-lo)*sizeof *ext);

          tmp		= reg->sub;
          DP(FORMAT_C(lo), ":", FORMAT_C(hi), FORMAT_P(tmp));
          if (tmp)
            memcpy(&ext[reg->min-lo], tmp, (1+reg->max-reg->min)*sizeof *ext);

          reg->min	= lo;
          reg->max	= hi;
          reg->sub	= ext;

          L_free(_, tmp);

          DP("add", FORMAT_C(c), FORMAT_I(c-lo));

          last		= &reg->sub[c-lo];
          pos		= 0;
          continue;
        }

      /* 0 < cc <> c > 0	*/

      /* we are in the middle of the node
       * split it
       */
      if (!create)
        return 0;

      lo		= cc;
      hi		= cc;
      if (c < lo)
        lo		= c;
      else
        hi		= c;

      add		= Lalloc0(_, sizeof *add);
      add->part		= Lstrdup_n(_, part, pos);	/* can be empty string	*/
      add->sub		= Lalloc0(_, (1+hi-lo)*sizeof *add->sub);
      add->min		= lo;
      add->max		= hi;
      add->sub[cc-lo]	= reg;
      /* add->sub[c] == NULL	*/

      tmp		= Lstrdup(_, part+pos+1);	/* part[pos] == cc > 0	*/

      *last		= add;
      reg->part		= tmp;

      L_free_const(_, part);

      DP("split", FORMAT_C(cc), FORMAT_C(c));

      last		= &add->sub[c-lo];
      pos		= 0;
    }
}

static L
L_register(L _, const char *name, Lfn fn)
{
  Lreg		reg;
  struct Liter	iter;

  DP(FORMAT_P(fn), " ", name);
  reg	= Lreg_find(_, Lbuf_iter(Lbuf_add_str(Lbuf_dec(Lbuf_new(_)), name), &iter), 1);

  LFATAL(!reg || !reg->part);	/* ->part can be the empty string but must not be NULL!	*/
  LFATAL(reg->fn, ": function ", name, " already registered", NULL);

  reg->fn	= fn;
  return _;
}

static L
L_register_all(L _, const struct Lregister *ptr)
{
  for (; ptr->name; ptr++)
    L_register(_, ptr->name, ptr->fn);
  return _;
}

/* Llist *************************************************************/
/* Llist *************************************************************/

static void
_Llist_free(Llist list)
{
  L	_ = list->ptr._;

  while (Lmem_free(_, &list->first))
    list->num--;
  LFATAL(list->num);
}

static Llist
Llist_push(Llist list, const void *ptr, size_t len)
{
  xDP(FORMAT_V(list), FORMAT_P(ptr), FORMAT_I(len), FORMAT_D(ptr, len));

  Lmem_attach(Lmem_dup(list->ptr._, ptr, len), &list->first);
  list->num++;
  return list;
}

static Llist
Llist_pop(Llist list, void *ptr, size_t len)
{
  L	_ = list->ptr._;
  Lmem	mem;

  mem		= list->first;
  LFATAL(!mem || mem->len != len);

  xDP(FORMAT_V(list), FORMAT_P(ptr), FORMAT_I(len), FORMAT_D(mem->data, len));

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
  LFATAL(_->pos & 1, ": hex strings must come in HEX pairs");
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
          LFATAL(x<0, ": non-hex digit in [HEX] constant: ", FORMAT_c(c1), FORMAT_c(c2));
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
      DP("ding");
      L_mem_free(_, Lbuf_mem_get_mutable(io->buf));	/* IO buffers must be used, else they would be freed	*/
      DP("dong");
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

  xDP(FORMAT_P(fn), FORMAT_P(arg));
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

#if 0
static void
Lpush_arg_dec(L _, Larg a)
{
  Lpush_dec(a.val);
}
#endif

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
  xDP(FORMAT_V(v1), "+", FORMAT_V(v2));
  if (!v1)
    {
      Lbuf_push_new(_);
      return;
    }
  if  (!v2)
    {
      Lpush_inc(v1);
      return;
    }
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
      LFATAL(v->num.num<0, ": nonblocking read not yet implemented");
      LFATAL(input->pos);

      if (!input->buf)
        input->buf	= Lbuf_new(_);
      DP("read", FORMAT_I(max));
      if (!Lbuf_add_readn(input->buf, input->fd, max ? max : BUFSIZ))
        {
          DP("EOF");
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
  DP("have=", FORMAT_I(input->buf->total), " pos=", FORMAT_I(input->pos), " out=", FORMAT_I(max));

  if (!max)
    ret.num	= Lnum_set_size(Lnum_new(_), input->buf->total - input->pos);	/* return number of bytes available	*/
  else
    ret.buf	= Lio_get_buf(input, max);					/* return buffer with wanted data	*/
  Lpush_dec(ret.val);
}

static Lbuf
Lrun_buf_from_val_dec(Lrun run, Lval v)
{
  Lbuf	buf;

  buf	= Lbuf_from_val_dec(v);
  if (!buf)
    Lunknown(run, FORMAT_V(v), " has no ASCII representation", NULL);
  return buf;
}

/* XXX TODO XXX
 * This should be nonblocking and asynchronous and left to the main loop
 */
static void
Lout(Lrun run, Lio out)
{
  L	_ = run->ptr._;
  Lbuf	buf;

  LFATAL(!out);
  if (out->eof)
    return;

  buf	= Lrun_buf_from_val_dec(run, Lpop_dec(_));
  if (!buf)
    return;

  /* Following should use Liter,
   * but this is not prepared yet
   */
  for (Lmem m=buf->first; m; m=Lmem_next(m))
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
Lo(Lrun run, Larg a)
{
  Lout(run, run->ptr._->write);
}

static void
Le(Lrun run, Larg a)
{
  Lout(run, run->ptr._->err);
}

/* 1$ is the toString function
 */
static void
Lfunc(Lrun run, Larg a)
{
  L		_ = run->ptr._;
  struct Liter	i;
  Larg		a1;
  Lreg		reg;

  a1.val	= Lpop_dec(_);
  if (!a1.val)
    return;

  switch (a1.ptr->type)
    {
    default:
      /* Try to convert it so a buffer	*/
      /* Following two lines should be folded into one somehow.	*/
      a1.buf	= Lbuf_from_val_dec(a1.val);
      Lpush_inc(a1.val);
      return;

    case LBUF:
      break;
    }

  Lbuf_iter(a1.buf, &i);
  reg	= Lreg_find(run->ptr._, &i, 0);
  LFATAL(!reg || !reg->fn, ": $ function unknown: ", Lmem_tmp_str(_, Lmem_from_val(a1.val)), NULL);

  DP(FORMAT_P(reg->fn));
  reg->fn(run, a1);
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
          m	= Lmem_next(m);
          x	= 0;
        }
      if ((y+=max) >= n->len)
        {
          n	= Lmem_next(n);
          y	= 0;
        }
    }
  return m ? -1 : n ? 1 : 0;
}

static void
Lcmp(Lrun run, Larg a)
{
  L	_ = run->ptr._;
  Larg	a1, a2;
  int	cmp;

  a1.val	= Lpop_dec(_);
  a2.val	= Lpop_dec(_);

  if (a1.ptr->type != a2.ptr->type)
    {
      a1.buf	= Lrun_buf_from_val_dec(run, a1.val);
      if (!a1.buf) return;
      a2.buf	= Lrun_buf_from_val_dec(run, a2.val);
      if (!a2.buf) return;
    }
  switch (a1.ptr->type)
    {
    default:	return Lunknown(run, FORMAT_V(a1.val), " type ", FORMAT_V(a1.val), " cannot be compared (yet)", NULL);
    case LNUM:	cmp	= Lnum_cmp(a1.num, a2.num);	break;
    case LBUF:	cmp	= Lbuf_cmp(a1.buf, a2.buf);	break;
    }
  Lnum_set_i(Lnum_push_new(_), cmp);
}

static void
Lnot(Lrun run, Larg a)
{
  L	_ = run->ptr._;
  Larg	a1, ret;

  a1.val	= Lpop_dec(_);
  switch (a1.ptr->type)
    {
    default:	return Lunknown(run, FORMAT_V(a1.val), " cannot apply function 'not' to type ", FORMAT_V(a1.val), " (yet?)", NULL);
    case LNUM:	ret.num	= Lnum_set_i(Lnum_new(_), !a1.num->num);	break;
    }
  Lpush_dec(ret.val);
}

static void
Lneg(Lrun run, Larg a)
{
  L	_ = run->ptr._;
  Larg	a1, ret;

  a1.val	= Lpop_dec(_);
  switch (a1.ptr->type)
    {
    default:	return Lunknown(run, FORMAT_V(a1.val), " cannot apply function 'neg' to type ", FORMAT_V(a1.val), " (yet?)", NULL);
    case LNUM:	ret.num	= Lnum_set_i(Lnum_new(_), ~a1.num->num);	break;
    }
  Lpush_dec(ret.val);
}

static void
Lstack_cnt(Lrun run, Larg a)
{
  L		_ = run->ptr._;
  Lstack	s;

  s	= _->stack[0];
  Lnum_push_new(_)->num	= s ? s->depth : 0;
}

/* This only examines the TOS, it does not pop it
 */
static int
Lrun_tos_cond(Lrun run)
{
  L		_ = run->ptr._;
  Larg		a1;
  Lstack	s;

  s		= _->stack[0];
  if (!s)
    return -1;
  a1.val	= s->arg.val;
  if (a1.val)
    switch (a1.ptr->type)
      {
      default:	Lunknown(run, FORMAT_V(a1.val), " unsuitable ", FORMAT_V(a1.val), " condition for {_}", NULL); return -1;
      case LNUM: if (a1.num->num) return 1; break;
      case LBUF: if (a1.buf->total) return 1; break;
      }
  return 0;
}

static int
Lrun_tos_cond_pop(Lrun run)
{
  int	cond;

  cond	= Lrun_tos_cond(run);
  if (cond>=0)
    Lpop_dec(run->ptr._);
  return cond;
}

/* a.i: loop start pos
 */
static void
Lrun_loop(Lrun run, Larg a)
{
  if (Lrun_tos_cond_pop(run) == 1)
    run->pos	= a.i;		/*  take the loop again	*/
}

/* a.i: conditional end pos (+1)
 */
static void
Lrun_cond(Lrun run, Larg a)
{
  if (Lrun_tos_cond_pop(run) != 1)
    run->pos	= a.i;		/* do not take the conditional	*/
}

#if 0
/* a.i: Loop start
 */
static void
Lrun_loop1(Lrun run, Larg a)
{
  if (Lrun_tos_cond_pop(run) == 1)
    run->pos	= a.i+1;	/* take the loop again	*/
}
#endif

/* XXX TODO XXX Re-implement this in L as soon as L is suitable for it
 *
 * pos buf pos width "xd" $
 */
static void
Lxd(Lrun run, Larg a)
{
  L		_ = run->ptr._;
  struct Liter	i1, i2;
  size_t	len;
  long long	pos, width;
  Lbuf		buf;
  char		tmp[100];
  Format	f;
  Lval		v;
  const char	*prefix, *suffix, *middle;
  int		ascii;

  prefix	= "";
  middle	= "  ! ";
  suffix	= " !\n";
  ascii		= 1;

  v	= Lpop_dec(_);
  if (v->ptr.type != LNUM)
    {
      prefix = Lstr_from_val(v);
      v	= Lpop_dec(_);
    }
  if (v->ptr.type != LNUM)
    {
      suffix = Lstr_from_val(v);
      v	= Lpop_dec(_);
    }
  if (v->ptr.type != LNUM)
    {
      middle = Lstr_from_val(v);
      v	= Lpop_dec(_);
    }
  width	= Lval_num(v);
  pos	= Lval_num(Lpop_dec(_));
  v	= Lpop_dec(_);
  if (v->ptr.type == LNUM)
    {
      ascii	= Lval_num(v);
      v	= Lpop_dec(_);
    }
  buf	= Lval_buf(v);

  FORMAT_INIT(f, Lbuf_writer, 0, Lbuf_push_new(_));
  len	= buf->total;
  Lnum_push_new(_)->num	= len;

  /* from here L could work asynchronously, .. but we cannot support that from C	*/

  Lbuf_iter(buf, &i1);
  Lbuf_iter(buf, &i2);
  for (; len; sFORMAT(&f, suffix))
    {
      int	max, i;

      max	= len;
      if (max>width)
        max	= width;

      sFORMAT(&f, prefix);
      snprintf(tmp, sizeof tmp, "%08llx", pos);
      sFORMAT(&f, tmp);

      pos	+= max;
      len	-= max;

      for (i=0; i<max; i++)
        {
          int	c;

          c	= Liter_getc(&i1);
          FORMAT(&f, " ", FORMAT_c(hex_nibble(c>>4)), FORMAT_c(hex_nibble(c)), NULL);
        }
      if (!ascii)
        continue;

      while (++i<=width)
        sFORMAT(&f, "   ");
      sFORMAT(&f, middle);

      for (i=0; i<max; i++)
        {
          int	c;

          c	= Liter_getc(&i2);
          if (!(ascii&2 ? (c>=' ' && c!=127) : isprint(c)))
            c	= ascii&4 ? '.' : ' ';
          cFORMAT(&f, c);
        }
      if (!(ascii&8))
        while (++i<=width)
          cFORMAT(&f, ' ');
    }
  FORMAT_FLUSH(f);
}

/* wait for some milliseconds	*/
static void
Lms(Lrun run, Larg a)
{
  struct timespec rq, rm;
  long long	num;

  num		= Lval_num(Lpop_dec(run->ptr._));

  rq.tv_sec	= num/1000;
  rq.tv_nsec	= 1000000L*(num%1000);

  /* perhaps better use clock_gettime, clock_nanosleep and TIMER_ABSTIME on CLOCK_MONOTONIC 	*/
  while (nanosleep(&rq, &rm)<0)
    {
      LFATAL(errno != EINTR, ": nanosleep() failed");
      rq	= rm;
    }
}

/* parse string into integer
 * we should improve this in future
 * and it should be done in L
 */
static void
Lint(Lrun run, Larg a)
{
  L		_ = run->ptr._;
  long long	base;
  const char	*data;
  long long	num;
  char		*end;

  base	= Lnum_opt(_, 10);
  data	= Lstr_pop(_);

  errno	= 0;
  num	= strtoll(data, &end, base);
  LFATAL(errno || !end || *end, ": cannot parse number ", data, " into integer: ", strerror(errno), ": ", end);
  Lnum_push_new(_)->num	= num;
}

/* parse string into integer from float
 * we should improve this in future
 * and it should be done in L
 */
static void
Lfloat(Lrun run, Larg a)
{
  L		_ = run->ptr._;
  const char	*data;
  long double	ld;
  long long	fact;
  char		*end;

  fact	= Lnum_opt(_, 1);
  data	= Lstr_pop(_);

  errno	= 0;
  ld	= strtold(data, &end);
  LFATAL(errno || !end || *end, ": cannot parse number ", data, " into float: ", strerror(errno), " ", end);
  Lnum_push_new(_)->num	= (long long)(ld * (long double)fact);
}

static void
Lstr(Lrun run, Larg a)
{
  Lbuf_push(Lbuf_from_val_dec(Lpop_dec(run->ptr._)));
}


static void ___here___(void) { 000; }	/* Add more Lfn here	*/



/* XXX TODO XXX wrong implementation *********************************/
/* XXX TODO XXX wrong implementation *********************************/
/* XXX TODO XXX wrong implementation *********************************/

static Lbuf
Lbuf_load(Lbuf _, const char *s)
{
  int	fd;

  if ((fd=open(s, O_RDONLY))<0)
    Loops(_->ptr._, "cannot load file ", s, NULL);
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
L_load_file(L _, const char *s)
{
  Lbuf	buf;

  buf	= Lbuf_load(Lbuf_new(_), s);
  L_parse(_, buf);
  Lptr_dec(&buf->ptr);
  return _;
}

static L
L_load_str(L _, const char *s)
{
  Lbuf	buf;

  buf	= Lbuf_add_str(Lbuf_new(_), s);
  L_parse(_, buf);
  Lptr_dec(&buf->ptr);
  return _;
}


/* $functions ********************************************************/
/* $functions ********************************************************/
/* $functions ********************************************************/


static void
Lstat_out(L _, Format *f)
{
  char	*sep;

  DP();
  sep	= "";
  for (int i=0; i<sizeof Lcounts/sizeof *Lcounts; i++)
    {
      FORMAT(f, sep, Lcounts[i], "=", FORMAT_I(_->counts[i]), NULL);
      sep	= " ";
    }
}

static void
Lstat(Lrun run, Larg name)
{
  L		_ = run->ptr._;
  Format	f;

  FORMAT_INIT(f, Lbuf_writer, 0, Lbuf_push_new(_));
  Lstat_out(_, &f);
  FORMAT_FLUSH(f);
}

#if 0
static void
Lstats(Lrun run, Larg name)
{
  L		_ = run->ptr._;
  Format	f;

  FORMAT_INIT(f, writer, 2, _);
  Lstat_out(_, &f);
  sFORMAT(&f, "\n");
}
#endif

/* open file for read
 * Return-type is wrong!  (For now)
 * It must return Lio!
 */
static void
Lr(Lrun run, Larg name)
{
  L		_ = run->ptr._;
  const char	*s;
  int		fd;

  fd		= open(s = Lstr_from_val(Lpop_dec(_)), O_RDONLY);
  LFATAL(!fd, ": opened file descriptor became 0, how is this possible?");
  if (fd<0)
    {
      L_warn(_, LW_r, s, ": cannot open for read: ", strerror(errno), NULL);
      fd	= 0;
    }
  Lnum_push_new(_)->num	= fd;
  DP(FORMAT_I(fd));
}

/* set input to FD
 */
static void
LI(Lrun run, Larg name)
{
  L	_ = run->ptr._;
  Larg	a1;
  Lio	old;

  a1.val	= Lpop_dec(_);
  switch (a1.ptr->type)
    {
      long long fd;

    default:	Lunknown(run, FORMAT_V(a1.val), " unsuitable ", FORMAT_V(a1.val), " condition for {_}", NULL); return;
    case LNUM:
      fd = a1.num->num;;
      LFATAL(fd<0 || (long long)(int)fd!=fd, ": file descriptor out of bounds");
      a1.io	= Lio_new(_);
      a1.io->fd	= fd;
      break;
    case LIO:
      Linc(a1);
      break;
    }
  old		= _->read;
  _->read	= a1.io;
  DP(FORMAT_I(a1.io->fd));

  a1.io		= old;
  Ldec(a1);
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
        !       not.  empty stack, empty buffer and 0 become 1, everything else 0\n"
        ~       invert.  Inverts all bits.  On Numbers it negates, so 1 becomes -1\n"
TODO:
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
  int		match;
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
  match	= 0;
  for (Lbuf_iter(buf, &iter); (c = Liter_getc(&iter))>=0; )
    {
      union Larg	a;
      Lfn		f;

      a.i	= -1;
      pos++;
      if (match)
        {
          if (match>0 ? match!=c : (c>='0' && c<='9'))
            {
              xDP(".", FORMAT_C(c));
              Ltmp_add_c(tmp, c);
              continue;
            }
          switch (match)
            {
            default:	LFATAL(1, ": internal error, unknown match: ", FORMAT_C(match), NULL);
            case -1:
              Ltmp_add_c(tmp, 0);
              a.num		= Lnum_new(_);
              a.num->num	= strtoll(tmp->buf, NULL, 10);
              break;
            case '"':
            case '\'':
              a.buf	= Lbuf_add_tmp(Lbuf_new(_), tmp, Ltmp_proc_copy);
              break;
            case ']':
              a.buf	= Lbuf_add_tmp(Lbuf_new(_), tmp, Ltmp_proc_hex);
              break;
            case '\n':
              a.ptr	= 0;	/* this is a comment, just ignore	*/
              break;
            }
          tmp->pos	= 0;
          if (a.ptr)
            Lrun_add(run, Lpush_arg_inc, a);

          if (match>0)
            {
              match	= 0;
              continue;
            }
          match	= 0;
          a.i	= -1;
        }
      DP("+", FORMAT_C(c));
      if (c>='a' && c<='z')		{ f = Lpop_into_reg;	a.i = c-'a'; }
      else if (c>='A' && c<='Z')	{ f = Lpush_reg;	a.i = c-'A'; }
      else if (c>='0' && c<='9')
        {
          match	= -1;
          Ltmp_add_c(tmp, c);
          continue;
        }
      else
        switch (c)	/* do not use other than a.i in this switch!	*/
          {
          default:
            if (!isspace(c)) Loops(_, "unknown input ", FORMAT_C(c), NULL);
          case '_':
            continue;
          case '#':	match='\n';	continue;
          case '"':	match='"';	continue;
          case '\'':	match='\'';	continue;
          case '[':	match=']';	continue;
          case '+':	f = Ladd;	break;
          case '-':	f = Lsub;	break;
          case '.':	f = Lmul;	break;
          case ':':	f = Ldiv;	break;
          case '<':	f = Li;		break;
          case '>':	f = Lo;		break;
          case '^':	f = Le;		break;
          case '$':	f = Lfunc;	break;
          case '=':	f = Lcmp;	break;
          case '!':	f = Lnot;	break;
          case '~':	f = Lneg;	break;
          case '@':	f = Lstack_cnt;	break;
          case '(':
            data.c	= ')';
            data.fn	= Lrun_loop;
            data.pos	= run->pos;
            Llist_push(list, &data, sizeof data);
            continue;
          case '{':
            data.c	= '}';
            data.fn	= 0;
            data.pos	= run->pos;
            Llist_push(list, &data, sizeof data);
            f	= Lrun_cond;
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
      if (f)
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

  LCOUNT(step);

  /* Cleanup freed structures	*/
  while (_->free)
    {
      LCOUNT(collect);

      DP("free", FORMAT_P(_->free));
      L_ptr_free(&_->free->ptr);
    }

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
      step	= &run->steps[run->pos];
      xDP(FORMAT_I(run->pos), FORMAT_P(step->fn), FORMAT_P(step->arg));
      run->pos++;
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

/* Idea: `-` is `rev` with AB- gives rev(A)rev(B)+ and not rev(A+B)
 * " hello " " world " - gives " olleh  dlrow
 */
static struct Lregister Lfns[] =
  {
    LR1(add,	'+',	"pop B, pop A, push A+B. str concatenates.  FUTURE: AstrBnum adds to each byte with rollover.  AnumBstr adds up all bytes"),
    LR1(sub,	'-',	"pop B, pop A, push A-B.  FUTURE: str ?, AstrBnum ?, AnumBstr ?"),
    LR1(mul,	'.',	"pop B, pop A, push A*B.  FUTURE: Astr indexes.  AnumBstr repeats str num times"),
    LR1(div,	':',	"pop B, pop A, push C, push D, A=B*C+D. B=0 is C=0,D=A else 0<=D<abs(B).  AstrBnum ?.  AnumBstr ?"),
    LR0(stat,		"push statistics.  WARNING: Format will change in future!"),
    LR0(r,		"pop name, push file opened for read.    0 on fail.        num: use fd for read"),
    LR1(i,	'<',	"pop count, read count bytes from input.  empty on EOF"),
    LR0(I,		"pop fd, set input to fd (default 0)"),
#if 0
    LR0(w,		"pop name, push file opened for create.  fail on exists.   num: use fd for write"),
    LR0(W,		"pop name, push file opened for append.  fail on missing.  num: use fd for write"),
    LR1(o),	'>',	"pop tos, write tos to output.  Terminate on error"),
    LR0(O,		"pop fd, set output to fd (default 1)"),
    LR1(e),	'^',	"pop tos, write tos to err.  Terminate on error"),
    LR0(E,		"pop fd, set err to fd (default 2)"),
    LR0(l,		"pop A. num: pop B, push current program from instruction A for addition B instructions.  A=0 first, B=-1 all"),
#endif
    LR0(xd,		"hexdump buffer"),
    LR0(ms,		"wait some milliseconds"),
    LR0(int,		"parse string to number, optionally with base"),
    LR0(float,		"parse float  to number, optionally with factor"),
    LR0(str,		"convert TOS into string"),
    {0}
  };







/* XXX TODO XXX
 * - should not use Lfns structure
 * - should be improved with binsearch
 */
static struct Lregister *
_Lreg_search(Lfn fn)
{
  struct Lregister *r;

  for (r=Lfns; r->name; r++)
    if (r->fn == fn)
      return r;
  return 0;
}

static void
_L_reg_dump(L _, Format *f, Lreg reg, const char *prefix)
{
  const char		*part;
  struct Lregister	*r;

  if (!reg)
    return FORMAT(f, "regdump: '", prefix, "' (none)\n", NULL);

  LFATAL(!reg->part);
  part	= Lstr_format(_, prefix, reg->part, NULL);

  FORMAT(f, "regdump: '", part, "': ", NULL);
  if (reg->fn)
    {
      r	= _Lreg_search(reg->fn);
      if (!r)
        FORMAT(f, "unknown function ", FORMAT_P(reg->fn), "\n", NULL);
      else
        FORMAT(f, "function ", r->name, "\n", NULL);
    }
  if (reg->min || reg->max)
    {
      int	n = reg->max - reg->min;

      for (int i=0; i<=n; i++)
        if (reg->sub[i])
          _L_reg_dump(_, f, reg->sub[i], Lstr_format(_, part, FORMAT_c(i+reg->min), NULL));
    }

  L_free_const(_, part);
  L_free_const(_, prefix);
}

static L
L_register_dump(L _, Format *f)
{
  _L_reg_dump(_, f, _->registry, Lstrdup(_,""));
  return _;
}

static void
_L_run_dump(L _, Format *f, Lrun run, const char *prefix)
{
  for (int i=0; i<run->cnt; i++)
    {
      struct Lstep	*step;

      step	= run->steps+i;
      FORMAT(f, prefix, ": ", FORMAT_I(i), " ", FORMAT_P(step->fn), " ", NULL);
      if (step->arg.i == -1 || (step->arg.i>=0 && (step->arg.i<=run->cnt || step->arg.i<='Z'-'A')))	/* yuck! But it works	*/
        {
          FORMAT(f, "i: ", FORMAT_I(step->arg.i), "\n", NULL);
          continue;
        }
      FORMAT(f, Ltypes[step->arg.ptr->type], ": ", NULL);
      switch (step->arg.ptr->type)
        {
        default:	FORMAT(f, FORMAT_X(step->arg), "\n", NULL); continue;
        case LNUM:	FORMAT(f, FORMAT_I(step->arg.num->num), "\n", NULL); continue;
        }
    }
  L_free_const(_, prefix);
}

static L
L_run_dump(L _, Format *f)
{
  for (Lstack s=_->run; s; s=s->next)
    {
      char	buf[200];

      snprintf(buf, sizeof buf, "run %4d", s->depth);
      _L_run_dump(_, f, s->arg.run, Lstrdup(_, buf));
    }
  return _;
}


static L
L_dump(L	_)
{
  Format	f;

  FORMAT_INIT(f, writer, 2, _);
  L_register_dump(_, &f);
  L_run_dump(_, &f);
  return _;
}

/* some very simple option processing
 */
static L
Largcargv(L _, int argc, char **argv)
{
  int	i;
  int	cflag, dump;

  cflag	= 0;
  dump	= 0;
  i	= 1;
  for (int opt=0; i<argc && argv[i][0]=='-'; )	/* programs must not start with `-`	*/
    {
      char		c;
      const char	*arg;

      DP("arg", FORMAT_I(i), " ", argv[i]);
      switch (c = argv[i][++opt])
        {
        default: Loops(_, "option not understood: ", argv[i], " problem at ", argv[i]+opt, NULL); break;
        case 0:
          LFATAL(opt==1, ": reading program from stdin not yet supported");
          i++;
          opt=0;
          continue;
        case 'c': cflag=1; continue;
        case 'd': dump=1; continue;

        case 'f':
          break;	/* options with argument fallthrough	*/
        }
      arg	= &argv[i][opt+1];
      i++;
      opt=0;
      if (!*arg)
        {
          LFATAL(i>=argc, ": missing argument for option ", FORMAT_c(c));
          arg	= argv[i++];
        }
      switch (c)
        {
        default: Loops(_, "internal error, forgot to implement option ", FORMAT_c(c), NULL); break;
        case 'f': L_load_file(_, arg);	break;
        }
    }

  if (cflag)
    {
      LFATAL(i>=argc, ": option -c requires an argument");
      L_load_str(_, argv[i++]);
    }
  else if (!_->run)
    {
      LFATAL(i>=argc, ": missing file argument (interactive mode not implemented)");
      L_load_file(_, argv[i++]);
    }

  /* push remaining args to stack	*/
  while (i < argc)
    Lbuf_add_str(Lbuf_push_new(_), argv[--argc]);

  if (dump)
    L_dump(_);
  return _;
}


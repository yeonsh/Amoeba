/*	@(#)extra.c	1.1	91/11/21 13:44:17 */
/* External procedures for these programs.  */

/* This includes <stdio.h> and "site.h".  */
#include "extra.h"

extern unsigned char xord[], xchr[];

/* These help to deal with Pascal vs. C strings.  */
void make_c_string (), make_pascal_string ();
void end_with_null (), end_with_space ();



/* Round R to the nearest whole number.  */

integer
zround (r)
  double r;
{
  return r >= 0.0 ? (r + 0.5) : (r - 0.5);
}



/* File operations.  */

/* Open a file; don't return if any error occurs.  NAME is a Pascal
   string, but is changed to a C string and not changed back.  */ 

FILE *
checked_fopen (name, mode)
  char *name;
  char *mode;
{
  FILE *result;
  char *cp;

  make_c_string (&name);
  
  result = fopen (name, mode);
  if (result != NULL)
    return result;
    
  perror (name);
  exit (1);
  /*NOTREACHED*/
}


/* Return true if we're at the end of FILE, else false.  This implements
   Pascal's `eof' builtin.  */

boolean
test_eof (file)
  FILE *file;
{
  register int c;

  /* Maybe we're already at the end?  */
  if (feof (file))
    return true;

  if ((c = getc (file)) == EOF)
    return true;

  /* Whoops, we weren't at the end.  Back up.  */
  (void) ungetc (c, file);

  return false;
}


/* Return true on end-of-line in FILE or at the end of FILE, else false.  */

boolean
eoln (file)
  FILE *file;
{
  register int c;

  if (feof (file))
    return true;
  
  c = getc (file);
  
  if (c != EOF)
    (void) ungetc (c, file);
    
  return c == '\n' || c == EOF;
}


/* Print real number R in the Pascal format N:M on the file F.  */

void
fprintreal (f, r, n, m)
  FILE *f;
  double r;
  int n, m;
{
  char fmt[50];  /* Surely enough, since N and M won't be more than 25
                    digits each!  */

  (void) sprintf (fmt, "%%%d.%dlf", n, m);
  (void) fprintf (f, fmt, r);
}


/* Print S, a Pascal string, on the file F.  It starts at index 1 and is
   terminated by a space.  */

static void
fprint_pascal_string (s, f)
  char *s;
  FILE *f;
{
  s++;
  while (*s != ' ') putc (*s++, f);
}

/* Print S, a Pascal string, on stdout.  */

void
printpascalstring (s)
  char *s;
{
  fprint_pascal_string (s, stdout);
}

/* Ditto, for stderr.  */

void
errprintpascalstring (s)
  char *s;
{
  fprint_pascal_string (s, stderr);
}

/* Read an integer from the file F, reading past the subsequent end of
   line.  */

integer
inputint (f)
  FILE *f;
{
  char buffer[50]; /* Long enough for anything reasonable.  */

  return
    fgets (buffer, sizeof (buffer), f)
    ? atoi (buffer)
    : 0;
}

/* Read three integers from stdin.  */

void
zinput3ints (a,b,c)
  integer *a, *b, *c;
{
  while (scanf ("%ld %ld %ld\n", a, b, c) != 3)
    (void) fprintf (stderr, "Please enter three integers.\n");
}



/* String operations.  */

/* Change the suffix of BASE (a Pascal string) to be SUFFIX (another
   Pascal string).  We have to change them to C strings to do the work,
   then convert them back to Pascal strings.  */

void
makesuffix (base, suffix)
  char *base;
  char *suffix;
{
  char *last_dot, *last_slash;
  make_c_string (&base);
  
  last_dot = rindex (base, '.');
  last_slash = rindex (base, '/');
  
  if (last_dot == NULL || last_dot < last_slash) /* Add the suffix?  */
    {
      make_c_string (&suffix);
      strcat (base, suffix);
      make_pascal_string (&suffix);
    }
    
  make_pascal_string (&base);
}
   
   
/* Deal with C and Pascal strings.  */

/* Change the Pascal string P_STRING into a C string; i.e., make it
   start after the leading character Pascal puts in, and terminate it
   with a null.  We also have to convert from the internal character set
   (ASCII) to the external character set, if we're running on a
   non-ASCII system.  */

void
make_c_string (p_string)
  char **p_string;
{
  (*p_string)++;
  end_with_null (*p_string);
}


/* Replace the first space we come to with a null.  */

void
end_with_null (s)
  char *s;
{
  for ( ; *s != ' '; s++)
#ifdef NONASCII
    *s = xchr[*s]
#endif
    ;

  *s = 0;
}


/* Change the C string C_STRING into a Pascal string; i.e., make it
   start one character before it does now (so C_STRING had better have
   been a Pascal string originally), and terminate with a space.  We
   also convert back from the external character set to the internal
   character set (ASCII), if we're running on a non-ASCII system.  */

void
make_pascal_string (c_string)
  char **c_string;
{
  end_with_space (*c_string);
  (*c_string)--;
}


/* Replace the first null we come to with a space.  */

void
end_with_space (s)
  char *s;
{
  for ( ; *s != 0; s++)
#ifdef NONASCII
    *s = xchr[*s]
#endif
    ;
  
  *s = ' ';
}


static char *
concat (s1, s2)
  char *s1, *s2;
{
  char *r = xmalloc (strlen (s1) + strlen (s2) + 1);
  strcpy (r, s1);
  strcat (r, s2);
  return r;
}


static char *
#ifdef ANSI               /* ifdef: lines added by UNIX TeX */
string_copy (char *s)     /* to solve syntax error problem */
#else
string_copy (s)
char *s;
#endif
{
  char *new_string = xmalloc (strlen (s) + 1);

  strcpy (new_string, s);

  return new_string;
}



/* Memory operations: variants of malloc(3) and realloc(3) that just
   give up the ghost when they fail.  */

extern char *malloc (), *realloc ();

char *
xmalloc (size)
  unsigned size;
{
  char *mem = malloc (size);
  
  if (mem == NULL)
    {
      fprintf (stderr, "! Cannot allocate %u bytes.\n", size);
      exit (10);
    }
  
  return mem;
}


char *
xrealloc (ptr, size)
  char *ptr;
  unsigned size;
{
  char *mem = realloc (ptr, size);
  
  if (mem == NULL)
    {
      fprintf (stderr, "! Cannot reallocate %u bytes at %x.\n", size, ptr);
      exit (10);
    }
    
  return mem;
}



/* Path searching.  */

#define NUMBER_OF_PATHS 11

static char *path[NUMBER_OF_PATHS];
static char *env_var_names[NUMBER_OF_PATHS]
  = { "BIBINPUTS",
      "GFFONTS",
      "MFBASES", "MFINPUTS", "MFPOOL",
      "PKFONTS",
      "TEXFORMATS", "TEXINPUTS", "TEXPOOL", "TEXFONTS", 
      "VFFONTS"
    };

#define READ_ACCESS 4       /* Used in access(2) call.  */

/* What separates elements of the path variables.  */
#ifdef MS_DOS
#define PATH_DELIMITER ';'
#else
#define PATH_DELIMITER ':'
#endif

/* We will need some system include files to deal with directories, even
   when SEARCH_SUBDIRECTORIES is undefined.  (We also make sure we don't
   open a directory as an input file.  */
#include <sys/types.h>
#include <sys/stat.h>

/* And if we're actually going to search subdirectorys, we need still
   more include files.  */
#ifdef SEARCH_SUBDIRECTORIES
/*AMOEBA CHANGE -- WE WANT dirent*/
/*#ifndef BSD*/
#include <dirent.h>
typedef struct dirent *directory_entry_type;
/*#else*/ /* BSD */
/*#include <sys/dir.h>
typedef struct direct *directory_entry_type;
#endif*/ /* BSD */

/* Declare the routine to get the current working directory.  */

#ifdef HAVE_GETWD
extern char *getwd ();
#define getcwd(b, len)  ((b) ? getwd (b) : getwd (xmalloc (len)))
#else
#ifdef ANSI
extern char *getcwd (char *, int);
#else
extern char *getcwd ();
#endif /* not ANSI */
#endif /* not HAVE_GETWD */

extern int errno;
#endif /* SEARCH_SUBDIRECTORIES */


/* Subroutines.  */
static void next_component ();
extern int is_dir ();

/* Says whether NAME is ok to open for reading.  */
#define READABLE_FILE(name) access (name, READ_ACCESS) == 0 && !is_dir (name)


/* Replace a leading or trailing `:' in DIR_LIST with DEFAULT_VALUE. 
   The result is always dynamically allocated.  */

static char *
expand_colon (dir_list, default_value)
  char *dir_list;
  char *default_value;
{
  char *expansion = xmalloc (strlen (dir_list) + 1);
  strcpy (expansion, dir_list);

  if (*expansion == PATH_DELIMITER)
    {
      char *temp = expansion;
      expansion = concat (default_value, expansion);
      free (temp);
    }
  if (*(expansion + strlen (expansion) - 1) == PATH_DELIMITER)
    {
      char *temp = expansion;
      expansion = concat (expansion, default_value);
      free (temp);
    }
  
  return expansion;
}


/* This routine initializes `path[PATH_INDEX]'.  If the environment
   variable `env_var_names[PATH_INDEX]' is not set, we simply use 
   DEFAULT_VALUE.  Otherwise, we use the value of the environment
   variable, and then replace a leading or trailing colon with
   DEFAULT_VALUE.  The result is always dynamically allocated.

   For example, if DEFAULT_VALUE is `foo', and the environment variable
   value is `:bar:baz:', the final result will be `foo:bar:baz:foo'.
   (Of course, it is pointless to have more than one extra `:' in
   practice.)  */
   
static void
do_path (path_index, default_value)
  unsigned path_index;
  char *default_value;
{
  char *temp = getenv (env_var_names[path_index]);

  path[path_index] = temp == NULL
                     ? string_copy (default_value)
                     : expand_colon (temp, default_value); 
}


#ifdef SEARCH_SUBDIRECTORIES
static char *
concat3 (s1, s2, s3)
  char *s1, *s2, *s3;
{
  char *r = xmalloc (strlen (s1) + strlen (s2) + strlen (s3) + 1);
  strcpy (r, s1);
  strcat (r, s2);
  strcat (r, s3);
  return r;
}


/* DIR_LIST is the default list of directories (colon-separated) to
   search.  If the environment variable
   "`env_var_names[PATH_INDEX]'_SUBDIR" exists, we use that instead.  We
   want to add all the subdirectories directly below each of the
   directories in the path.
     
   We return the list of directories found.
   
   By doing this all at the beginning of the program, we make the
   programs that look for only one file somewhat less efficient but this
   is more efficient when we look up many files using the same path.  */

static char *
do_subdir_path (path_index, dir_list)
  unsigned path_index;
  char *dir_list;
{
  char *cwd;
  unsigned len;
  char *result = xmalloc (1);
  char *env_var = concat (env_var_names[path_index], "_SUBDIR");
  char *temp = getenv (env_var);

  free (env_var);
  
  if (temp == NULL)
    {
      temp = dir_list;
      /* Make a copy in writable memory.  */
      dir_list = xmalloc (strlen (temp) + 1);
      strcpy (dir_list, temp);
    }
  else
    dir_list = expand_colon (temp, dir_list);

  *result = 0;

  /* Unfortunately, we can't look in the environment for the current
     directory, because if we are running under a program (let's say
     Emacs), the PWD variable might have been set by Emacs' parent
     to the current directory at the time Emacs was invoked.  This
     is not necessarily the same directory the user expects to be
     in.  So, we must always call getcwd(3) or getwd(3), even though
     they are slow and prone to hang in networked installations.  */
  cwd = getcwd (NULL, FILENAMESIZE + 2);
  if (cwd == NULL)
    {
      perror ("getcwd");
      exit (errno);
    }

  do
    {
      DIR *dir;
      directory_entry_type e;
      char dirname[FILENAMESIZE];

      next_component (dirname, &dir_list);

      /* All the `::'s should be gone by now, but we may as well make
         sure `chdir' doesn't crash.  */
      if (*dirname == 0) continue;

      /* By changing directories, we save a bunch of string
         concatenations (and make the pathnames the kernel looks up
         shorter).  */
      if (chdir (dirname) != 0) continue;

      dir = opendir (".");
      if (dir == NULL) continue;

      while ((e = readdir (dir)) != NULL)
        {
          if (is_dir (e->d_name) && strcmp (e->d_name, ".") != 0
              && strcmp (e->d_name, "..") != 0)
            {
              char *found = concat3 (dirname, "/", e->d_name);

              result = xrealloc (result, strlen (result) + strlen (found) + 2);

              len = strlen (result);
              if (len > 0)
                {
                  result[len] = PATH_DELIMITER;
                  result[len + 1] = 0;
                }
              strcat (result, found);
              free (found);
            }
        }
      closedir (dir);

      /* Change back to the current directory, in case the path
         contains relative directory names.  */
      if (chdir (cwd) != 0)
        {
          perror (cwd);
          exit (errno);
        }
    }
  while (*dir_list != 0);
  
  len = strlen (path[path_index]);
  path[path_index] = xrealloc (path[path_index], len + strlen (result) + 2);
  *(path[path_index] + len) = PATH_DELIMITER;
  *(path[path_index] + len + 1) = 0;
  strcat (path[path_index], result);
  
  return result;
}
#endif /* SEARCH_SUBDIRECTORIES */


/* This sets up the paths, by either copying from an environment variable
   or using the default path, which is defined as a preprocessor symbol
   (with the same name as the environment variable) in `site.h'.  The
   parameter PATH_BITS is a logical or of the paths we need to set.  */

extern void
setpaths (path_bits)
  int path_bits;
{
#if defined(SEARCH_SUBDIRECTORIES) && defined(TEXFONTS_SUBDIR)
  char *font_subdirs;
#endif

  /* We must assign to the TFM file path before doing any of the other
     font paths, since it is used as a default.  */
  if (path_bits
      & (TFMFILEPATHBIT | GFFILEPATHBIT | PKFILEPATHBIT | VFFILEPATHBIT))
    {
      do_path (TFMFILEPATH, TEXFONTS);
#if defined(SEARCH_SUBDIRECTORIES) && defined(TEXFONTS_SUBDIR)
      font_subdirs = do_subdir_path (TFMFILEPATH, TEXFONTS_SUBDIR);
#endif
    }

  if (path_bits & BIBINPUTPATHBIT)
    do_path (BIBINPUTPATH, BIBINPUTS);

  if (path_bits & GFFILEPATHBIT)
    {
      do_path (GFFILEPATH, path[TFMFILEPATH]);
#if defined(SEARCH_SUBDIRECTORIES) && defined(TEXFONTS_SUBDIR)
      path[GFFILEPATH] = concat3 (path[GFFILEPATH], ":", font_subdirs);
#endif
    }

  if (path_bits & MFBASEPATHBIT)
    do_path (MFBASEPATH, MFBASES);

  if (path_bits & MFINPUTPATHBIT)
    {
      do_path (MFINPUTPATH, MFINPUTS);
#if defined(SEARCH_SUBDIRECTORIES) && defined(MFINPUTS_SUBDIR)
      (void) do_subdir_path (MFINPUTPATH, MFINPUTS_SUBDIR);
#endif
    }

  if (path_bits & MFPOOLPATHBIT)
    do_path (MFPOOLPATH, MFPOOL);

  if (path_bits & PKFILEPATHBIT)
    {
      do_path (PKFILEPATH, path[TFMFILEPATH]);
#if defined(SEARCH_SUBDIRECTORIES) && defined(TEXFONTS_SUBDIR)
      path[PKFILEPATH] = concat3 (path[PKFILEPATH], ":", font_subdirs);
#endif
    }

  if (path_bits & TEXFORMATPATHBIT)
    do_path (TEXFORMATPATH, TEXFORMATS);

  if (path_bits & TEXINPUTPATHBIT)
    {
      do_path (TEXINPUTPATH, TEXINPUTS);
#if defined(SEARCH_SUBDIRECTORIES) && defined(TEXINPUTS_SUBDIR)
      (void) do_subdir_path (TEXINPUTPATH, TEXINPUTS_SUBDIR);
#endif
    }

  if (path_bits & TEXPOOLPATHBIT)
    do_path (TEXPOOLPATH, TEXPOOL);

  /* Some sites want to have a system default for VFFONTS, and others
     don't.  */
  if (path_bits & VFFILEPATHBIT)
    {
#ifdef VFFONTS
      do_path (VFFILEPATH, VFFONTS);
#else
      do_path (VFFILEPATH, path[TFMFILEPATH]);
#endif
#if defined(SEARCH_SUBDIRECTORIES) && defined(TEXFONTS_SUBDIR)
      path[VFFILEPATH] = concat3 (path[VFFILEPATH], ":", font_subdirs);
#endif
    }
}


/* Look for NAME, a Pascal string, in a colon-separated list of
   directories.  The path to use is given (indirectly) by PATH_INDEX.
   If the search is successful, leave the full pathname in NAME (which
   therefore must have enough room for such a pathname), padded with
   blanks.  Otherwise, or if NAME is an absolute or relative pathname,
   just leave it alone.  */

boolean
testreadaccess (name, path_index)
  char *name;
  int path_index;
{
  char potential[FILENAMESIZE];
  int ok = 0;
  char *the_path = path[path_index];
  char *saved_path = the_path;
  
  make_c_string (&name);

  if (*name == '/'
      || (*name == '.' && *(name + 1) == '/')
      || (*name == '.' && *(name + 1) == '.' && *(name + 2) == '/'))
    ok = READABLE_FILE (name); 
  else
    {
      do
	{
	  next_component (potential, &the_path);

          if (*potential != 0)
            {
              strcat (potential, "/");
	      strcat (potential, name);
	      ok = READABLE_FILE (potential);
            }
	}
      while (!ok && *the_path != 0);

      /* If we found it, leave the answer in NAME.  */
      if (ok)
        strcpy (name, potential);
    }
  
  make_pascal_string (&name);
  
  return ok;
}


/* Return, in NAME, the next component of PATH, i.e., the characters up
   to the next PATH_DELIMITER.  */
   
static void
next_component (name, path)
  char name[];
  char **path;
{
  unsigned count = 0;
  
  while (**path != 0 && **path != PATH_DELIMITER)
    {
      name[count++] = **path;
      (*path)++; /* Move further along, even between calls.  */
    }
  
  name[count] = 0;
  if (**path == PATH_DELIMITER)
    (*path)++; /* Move past the delimiter.  */
}


#if !defined(S_ISDIR)
#define S_ISDIR(m) ((m & S_IFMT) == S_IFDIR)
#endif

/* Return true if FN is a directory or a symlink to a directory,
   false if not. */

int
is_dir (fn)
  char *fn;
{
  struct stat stats;

  return stat (fn, &stats) == 0 && S_ISDIR (stats.st_mode);
}


#include <ctype.h>

#include "util.h"
#include "uthash.h"

#undef uthash_fatal
#define uthash_fatal(msg) (Warning(msg), exit(-1))

int xbup_opt_preserve_uuids = 0;
int xbup_opt_numeric_ids = 0;


/* string_to_long:
 * I got tired of the clunky interface for strtol, so I wrote my own function.
 *
 * string_to_long(s) attempts to convert the string s to a long.
 *
 * legal syntax: W^* [+|-] D^+ W^*
 * 
 * here W is whitespace, and D is a digit
 *
 * if the syntax is invalid, then the global variable conversion_error is
 * set to -1, errno is set to EINVAL, and 0 is returned.
 *
 * if the syntax is legal, but the number is out of range, then
 * conversion_error is set to -1, errno is set to ERANGE, and the value
 * reduced modulo the word size is returned
 *
 * Otherwise, conversion_error is set to 0, errno is unchanged, and the
 * value is returned.
 *
 */
 
int conversion_error = 0;

long string_to_long(const char *s)
{
   unsigned long res = 0;
   unsigned long bnd = ULONG_MAX/10;
   int neg = 0;

   conversion_error = 0;

   while (*s && isspace((int)(unsigned char) *s)) s++; // skip white space
   if (*s == '-') {
      neg = 1;
      s++;
   }
   else if (*s == '+') {
      s++;
   }

   if (!*s || !isdigit((int)(unsigned char) *s)) {
      conversion_error = -1;
      errno = EINVAL;
      return 0;
   }

   while (*s && isdigit((int)(unsigned char) *s)) {
      unsigned long dig = *s - '0';

      if (res > bnd) {
         conversion_error = -1;
         errno = ERANGE;
      }

      res = 10UL*res + dig; 

      s++;
   }

   while (*s && isspace((int)(unsigned char) *s)) s++; // skip white space
   if (*s) {

      /* This path is taken if syntax is incorrect,
         even if overflow occurred */

      conversion_error = -1;
      errno = EINVAL;
      return 0;
   }

   if (!conversion_error) {
      unsigned long bnd1; 

      if (neg)
         bnd1 = (unsigned long) LONG_MIN;
      else
         bnd1 = (unsigned long) LONG_MAX;

      if (res > bnd1) {
         conversion_error = -1;
         errno = ERANGE;
      }
   }

   if (neg) res = -res;

   return CAST_sl_ul(res);
}


/* strip_slashes strips off all trailing slashes from a file
 * name (except if that would leave the empty string).
 * Returns the length of the resulting string.
 */

int strip_slashes(char *s)
{
   int len = strlen(s);

   while (len > 1 && s[len-1] == '/') len--;
   s[len] = '\0';
   return len;
}



int is_prefix(const char *pat, const char *txt)
{
   const char *p, *t;

   p = pat;
   t = txt;

   if (!p) return 1;
   if (!t) {
      return !(*p);
   }

   while (*p) {
      if (*p != *t) return 0;
      ++p;
      ++t;
   }

   return 1;
}

int is_suffix(const char *pat, long patlen, const char *txt, long txtlen)
{
   if (patlen > txtlen) {
      return 0;
   }
   else {
      return (strcmp(pat, txt + txtlen - patlen) == 0);
   }
}




/* reads up a line of input into s, null terminated, no newline.
 * Assumes s has space for MAXLEN characters.
 * returns # of characters written into s (including trailing zero).
 * returns 0 on eof and -1 on error.
 */


int readline(FILE *fp, char *s)
{
   int i, c;

   c = getc(fp);

   if (c == EOF) {
      if (feof(fp)) 
         return 0;
      else
         return -1;
   }

   i = 0;
   while (c != EOF && c != '\n') {
      if (i >= MAXLEN) return -1;
      s[i] = c;
      i++;

      c = getc(fp);
   }

   if (c == EOF) return -1;

   if (i >= MAXLEN) return -1;
   s[i] = '\0';
   i++;

   return i;
}


/* name_table is a hash table implemented using uthash */

struct name_table_entry {
   char *key;
   int data;
   UT_hash_handle hh;
};

static 
struct name_table_entry *name_table = NULL;

static 
struct name_table_entry *add_name(const char *s)
{
   struct name_table_entry *ptr;
   char *dup;
   size_t len;

   ptr = malloc(sizeof(struct name_table_entry));
   dup = strdup(s);
   if (!ptr || !dup) {
      Warning("malloc error");
      exit(-1);
   }

   ptr->key = dup;
   ptr->data = 0;
   len = strlen(ptr->key);

   HASH_ADD_KEYPTR(hh, name_table, ptr->key, len, ptr);

   return ptr;
}

static 
struct name_table_entry *find_name(const char *s)
{
   struct name_table_entry *ptr;
   size_t len;

   len = strlen(s);

   HASH_FIND(hh, name_table, s, len, ptr);
   return ptr;
}



void collect_names(const char *fname)
{
   FILE *fp;
   char s[MAXLEN];
   int ret;
   struct name_table_entry *p;
   int i;

   fp = fopen(fname, "r");
   if (!fp) {
      WARN("can't open %s\n", fname);
      exit(-1);
   }

   while ( (ret = readline(fp, s)) > 0) {
      if (ret == 1) continue; /* ignore blank lines */

      /* check for bad names */ 

      if (s[0] == '/' || s[ret-2] == '/') {
         WARN("file: %s: bad name: %s\n", fname, s);
         exit(-1);
      }

      for (i = 0; i <= ret-3; i++) {
         if (s[i] == '/' && s[i+1] == '/') {
            WARN("file: %s: bad name: %s\n", fname, s);
            exit(-1);
         }
      }

      p = find_name(s);
      if (!p) p = add_name(s);
      p->data = 1;
      for (i = ret-2; i >= 0; i--) {
         if (s[i] == '/') {
            s[i] = '\0';
            if (!find_name(s)) add_name(s);
         }
      }
   }

   if (ret < 0) {
      WARN("error reading  %s\n", fname);
      exit(-1);
   }

   fclose(fp);
}

int lookup_name(const char *s)
{
   struct name_table_entry *p = find_name(s);
   if (p) 
      return p->data;
   else
      return -1;
}

/**** hash table to map uid_t's to names */

struct uid2nam_table_entry {
   uid_t key;
   char *data;
   UT_hash_handle hh;
};

static
struct uid2nam_table_entry *uid2nam_table = NULL;

static
struct uid2nam_table_entry *uid2nam_add(uid_t uid)
{
   struct uid2nam_table_entry *ptr;

   ptr = malloc(sizeof(struct uid2nam_table_entry));
   if (!ptr) {
      Warning("malloc error");
      exit(-1);
   }
 
   ptr->key = uid;
   ptr->data = NULL;

   HASH_ADD_KEYPTR(hh, uid2nam_table, &ptr->key, sizeof(uid_t), ptr);

   return ptr;
}

static
struct uid2nam_table_entry *uid2nam_find(uid_t uid)
{
   struct uid2nam_table_entry *ptr;

   HASH_FIND(hh, uid2nam_table, &uid, sizeof(uid_t), ptr);
   return ptr;
}

char *map_uid_to_name(uid_t uid)
{
   struct uid2nam_table_entry *ptr;
   struct passwd *uid_entry;

   ptr = uid2nam_find(uid);
   if (!ptr) {
      ptr = uid2nam_add(uid);
      uid_entry = getpwuid(uid);
      if (uid_entry) {
         ptr->data = strdup(uid_entry->pw_name);
         if (!ptr->data) {
            Warning("malloc failure");
            exit(-1);
         }
      }
   }

   return ptr->data;
}

/**** hash table to map gid_t's to names */

struct gid2nam_table_entry {
   gid_t key;
   char *data;
   UT_hash_handle hh;
};

static
struct gid2nam_table_entry *gid2nam_table = NULL;

static
struct gid2nam_table_entry *gid2nam_add(gid_t gid)
{
   struct gid2nam_table_entry *ptr;

   ptr = malloc(sizeof(struct gid2nam_table_entry));
   if (!ptr) {
      Warning("malloc error");
      exit(-1);
   }
 
   ptr->key = gid;
   ptr->data = NULL;

   HASH_ADD_KEYPTR(hh, gid2nam_table, &ptr->key, sizeof(gid_t), ptr);

   return ptr;
}

static
struct gid2nam_table_entry *gid2nam_find(gid_t gid)
{
   struct gid2nam_table_entry *ptr;

   HASH_FIND(hh, gid2nam_table, &gid, sizeof(gid_t), ptr);
   return ptr;
}

char *map_gid_to_name(gid_t gid)
{
   struct gid2nam_table_entry *ptr;
   struct group *gid_entry;

   ptr = gid2nam_find(gid);
   if (ptr) 
      return ptr->data;
   else {
      ptr = gid2nam_add(gid);
      gid_entry = getgrgid(gid);
      if (gid_entry) {
         ptr->data = strdup(gid_entry->gr_name);
         if (!ptr->data) {
            Warning("malloc failure");
            exit(-1);
         }
      }
   }

   return ptr->data;
}

/* hash table to map names to uids */

struct nam2uid_table_entry {
   char *key;
   uid_t data;
   int known;
   UT_hash_handle hh;
};

static 
struct nam2uid_table_entry *nam2uid_table = NULL;

static 
struct nam2uid_table_entry *nam2uid_add(const char *s)
{
   struct nam2uid_table_entry *ptr;
   char *dup;
   size_t len;

   ptr = malloc(sizeof(struct nam2uid_table_entry));
   dup = strdup(s);
   if (!ptr || !dup) {
      Warning("malloc error");
      exit(-1);
   }

   ptr->key = dup;
   ptr->data = ((uid_t)-1);
   ptr->known = 0;
   len = strlen(ptr->key);

   HASH_ADD_KEYPTR(hh, nam2uid_table, ptr->key, len, ptr);

   return ptr;
}

static 
struct nam2uid_table_entry *nam2uid_find(const char *s)
{
   struct nam2uid_table_entry *ptr;
   size_t len;

   len = strlen(s);

   HASH_FIND(hh, nam2uid_table, s, len, ptr);
   return ptr;
}


int map_name_to_uid(const char *s, uid_t *uid)
{
   struct nam2uid_table_entry *ptr;
   struct passwd *uid_entry;

   ptr = nam2uid_find(s);
   if (!ptr) {
      ptr = nam2uid_add(s);
      uid_entry = getpwnam(s);
      if (uid_entry) {
         ptr->data = uid_entry->pw_uid;
         ptr->known = 1;
      }
   }

   if (ptr->known) {
      *uid = ptr->data;
      return 0;
   }
   else {
      return -1;
   }
}

/* hash table to map names to gids */

struct nam2gid_table_entry {
   char *key;
   gid_t data;
   int known;
   UT_hash_handle hh;
};

static 
struct nam2gid_table_entry *nam2gid_table = NULL;

static 
struct nam2gid_table_entry *nam2gid_add(const char *s)
{
   struct nam2gid_table_entry *ptr;
   char *dup;
   size_t len;

   ptr = malloc(sizeof(struct nam2gid_table_entry));
   dup = strdup(s);
   if (!ptr || !dup) {
      Warning("malloc error");
      exit(-1);
   }

   ptr->key = dup;
   ptr->data = ((gid_t)-1);
   ptr->known = 0;
   len = strlen(ptr->key);

   HASH_ADD_KEYPTR(hh, nam2gid_table, ptr->key, len, ptr);

   return ptr;
}

static 
struct nam2gid_table_entry *nam2gid_find(const char *s)
{
   struct nam2gid_table_entry *ptr;
   size_t len;

   len = strlen(s);

   HASH_FIND(hh, nam2gid_table, s, len, ptr);
   return ptr;
}


int map_name_to_gid(const char *s, gid_t *gid)
{
   struct nam2gid_table_entry *ptr;
   struct group *gid_entry;

   ptr = nam2gid_find(s);
   if (!ptr) {
      ptr = nam2gid_add(s);
      gid_entry = getgrnam(s);
      if (gid_entry) {
         ptr->data = gid_entry->gr_gid;
         ptr->known = 1;
      }
   }

   if (ptr->known) {
      *gid = ptr->data;
      return 0;
   }
   else {
      return -1;
   }
}



/* hash table to map uuids to ids */

struct uuid2id_table_entry {
   uuid_t key;
   uid_t data;
   int type;
   int known;
   UT_hash_handle hh;
};

static 
struct uuid2id_table_entry *uuid2id_table = NULL;

static 
struct uuid2id_table_entry *uuid2id_add(uuid_t uu)
{
   struct uuid2id_table_entry *ptr;

   ptr = malloc(sizeof(struct uuid2id_table_entry));
   if (!ptr) {
      Warning("malloc error");
      exit(-1);
   }

   memcpy(ptr->key, uu, sizeof(uuid_t));
   ptr->data = ((uid_t)-1);
   ptr->type = -1;
   ptr->known = 0;

   HASH_ADD_KEYPTR(hh, uuid2id_table, ptr->key, sizeof(uuid_t), ptr);

   return ptr;
}

static 
struct uuid2id_table_entry *uuid2id_find(uuid_t uu)
{
   struct uuid2id_table_entry *ptr;

   HASH_FIND(hh, uuid2id_table, uu, sizeof(uuid_t), ptr);
   return ptr;
}


int map_uuid_to_id(uuid_t uu, uid_t *uid, int *id_type)
{
   struct uuid2id_table_entry *ptr;

   ptr = uuid2id_find(uu);
   if (!ptr) {
      ptr = uuid2id_add(uu);
      if (mbr_uuid_to_id(uu, &ptr->data, &ptr->type) == 0) {
         ptr->known = 1;
      }
   }

   if (ptr->known) {
      *uid = ptr->data;
      *id_type = ptr->type;
      return 0;
   }
   else {
      return -1;
   }
}

/* hash table to map uids to uuids */

struct uid2uuid_table_entry {
   uid_t key;
   uuid_t data;
   int known;
   UT_hash_handle hh;
};

static 
struct uid2uuid_table_entry *uid2uuid_table = NULL;

static 
struct uid2uuid_table_entry *uid2uuid_add(uid_t uid)
{
   struct uid2uuid_table_entry *ptr;

   ptr = malloc(sizeof(struct uid2uuid_table_entry));
   if (!ptr) {
      Warning("malloc error");
      exit(-1);
   }

   ptr->key = uid;
   bzero(ptr->data, sizeof(uuid_t));
   ptr->known = 0;

   HASH_ADD_KEYPTR(hh, uid2uuid_table, &ptr->key, sizeof(uid_t), ptr);

   return ptr;
}

static 
struct uid2uuid_table_entry *uid2uuid_find(uid_t uid)
{
   struct uid2uuid_table_entry *ptr;

   HASH_FIND(hh, uid2uuid_table, &uid, sizeof(uid_t), ptr);
   return ptr;
}


int map_uid_to_uuid(uid_t uid, uuid_t uuid)
{
   struct uid2uuid_table_entry *ptr;

   ptr = uid2uuid_find(uid);
   if (!ptr) {
      ptr = uid2uuid_add(uid);
      if (mbr_uid_to_uuid(uid, ptr->data) == 0) {
         ptr->known = 1;
      }
   }

   if (ptr->known) {
      memcpy(uuid, ptr->data, sizeof(uuid_t));
      return 0;
   }
   else {
      return -1;
   }
}

/* hash table to map gids to uuids */

struct gid2uuid_table_entry {
   gid_t key;
   uuid_t data;
   int known;
   UT_hash_handle hh;
};

static 
struct gid2uuid_table_entry *gid2uuid_table = NULL;

static 
struct gid2uuid_table_entry *gid2uuid_add(gid_t gid)
{
   struct gid2uuid_table_entry *ptr;

   ptr = malloc(sizeof(struct gid2uuid_table_entry));
   if (!ptr) {
      Warning("malloc error");
      exit(-1);
   }

   ptr->key = gid;
   bzero(ptr->data, sizeof(uuid_t));
   ptr->known = 0;

   HASH_ADD_KEYPTR(hh, gid2uuid_table, &ptr->key, sizeof(gid_t), ptr);

   return ptr;
}

static 
struct gid2uuid_table_entry *gid2uuid_find(gid_t gid)
{
   struct gid2uuid_table_entry *ptr;

   HASH_FIND(hh, gid2uuid_table, &gid, sizeof(gid_t), ptr);
   return ptr;
}


int map_gid_to_uuid(gid_t gid, uuid_t uuid)
{
   struct gid2uuid_table_entry *ptr;

   ptr = gid2uuid_find(gid);
   if (!ptr) {
      ptr = gid2uuid_add(gid);
      if (mbr_gid_to_uuid(gid, ptr->data) == 0) {
         ptr->known = 1;
      }
   }

   if (ptr->known) {
      memcpy(uuid, ptr->data, sizeof(uuid_t));
      return 0;
   }
   else {
      return -1;
   }
}

/****** user mapping stuff ******/

/* first, a hash table to translate uids to uids */

struct uid2uid_table_entry {
   uid_t key;
   uid_t data;
   UT_hash_handle hh;
};

static 
struct uid2uid_table_entry *uid2uid_table = NULL;

static 
struct uid2uid_table_entry *uid2uid_add(uid_t key, uid_t data)
{
   struct uid2uid_table_entry *ptr;

   ptr = malloc(sizeof(struct uid2uid_table_entry));
   if (!ptr) {
      Warning("malloc error");
      exit(-1);
   }

   ptr->key = key;
   ptr->data = data;

   HASH_ADD_KEYPTR(hh, uid2uid_table, &ptr->key, sizeof(uid_t), ptr);

   return ptr;
}

static 
struct uid2uid_table_entry *uid2uid_find(uid_t key)
{
   struct uid2uid_table_entry *ptr;

   HASH_FIND(hh, uid2uid_table, &key, sizeof(uid_t), ptr);
   return ptr;
}


int translate_uid(uid_t *uid)
{
   struct uid2uid_table_entry *ptr;

   ptr = uid2uid_find(*uid);
   if (!ptr) {
      return 0;
   }
   else {
      *uid = ptr->data;
      return 1;
   }
}

static
void process_usermap_pair(const char *src, const char *dst)
{
   int src_isnum, dst_isnum;
   uid_t src_val, dst_val;
   struct passwd *ptr;
   struct nam2uid_table_entry *tptr;

   if (*src == '\0' || *dst == '\0') {
      WARN("usermap error \"%s:%s\" -- empty user name\n", src, dst);
      exit(-1);
   }

   src_val = string_to_long(src);
   src_isnum = (!conversion_error || errno == ERANGE);

   dst_val = string_to_long(dst);
   dst_isnum = (!conversion_error || errno == ERANGE);

   if (!dst_isnum) {
      ptr = getpwnam(dst);
      if (!ptr) {
         WARN("usermap error \"%s:%s\" -- unkowwn user %s\n", src, dst, dst);
         exit(-1);
      }
      dst_val = ptr->pw_uid;
   }

   if (src_isnum) {
      if (uid2uid_find(src_val)) {
         WARN("usermap error \"%s:%s\" -- user %s mapped twice\n", src, dst, src);
         exit(-1);
      }
      uid2uid_add(src_val, dst_val);
   }
   else {
      if (!getpwuid(dst_val)) {
         WARN("usermap error \"%s:%s\" -- unknown user %s\n", src, dst, dst);
         exit(-1);
      }
      tptr = nam2uid_find(src);
      if (tptr) {
         WARN("usermap error \"%s:%s\" -- %s mapped twice\n", src, dst, src);
         exit(-1);
      }
      tptr = nam2uid_add(src);
      tptr->data = dst_val;
      tptr->known = 1;
   }
}


void process_usermap(char *str)
{
   char *pair, *src, *dst;

   while ((pair = strsep(&str, ","))) {
      if (*pair == '\0') continue;
      src = strsep(&pair, ":");
      if ((dst = strsep(&pair, ":")) && !pair) {
         process_usermap_pair(src, dst);
         // fprintf(stderr, "|%s|%s|\n", src, dst);
      }
      else {
         if (dst) dst[-1] = ':';
         if (pair) pair[-1] = ':';
         WARN("bad user pair \"%s\"\n", src); 
         exit(-1);
      }
   }


#if 0
   {
      struct nam2uid_table_entry *p = nam2uid_table;


      fprintf(stderr, "\nnam2uid table:\n");
      while (p) {
         fprintf(stderr, "%s:%d:%d\n", p->key, p->data, p->known);
         p = p->hh.next;
      }

   }

   {
      struct uid2uid_table_entry *p = uid2uid_table;


      fprintf(stderr, "\nuid2uid table:\n");
      while (p) {
         fprintf(stderr, "%d:%d\n", p->key, p->data);
         p = p->hh.next;
      }

   }

#endif 

}



/****** group mapping stuff ******/

/* first, a hash table to translate gids to gids */

struct gid2gid_table_entry {
   gid_t key;
   gid_t data;
   UT_hash_handle hh;
};

static 
struct gid2gid_table_entry *gid2gid_table = NULL;

static 
struct gid2gid_table_entry *gid2gid_add(gid_t key, gid_t data)
{
   struct gid2gid_table_entry *ptr;

   ptr = malloc(sizeof(struct gid2gid_table_entry));
   if (!ptr) {
      Warning("malloc error");
      exit(-1);
   }

   ptr->key = key;
   ptr->data = data;

   HASH_ADD_KEYPTR(hh, gid2gid_table, &ptr->key, sizeof(gid_t), ptr);

   return ptr;
}

static 
struct gid2gid_table_entry *gid2gid_find(gid_t key)
{
   struct gid2gid_table_entry *ptr;

   HASH_FIND(hh, gid2gid_table, &key, sizeof(gid_t), ptr);
   return ptr;
}


int translate_gid(gid_t *gid)
{
   struct gid2gid_table_entry *ptr;

   ptr = gid2gid_find(*gid);
   if (!ptr) {
      return 0;
   }
   else {
      *gid = ptr->data;
      return 1;
   }
}

static
void process_groupmap_pair(const char *src, const char *dst)
{
   int src_isnum, dst_isnum;
   gid_t src_val, dst_val;
   struct group *ptr;
   struct nam2gid_table_entry *tptr;

   if (*src == '\0' || *dst == '\0') {
      WARN("groupmap error \"%s:%s\" -- empty group name\n", src, dst);
      exit(-1);
   }


   src_val = string_to_long(src);
   src_isnum = (!conversion_error || errno == ERANGE);

   dst_val = string_to_long(dst);
   dst_isnum = (!conversion_error || errno == ERANGE);

   if (!dst_isnum) {
      ptr = getgrnam(dst);
      if (!ptr) {
         WARN("groupmap error \"%s:%s\" -- unkowwn group %s\n", src, dst, dst);
         exit(-1);
      }
      dst_val = ptr->gr_gid;
   }

   if (src_isnum) {
      if (gid2gid_find(src_val)) {
         WARN("groupmap error \"%s:%s\" -- group %s mapped twice\n", src, dst, src);
         exit(-1);
      }
      gid2gid_add(src_val, dst_val);
   }
   else {
      if (!getgrgid(dst_val)) {
         WARN("groupmap error \"%s:%s\" -- unknown group %s\n", src, dst, dst);
         exit(-1);
      }
      tptr = nam2gid_find(src);
      if (tptr) {
         WARN("groupmap error \"%s:%s\" -- %s mapped twice\n", src, dst, src);
         exit(-1);
      }
      tptr = nam2gid_add(src);
      tptr->data = dst_val;
      tptr->known = 1;
   }
}


void process_groupmap(char *str)
{
   char *pair, *src, *dst;


   while ((pair = strsep(&str, ","))) {
      if (*pair == '\0') continue;
      src = strsep(&pair, ":");
      if ((dst = strsep(&pair, ":")) && !pair) {
         process_groupmap_pair(src, dst);
         // fprintf(stderr, "|%s|%s|\n", src, dst);
      }
      else {
         if (dst) dst[-1] = ':';
         if (pair) pair[-1] = ':';
         WARN("bad group pair \"%s\"\n", src); 
         exit(-1);
      }
   }

#if 0

   {
      struct nam2gid_table_entry *p = nam2gid_table;


      fprintf(stderr, "\nnam2gid table:\n");
      while (p) {
         fprintf(stderr, "%s:%d:%d\n", p->key, p->data, p->known);
         p = p->hh.next;
      }

   }

   {
      struct gid2gid_table_entry *p = gid2gid_table;


      fprintf(stderr, "\ngid2gid table:\n");
      while (p) {
         fprintf(stderr, "%d:%d\n", p->key, p->data);
         p = p->hh.next;
      }

   }

#endif 

}




/*****************************************/

void overflow(void) 
{
   WARN("buffer overrun -- terminating\n");
   exit(-1);
}




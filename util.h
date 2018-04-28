#ifndef XBUP__util_H
#define XBUP__util_H

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/time.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <membership.h>


#define WARNING (fprintf(stderr, "%s %d: ", __FILE__, __LINE__), perror(""))
#define Warning(x) (fprintf(stderr, "%s %d: %s", __FILE__, __LINE__, x))
#define WARN(...) (fprintf(stderr, __VA_ARGS__ ))


/* The following casts an uint32_t to an int32_t
 * in a portable fashion (assuming 2's comp).
 * An optimizing compiler will convert to to the identity function.
 */


#define CAST_s32_u32(a) \
   (((a) >= (((uint32_t) INT32_MIN))) ? \
    (((int32_t) ((a) - ((uint32_t) INT32_MIN))) + \
       INT32_MIN) : \
    ((int32_t) (a)))


/* The following is the same as above, but for longs. */

#define CAST_sl_ul(a) \
   (((a) >= (((unsigned long) LONG_MIN))) ? \
    (((int32_t) ((a) - ((unsigned long) LONG_MIN))) + \
       LONG_MIN) : \
    ((int32_t) (a)))


/* The following casts a uint32_t to an integral type t, extended the sign
   bit if necessary.  The argument x is always cast to a uint32_t.
 */

#define CAST_u32(t,x) ((((t) -1) < ((t)0)) ?  \
   ((t)(CAST_s32_u32((uint32_t)(x)))) : ((t) ((uint32_t)(x))))


/* The following casts an integral type t to long, preserving the
 * "sign bit" of its argument x, even if t is an unsigned type.
 * If type t is longer than long, x is truncated.
 * Portable (assuming 2's comp).
 * 
 * Invariant: if t is not longer than long, then 
 *  (t) CAST_to_long(t, x) == x
 */

#define CAST_to_long(t, x) \
   ( (((t) -1) < ((t) 0)) ? \
     CAST_sl_ul((unsigned long) ((t)(x))) : \
     CAST_sl_ul( (((unsigned long) ((t)(x))) ^ CAST_MASK(t)) - CAST_MASK(t) ) ) 


/* These are auxilliary macros used by CAST_to_long.
 * CAST_MASK(t) = 
 *   an unsigned long with a 1 in the sign bit position of t 
 *      (if unsigned long is wider than t)
 *   0 (otherwise).
 */

#define CAST_MASK(t) (CAST_MASK1(t) ^ (CAST_MASK1(t) >> 1))
#define CAST_MASK1(t) (((unsigned long) -1) ^ ((unsigned long) ((t) -1)))
   
   




#define MAXLEN (2048)


#define DBL_PREFIX "@_"
#define DBL_PREFIX_LEN (2)

#define DBL_SUFFIX ".__@"
#define DBL_SUFFIX_LEN (4)

#define S_ISREG_OR_LNK(m) (S_ISREG(m) || S_ISLNK(m))

extern int xbup_opt_preserve_uuids;
extern int xbup_opt_numeric_ids;

long string_to_long(const char *s);
extern int conversion_error;

int strip_slashes(char *s);


int is_prefix(const char *pat, const char *txt);

int is_suffix(const char *pat, long patlen, const char *txt, long txtlen);


void collect_names(const char *fname);
int lookup_name(const char *s);

char *map_uid_to_name(uid_t uid); // NULL if fail
char *map_gid_to_name(gid_t gid); // NULL if fail
int map_name_to_uid(const char *s, uid_t *uid); // non-zero on fail
int map_name_to_gid(const char *s, gid_t *gid); // non-zero on fail
int map_uuid_to_id(uuid_t uu, uid_t *uid, int *id_type); // non-zero on fail
int map_uid_to_uuid(uid_t uid, uuid_t uuid);  // non-zero on fail
int map_gid_to_uuid(gid_t uid, uuid_t uuid);  // non-zero on fail

void process_usermap(char *str);
void process_groupmap(char *str);

int translate_uid(uid_t *uid); // 1 if mapped, 0 o/w
int translate_gid(gid_t *gid); // 1 if mapped, 0 o/w



void overflow(void);


#endif


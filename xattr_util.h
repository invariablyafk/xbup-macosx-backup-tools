#ifndef XBUP__xattr_util_H
#define XBUP__xattr_util_H

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/attr.h>
#include <sys/time.h>
#include <sys/acl.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

extern int xattr_access_error;

struct owner_prefs_struct {
   int u_keep, u_default;
   uid_t uid;
   int g_keep, g_default;
   gid_t gid;
};


typedef struct owner_prefs_struct owner_prefs_t;

int set_owner_prefs(owner_prefs_t *oprefs,
                    const char *owner_name, const char *group_name);

static inline int save_owner(const owner_prefs_t *oprefs, const struct stat *sbuf)
{
   return oprefs->u_keep && 
          (oprefs->u_default == 0 || oprefs->uid != sbuf->st_uid);
}

static inline int restore_owner(const owner_prefs_t *oprefs, const struct stat *sbuf)
{
   return oprefs->u_keep && 
          (oprefs->u_default == 1 && oprefs->uid != sbuf->st_uid);
}

static inline int save_group(const owner_prefs_t *oprefs, const struct stat *sbuf)
{
   return oprefs->g_keep && 
          (oprefs->g_default == 0 || oprefs->gid != sbuf->st_gid);
}

static inline int restore_group(const owner_prefs_t *oprefs, const struct stat *sbuf)
{
   return oprefs->g_keep && 
          (oprefs->g_default == 1 && oprefs->gid != sbuf->st_gid);
}


int get_crtime(const char *path, time_t* t);
int set_crtime(const char *path, time_t t);

int set_mtime(const char *fname, time_t t);

#define SPECIAL_CHMOD_BITS (S_ISUID | S_ISGID | S_ISVTX)
#define CHMOD_BITS (S_IRWXU | S_IRWXG | S_IRWXO | SPECIAL_CHMOD_BITS)

/* problem_perms determines if the permissions may cause
   problems when stored on a remote server.
   A "problematic" object is either:
     1) a regular file that is not owner readable/writable
     2) a directory that is not owner readable/writable/executable
     3) an object with any special bits set (setuid, setgid, or sticky)
 */

static inline int problem_perms(const struct stat *sbuf)
{
   mode_t mode = sbuf->st_mode;

   if ((mode & S_IRUSR) == 0 || (mode & S_IWUSR) == 0) return 1; 
   if (S_ISDIR(mode) && (mode & S_IXUSR) == 0) return 1;
   if ((mode & SPECIAL_CHMOD_BITS) != 0) return 1;
   return 0;
}



static inline int normal_object(const struct stat *sbuf)
{
   return S_ISREG(sbuf->st_mode) || S_ISDIR(sbuf->st_mode) ||
          S_ISLNK(sbuf->st_mode);
}


#define CHFLAGS_BITS (UF_NODUMP | UF_IMMUTABLE | UF_APPEND | UF_OPAQUE)

static inline int has_locks(const struct stat *sbuf)
{
   return (sbuf->st_flags & CHFLAGS_BITS) != 0;
}

int remove_locks(const char *fname, const struct stat *sbuf);


int has_xattr(const char *fname, const struct stat *namebuf);

int strip_xattr(const char *fname, const struct stat *sbuf);

int split_xattr(const char *fname, const struct stat *sbuf,
                const char *cname, int crtimeflag, int mtimeflag, acl_t acl,
                int saveperms, const owner_prefs_t *oprefs);

acl_t get_acl(const char *fname, const struct stat *sbuf);
int strip_acl(const char *fname, const struct stat *sbuf);

int has_acl(const char *fname, const struct stat *sbuf);

int join_xattr(const char *fname, const struct stat *sbuf, const char *cname,
               int aclflag, const owner_prefs_t *oprefs);

int skip_xattr(const char *cname);

static inline 
int need_container(const char *fname, const struct stat *sbuf,
                   int crtimeflag, int savemtime, acl_t acl,
                   int saveperms, const owner_prefs_t *oprefs)
{
   return 
      crtimeflag || 
      savemtime ||
      saveperms || 
      acl || 
      has_locks(sbuf) || 
      save_owner(oprefs, sbuf) || save_group(oprefs, sbuf) ||
      has_xattr(fname, sbuf); 

}

static inline
int need_reset(const char *fname, const struct stat *sbuf, int aclflag,
               const owner_prefs_t *oprefs)
{
   xattr_access_error = 0;
   return
      has_xattr(fname, sbuf) ||
      (aclflag && has_acl(fname, sbuf)) ||
      xattr_access_error ||
      has_locks(sbuf) ||
      restore_owner(oprefs, sbuf) ||
      restore_group(oprefs, sbuf);
}


#endif


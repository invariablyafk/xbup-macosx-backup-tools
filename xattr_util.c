

#include "util.h"
#include "xattr_util.h"
#include "xbup_acl_translate.h"


int xattr_access_error = 0;
  /* This gets set whenever an attempt is made to access xattrs
     that is denied for lack of permissions.
     set by: has_xattr, split_xattr

     NOTE: for ACLs, one can deny permission by setting an ACE
     that denies readsecurity -- however, this apparently already
     results in an lstat failure...in any event, the xbup code
     is written to assume that has_acl, get_acl (and indirectly, 
     split_xattr) could set this variable.

     In the future, it may be necessary to modify the code
     in get_acl and strip_acl to check for access errors,
     if the current behavior changes....
   */

#define BUFSIZE (1024)
#define MAXNAME (4*1024)

static char name_buffer[MAXNAME];





#ifdef __LP64__
#define PACKED __attribute__((packed))
#else
#define PACKED
#endif



struct attrbuf {
   uint32_t length;
   struct timespec ts;
} PACKED ;

/* NOTE: the man page for getattrlist says length should be an unsigned long,
 * but the code says its a uint32_t.  This only matters on a 
 * 64-bit machine, of course.
 * 
 * Right now, this will work even if compiled with -m64.
 * Note the use of packing to overcome alignment issues.
 *
 */
   

int get_crtime(const char *path, time_t* t)
{
   struct attrlist attrList;
   struct attrbuf  attrBuf;
   int err;

   memset(&attrList, 0, sizeof(attrList));

   attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
   attrList.commonattr  = ATTR_CMN_CRTIME;

   err = getattrlist(path, &attrList, &attrBuf, sizeof(attrBuf), FSOPT_NOFOLLOW);
   if (err == 0) {
      *t = attrBuf.ts.tv_sec;
      return 0;
   }
   else
      return -1;
}

/* NOTE: setting crtime on symlinks does nothing */

int set_crtime(const char *path, time_t t)
{
   struct attrlist attrList;
   struct attrbuf  attrBuf;
   int err;

   attrBuf.ts.tv_sec = t;
   attrBuf.ts.tv_nsec = 0;

   memset(&attrList, 0, sizeof(attrList));

   attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
   attrList.commonattr  = ATTR_CMN_CRTIME;

   err = setattrlist(path, &attrList, &attrBuf.ts, sizeof(attrBuf.ts), FSOPT_NOFOLLOW);

   return err;
}


/* the following works like chflags, but applies to symlinks as well:
 * on HFS+ filesystems, symlinks can have BSD flags.
 */

/* NOTE: the man page for getattrlist says t should be an unsigned long,
 * but the code says its a uint32_t.
 */

int hfs_chflags(const char *path, uint32_t t)
{
   struct attrlist attrList;
   int err;

   memset(&attrList, 0, sizeof(attrList));

   attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
   attrList.commonattr  = ATTR_CMN_FLAGS;

   err = setattrlist(path, &attrList, &t, sizeof(t), FSOPT_NOFOLLOW);

   return err;
}


/* the following works like chmod, but applies to symlinks as well.
 */

/* NOTE: the man page for getattrlist says t should be a mode_t,
 * but the code says its a uint32_t.  Since mode_t is a 16-bit integer,
 * this will *fail* if t is declared mode_t.
 */


int hfs_chmod(const char *path, mode_t tt)
{
   uint32_t t = tt;
   struct attrlist attrList;
   int err;


   memset(&attrList, 0, sizeof(attrList));

   attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
   attrList.commonattr  = ATTR_CMN_ACCESSMASK;

   err = setattrlist(path, &attrList, &t, sizeof(t), FSOPT_NOFOLLOW); 

   return err;
}




/* set_mtime does not follow symlinks,
 * but does not seem to actually set the time on symlinks either.
 */


int set_mtime(const char *path, time_t tt)
{
   struct attrlist attrList;
   int err;

   struct timespec t;

   t.tv_sec = tt;
   t.tv_nsec = 0;

   memset(&attrList, 0, sizeof(attrList));

   attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
   attrList.commonattr  = ATTR_CMN_MODTIME;

   err = setattrlist(path, &attrList, &t, sizeof(t), FSOPT_NOFOLLOW); 

   return err;
}


/* make_writable makes the file readable and writable */

int make_writable(const char *fname, const struct stat *sbuf)
{
   return hfs_chmod(fname, sbuf->st_mode | 
                           S_IWUSR | S_IWGRP | S_IWOTH |
                           S_IRUSR | S_IRGRP | S_IROTH); 
}



int remove_locks(const char *fname, const struct stat *sbuf)
{
   return hfs_chflags(fname, sbuf->st_flags & (~CHFLAGS_BITS));
}


/* Code for getting and setting ACLs.
 * There is some Apple brain damage regard symlinks.
 * Apparently, symlinks can have ACLs (you can set them
 * via the GUI in 10.5, by applying the permissions
 * of a folder to its contents).
 *
 * The function acl_get_link_np will correctly fetch the ACL
 * of either a symlink of non-symlink.
 * Unfortunately, the function acl_set_link_np does not
 * work at all -- it will always fail, even if the input is
 * not a symlink.
 * The function acl_set_fd_np *will* set the ACL of a symlink;
 * however, I only know how to get a file descriptor for a symlink
 * in OSX v10.5, where open has the O_SYMLINK flag.
 *
 * So the current implementation will fail to set the ACL of 
 * a symlink in 10.4, which is probably OK.
 *
 * More Apple brain damage:  the acl_delete functions are all
 * broken, even in 10.5 -- so the only way to clear an ACL
 * is to set it to the "empty" ACL.
 */

acl_t get_acl(const char *fname, const struct stat *sbuf)
{
   acl_t acl;
   acl_entry_t dummy;

   acl = acl_get_link_np(fname, ACL_TYPE_EXTENDED);
   if (acl && acl_get_entry(acl, ACL_FIRST_ENTRY, &dummy) == -1) {
      acl_free(acl);
      acl = 0;
   }

   return acl;
}

int put_acl(const char *fname, const struct stat *sbuf, acl_t acl)
{
   int retval = -1;

   if (!S_ISLNK(sbuf->st_mode)) {
      retval = acl_set_file(fname, ACL_TYPE_EXTENDED, acl);
   }
   else {

#ifdef O_SYMLINK

      // This is the only way I know how to really set an ACL
      // on a symlink -- unfortunately, the O_SYMLINK
      // flag is only available in OSX v10.5, and not in 
      // v10.4.

      int fd;
      fd = open(fname, O_SYMLINK);
      if (fd >= 0) {
         retval = acl_set_fd_np(fd, acl, ACL_TYPE_EXTENDED);
         close(fd);
      }

#else

      // The documentation says this should work, but it does not.
      // In fact, it always fails, even for non-symlinks

      // retval = acl_set_link_np(fname, ACL_TYPE_EXTENDED, acl);

      // It gets even worse: this function is not even declared
      // in the header file in v10.4...instead, acl_set_link
      // is declared, but acl_set_link_np is defined in the library.
      // Let's just fall through and fail for now....

#endif

   }

   return retval;
}


int strip_acl(const char *fname, const struct stat *sbuf)
{
   acl_t empty_acl = 0;
   acl_t acl = 0;
   int retval = 0;
   acl_entry_t dummy;

   acl = acl_get_link_np(fname, ACL_TYPE_EXTENDED);
   if (acl && acl_get_entry(acl, ACL_FIRST_ENTRY, &dummy) != -1) {
      empty_acl = acl_init(0);
      if (put_acl(fname, sbuf, empty_acl)) retval = -1;
   }

   if (empty_acl) acl_free(empty_acl);
   if (acl) acl_free(acl);
   return retval;
}

/*
 * checks if acl is present 
 */

int has_acl(const char *fname, const struct stat *sbuf)
{
   acl_t acl = 0;
   int retval = 0;

   acl = get_acl(fname, sbuf);
   if (acl) retval = 1;

   if (acl) acl_free(acl);
   return retval;
}


int set_owner_prefs(owner_prefs_t *oprefs,
                    const char *owner_name, const char *group_name)
{
   int retval = 0;
   long val;
   int isnum;

   if (!owner_name) {
      oprefs->u_keep = 0;
   }
   else {
      oprefs->u_keep = 1;
      if (strcmp(owner_name, "-") == 0) {
         oprefs->u_default = 0;
      }
      else {
         oprefs->u_default = 1;
         oprefs->uid = (uid_t) -1;

         val = string_to_long(owner_name);
         isnum = (!conversion_error || errno == ERANGE);

         if (isnum) {
            uid_t uid = (uid_t) val;
            translate_uid(&uid);
            oprefs->uid = uid;
         }
         else if (*owner_name == '\0' ||
                  map_name_to_uid(owner_name, &oprefs->uid)) {
            oprefs->u_default = 0;
            retval |= 1;
         }
      }
   }

   if (!group_name) {
      oprefs->g_keep = 0;
   }
   else {
      oprefs->g_keep = 1;
      if (strcmp(group_name, "-") == 0) {
         oprefs->g_default = 0;
      }
      else {
         oprefs->g_default = 1;
         oprefs->gid = (gid_t) -1;

         val = string_to_long(group_name);
         isnum = (!conversion_error || errno == ERANGE);

         if (isnum) {
            gid_t gid = (gid_t) val;
            translate_gid(&gid);
            oprefs->gid = gid;
         }
         else if (*group_name == '\0' || 
                  map_name_to_gid(group_name, &oprefs->gid)) {
            oprefs->g_default = 0;
            retval |= 2;
         }
      }
   }

   return retval;
}




static
int write_int4(uint32_t x, FILE *f)
{
   unsigned char buf[4];

   buf[3] = x & 0xffu; x = x >> 8;
   buf[2] = x & 0xffu; x = x >> 8;
   buf[1] = x & 0xffu; x = x >> 8;
   buf[0] = x & 0xffu; x = x >> 8;

   if (fwrite(buf, 1, 4, f) == 4)
      return 0;
   else
      return -1;
}

static
int read_int4(uint32_t *xp, FILE *f)
{
   unsigned char buf[4];
   uint32_t x;

   if (fread(buf, 1, 4, f) != 4)
      return -1;

   x = 0;
   x = x << 8; x += buf[0] & 0xffu;
   x = x << 8; x += buf[1] & 0xffu;
   x = x << 8; x += buf[2] & 0xffu;
   x = x << 8; x += buf[3] & 0xffu;

   *xp = x;
   return 0;
}

static
int write_int2(uint16_t x, FILE *f)
{
   unsigned char buf[2];

   buf[1] = x & 0xffu; x = x >> 8;
   buf[0] = x & 0xffu; x = x >> 8;

   if (fwrite(buf, 1, 2, f) == 2)
      return 0;
   else
      return -1;
}

static
int read_int2(uint16_t *xp, FILE *f)
{
   unsigned char buf[2];
   uint32_t x;

   if (fread(buf, 1, 2, f) != 2)
      return -1;

   x = 0;
   x = x << 8; x += buf[0] & 0xffu;
   x = x << 8; x += buf[1] & 0xffu;

   *xp = x;
   return 0;
}

static
int write_int1(uint8_t x, FILE *f)
{
   unsigned char buf[1];

   buf[0] = x & 0xffu; x = x >> 8;

   if (fwrite(buf, 1, 1, f) == 1)
      return 0;
   else
      return -1;
}

static
int read_str(char *buf, int len, FILE *f)
{
   int c, k;

   k = 0;
   for (;;) {
      c = getc(f);
      if (c == EOF) return -1;
      if (k >= len) return -1;
      buf[k] = c;
      k++;
      if (c == 0) return 0;
   }
}

static
char * read_str1(FILE *f)
{
   int c, k;

   char *buf = 0;
   int len = 0;

   char *newbuf;

   k = 0;
   for (;;) {
      c = getc(f);
      if (c == EOF) goto bad;

      if (k >= len) {
         if (len == 0) {
            len = 64;
            newbuf = malloc(len);
         }
         else {
            len = 2 * len;
            newbuf = realloc(buf, len);
         }
         if (!newbuf) goto bad;
         buf = newbuf;
      }

      buf[k] = c;
      k++;
      if (c == 0) return buf;
   }

bad:

   if (buf) free(buf);
   return 0;
}





/* Container format: 
 *  - header (10 bytes):
 *      - magic (8 bytes)
 *      - version (2 bytes)
 *
 *   - perms bits (2 bytes) -- included if PERMS_FLAG 
 *
 *   - bsd flags (2 bytes) -- included if LOCKS_FLAG 
 *
 *   - create time (4 bytes) -- included if CRTIME_FLAG 
 *
 *   - mod time (4 bytes) -- included if MTIME_FLAG
 *
 *   - owner:  -- included if OWNER_FLAG 
 *       - name (null-terminated string)
 *       - uid (4 bytes)
 *
 *   - group:  -- included if GROUP_FLAG 
 *       - name (null-terminated string)
 *       - gid (4 bytes)
 *
 *  -  acl:   -- included if ACLTEXT_FLAG
 *       - null terminated string in apple acl_to_text format
 *
 *  - xattr list --included if XAT_FLAG 
 *     - number of xattrs (2 bytes) 
 *     - for each xattr:
 *         - name (null-terminated string)
 *         - attrlen (4 bytes)
 *         - attr (attrlen bytes)
 * 
 * All numbers in "network byte order" (high-order byte first)
 */

#define MAGIC1 (0x30bc83f9u)
#define MAGIC2 (0x22f0f8dfu)

/* version is a 16-bit integer.  The low-order 4 bits are reserved
 * for the version number, and the remaining bits for various flags.
 */

#define VERSION (2)
#define VERSION_MASK     (0x000f)
#define PERMS_FLAG       (0x0010)
#define LOCKS_FLAG       (0x0020)
#define CRTIME_FLAG      (0x0040)
#define MTIME_FLAG       (0x0080)
#define OWNER_FLAG       (0x0100)
#define GROUP_FLAG       (0x0200)
#define ACLTEXT_FLAG     (0x0400)
#define XAT_FLAG         (0x0800)

int write_header(uint16_t v, FILE *f)
{
   if (write_int4(MAGIC1, f) || write_int4(MAGIC2, f) || write_int2(v, f))
      return -1;
   else
      return 0;
}

int read_header(uint16_t *vp, FILE *f)
{
   uint32_t m1, m2;
   uint16_t v;

   if (read_int4(&m1, f) || read_int4(&m2, f) || read_int2(&v, f))
      return -1;

   if (m1 != MAGIC1 || m2 != MAGIC2)
      return -1;

   *vp = v;
   return 0;
}



/* read xattr's from file fname and store in container cname. 
 *    cname == "" => xattr's written to stdout
 * sbuf: should be stat struct for fname
 *
 * retval: -1 on error
 *         0 otherwise
 *
 * design options: follow symlinks? no
 *                 create container if no xattr's? yes
 */



int split_xattr(const char *fname, const struct stat *sbuf, const char *cname, 
                int crtimeflag, int savemtime, acl_t acl,
                int saveperms, const owner_prefs_t* oprefs)
{
   char *namebuf=0, *attrbuf=0;
   FILE *cfp=0;
   char *acltext=0;

   int retval = -1;

   char *attrname, *bp;
   long numxattrs, i, namesz, attrnamesz, attrsz, bufsize;

   ssize_t acltextsz = 0;

   uint16_t v;
   uint16_t bsd_flags;
   time_t crtime;


   namesz = listxattr(fname, 0, 0, XATTR_NOFOLLOW);

   /* NOTE: if namesz <= 0 (which is the same criteria used in has_xattr)
    * then the file will be treated as if it has no xattrs.
    * In particular, this will be the case if the file is unreadable.
    */
   
   numxattrs = 0;
   bufsize = 0;

   if (namesz > 0) {
      namebuf = (char *) malloc(namesz);
      if (!namebuf) {
         WARNING;
         goto done;
      }

      bufsize = BUFSIZE;
      attrbuf = (char *) malloc(BUFSIZE);
      if (!attrbuf) {
         WARNING;
         goto done; 
      }

      if (listxattr(fname, namebuf, namesz, XATTR_NOFOLLOW) != namesz) {
         WARNING;
         goto done;
      }

      for (i = 0; i < namesz; i++) {
         if (!namebuf[i]) numxattrs++;
      }

   }

   if (namesz < 0 && errno == EACCES) xattr_access_error = 1;

   v = VERSION;

   if (saveperms) v |= PERMS_FLAG;

   bsd_flags = sbuf->st_flags & CHFLAGS_BITS;
   if (bsd_flags) v |= LOCKS_FLAG;

   if (crtimeflag) {

      if (get_crtime(fname, &crtime)) {
         WARNING;
         goto done;
      }

      v |= CRTIME_FLAG;
      
   }

   if (savemtime) {
      v |= MTIME_FLAG;
   }

   if (save_owner(oprefs, sbuf)) {
      v |= OWNER_FLAG;
   }

   if (save_group(oprefs, sbuf)) {
      v |= GROUP_FLAG;
   }

   if (acl) {
      acltext = xbup_acl_to_text(acl, &acltextsz);

      if (!acltext) {
         WARNING;
         goto done;
      }
 
      v |= ACLTEXT_FLAG;
   }

   if (numxattrs > 0) {
      v |= XAT_FLAG;
   }

   if (cname[0] != 0) {
      cfp = fopen(cname, "w");
   }
   else {
      cfp = stdout;
   }

   if (!cfp) {
      WARNING;
      goto done; 
   }

   if (write_header(v, cfp)) {
      WARNING;
      goto done;
   }

   if (saveperms) {
      if (write_int2(sbuf->st_mode & CHMOD_BITS, cfp)) {
         WARNING;
         goto done;
      }
   }

   if (bsd_flags) {
      if (write_int2(bsd_flags, cfp)) {
         WARNING;
         goto done;
      }
   }

   if (crtimeflag) {

      /* NOTE: a 64-bit time_t could get truncated here */

      if (write_int4(crtime, cfp)) {
         WARNING;
         goto done;
      }
   }

   if (savemtime) {

      /* NOTE: a 64-bit time_t could get truncated here */

      if (write_int4(sbuf->st_mtime, cfp)) {
         WARNING;
         goto done;
      }
   }

   if (v & OWNER_FLAG) {
      char *unam = map_uid_to_name(sbuf->st_uid);
      if (unam) {
         long unamsz = strlen(unam);
         if (unamsz+1 > MAXNAME || unamsz == 0) {
            WARNING;
            goto done;
         }
         if (fwrite(unam, 1, unamsz+1, cfp) != unamsz+1) {
            WARNING;
            goto done;
         }
      }
      else {
         if (write_int1(0, cfp)) {
            WARNING;
            goto done;
         }
      }

      if (write_int4(sbuf->st_uid, cfp)) {
         WARNING;
         goto done;
      }
   }

   if (v & GROUP_FLAG) {
      char *grnam = map_gid_to_name(sbuf->st_gid);
      if (grnam) {
         long grnamsz = strlen(grnam);
         if (grnamsz+1 > MAXNAME || grnamsz == 0) {
            WARNING;
            goto done;
         }
         if (fwrite(grnam, 1, grnamsz+1, cfp) != grnamsz+1) {
            WARNING;
            goto done;
         }
      }
      else {
         if (write_int1(0, cfp)) {
            WARNING;
            goto done;
         }
      }

      if (write_int4(sbuf->st_gid, cfp)) {
         WARNING;
         goto done;
      }
   }


   if (acl) {
      if (fwrite(acltext, 1, acltextsz+1, cfp) != acltextsz+1) {
         WARNING;
         goto done;
      }
   }

   if (numxattrs > 0) {
      if (numxattrs >= (1L << 16)) {
         WARNING;
         goto done;
      }

      if (write_int2(numxattrs, cfp)) {
         WARNING;
         goto done;
      }

      attrname = namebuf;
      for (i = 0; i < numxattrs; i++) {
         attrnamesz = strlen(attrname);

         attrsz = getxattr(fname, attrname, 0, 0, 0, XATTR_NOFOLLOW);

         if (attrsz < 0) {
            WARNING;
            goto done;
         }

         if (attrsz > (1L << 20)) {
            WARN("WARNING: file: %s: very large attribute %s (%ld bytes)\n",
                    fname, attrname, attrsz);
         }

         if (attrsz > bufsize) {
            bufsize = attrsz;
            bp = realloc(attrbuf, bufsize);
            if (!bp) {
               WARNING;
               goto done;
            }
            attrbuf = bp;
         }

         if (getxattr(fname, attrname, attrbuf, bufsize, 0, XATTR_NOFOLLOW)
              != attrsz) {
            WARNING;
            goto done;
         }

         if (attrnamesz >= MAXNAME) {
            WARNING;
            goto done;
         }

         if (fwrite(attrname, 1, attrnamesz+1, cfp) != attrnamesz+1) {
            WARNING;
            goto done;
         }

         if (attrsz > (1L << 30)) {
            WARNING;
            goto done;
         }

         if (write_int4(attrsz, cfp)) { 
            WARNING;
            goto done;
         }

         if (fwrite(attrbuf, 1, attrsz, cfp) != attrsz) {
            WARNING;
            goto done;
         }

         attrname += attrnamesz + 1;

      }
   }

   retval = 0;


done:

   if (cfp && cfp != stdout) fclose(cfp);
   if (attrbuf) free(attrbuf);
   if (namebuf) free(namebuf);
   if (acltext) acl_free(acltext);

   return retval;

}


int has_xattr(const char *fname, const struct stat *sbuf)
{
   long retval =  listxattr(fname, 0, 0, XATTR_NOFOLLOW);
   if (retval < 0 && errno == EACCES) xattr_access_error = 1;
   return retval > 0;
}


/*
 * attempt to remove all xattrs from file fname.
 * Return 0 on success, -1 on failure.
 */

int strip_xattr(const char *fname, const struct stat *sbuf)
{
   char *namebuf=0;

   int retval = -1;

   char *attrname;
   long numxattrs, i, namesz, attrnamesz;
   int fail;

   namesz = listxattr(fname, 0, 0, XATTR_NOFOLLOW);

   if (namesz < 0 && errno == EACCES) {
      WARNING;
      goto done;
   }
   
   if (namesz <= 0) {
      retval = 0;
      goto done;
   }

   namebuf = (char *) malloc(namesz);
   if (!namebuf) {
      WARNING;
      goto done;
   }

   if (listxattr(fname, namebuf, namesz, XATTR_NOFOLLOW) != namesz) {
      WARNING;
      goto done;
   }

   numxattrs = 0;
   for (i = 0; i < namesz; i++) {
      if (!namebuf[i]) numxattrs++;
   }


   fail = 0;
   attrname = namebuf;
   for (i = 0; i < numxattrs; i++) {
      attrnamesz = strlen(attrname);

      if (removexattr(fname, attrname, XATTR_NOFOLLOW)) {
         WARNING;
         fail = 1;
      }

      attrname += attrnamesz + 1;
   }

   if (!fail) retval = 0;

done:
   if (namebuf) free(namebuf);

   return retval;
}





/* read xattr's from container cname and set them in file fname
 *    cname == NULL => all xattr's and locks stripped from fname
 *    cname == ""   => xattr's read from stdin
 *
 * sbuf: should be stat struct for fname
 *
 * retval: -1 on recoverable error (xattr container completely consumed)
 *         -2 on unrecoverable error
 *          0 otherwise
 *
 * options: follow symlinks? no
 *
 * NOTE: This tries to keep going in the face of errors as best as possible.
 * This is especially important in conjunction with the joinf_xattr program.
 */

int join_xattr(const char *fname, const struct stat *sbuf, const char *cname,
               int aclflag, const owner_prefs_t *oprefs)
{
   char *attrbuf=0;
   FILE *cfp=0;
   char *acltext=0;
   acl_t acl=0;

   int retval = 0;
   uint16_t bsd_flags = 0;

   char *bp;
   long numxattrs, i, attrsz, bufsize;
   uint16_t v, x;
   uint32_t xx;
   time_t crtime = 0;
   int got_crtime = 0;

   mode_t mode = sbuf->st_mode;
   uid_t  uid = sbuf->st_uid;
   gid_t  gid = sbuf->st_gid;
   time_t mtime = sbuf->st_mtime;

   /* process uid and gid default values */

   if (oprefs->u_keep && oprefs->u_default) uid = oprefs->uid;
   if (oprefs->g_keep && oprefs->g_default) gid = oprefs->gid;

   /* remove any locks */

   if (has_locks(sbuf)) {
      if (remove_locks(fname, sbuf)) {
         WARN("ERROR: failed to remove locks\n");
         retval = -1;
      }
   }

   /* remove acl */

   if (aclflag) {
      if (strip_acl(fname, sbuf)) {
         WARN("ERROR: failed to strip acl\n");
         retval = -1;
      }
   }

   /* setting xattrs requires write permission */

   if (make_writable(fname, sbuf)) {
      WARN("ERROR: failed to make writable\n");
      retval = -1; 
   }

   /* remove all xattrs first.
    * NOTE: writing the resource fork actually does not truncate it,
    * so removing the resource fork first is essential.
    */

   if (strip_xattr(fname, sbuf)) {
      WARN("ERROR: failed to strip xattr\n");
      retval = -1;
   }

   /* if no cname is given, the effect is to just strip locks, acl, xattrs,
      and to set owner/group to default values */

   if (!cname) {
      goto restore;
   }

   if (cname[0] != 0) {
      cfp = fopen(cname, "r");
   }
   else {
      cfp = stdin;
   }

   if (!cfp) {
      WARN("ERROR: failed to open container %s\n", cname);
      retval = -2; goto done;
   }

   if (read_header(&v, cfp) || (v & VERSION_MASK) != VERSION) {
      WARN("ERROR: corrupt header in container\n");
      retval = -2; goto done;
   }

   if (v & PERMS_FLAG) {
      if (read_int2(&x, cfp)) {
         Warning("read error");
         retval = -2; goto done;
      }

      mode = (mode & (~CHMOD_BITS)) | x;
   }

   if (v & LOCKS_FLAG) {
      if (read_int2(&bsd_flags, cfp)) {
         Warning("read error");
         retval = -2; goto done;
      }
   }

   if (v & CRTIME_FLAG) {

      if (read_int4(&xx, cfp)) {
         Warning("read error");
         retval = -2; goto done;
      }

      crtime = CAST_u32(time_t,xx);
      got_crtime = 1;

   }

   if (v & MTIME_FLAG) {

      if (read_int4(&xx, cfp)) {
         Warning("read error");
         retval = -2; goto done;
      }

      mtime = CAST_u32(time_t,xx);
   }

   if (v & OWNER_FLAG) {
      if (read_str(name_buffer, MAXNAME, cfp)) {
         Warning("read error");
         retval = -2; goto done;
      }
      if (read_int4(&xx, cfp)) {
         Warning("read error");
         retval = -2; goto done;
      }
      
      if (oprefs->u_keep) {
         int trans;
         uid = CAST_u32(uid_t, xx);
         trans = translate_uid(&uid);
         if (!xbup_opt_numeric_ids) {
            if (name_buffer[0] != 0) {
               if (map_name_to_uid(name_buffer, &uid)) {
                  WARN("WARNING: file %s: ownername %s translated to uid %ld\n", 
                       fname, name_buffer, CAST_to_long(uid_t, uid));
               }
            }
            else if (!trans) {
               WARN("WARNING: file %s: no ownername -- using uid %ld\n",
                    fname, CAST_to_long(uid_t, uid));
            }
         }
      }
   }

   if (v & GROUP_FLAG) {
      if (read_str(name_buffer, MAXNAME, cfp)) {
         Warning("reade error");
         retval = -2; goto done;
      }
      if (read_int4(&xx, cfp)) {
         Warning("read error");
         retval = -2; goto done;
      }
      
      if (oprefs->g_keep) {
         int trans;
         gid = CAST_u32(gid_t, xx);
         trans = translate_gid(&gid);
         if (!xbup_opt_numeric_ids) {
            if (name_buffer[0] != 0) {
               if (map_name_to_gid(name_buffer, &gid)) {
                  WARN("WARNING: file %s: groupname %s translated to gid %ld\n", 
                       fname, name_buffer, CAST_to_long(gid_t, gid));
               }
            }
            else if (!trans) {
               WARN("WARNING: file %s: no groupname -- using gid %ld\n",
                    fname, CAST_to_long(gid_t, gid));
            }
         }
      }
   }

   if (v & ACLTEXT_FLAG) {
      acltext = read_str1(cfp);
      if (!acltext) {
         Warning("read error");
         retval = -2; goto done;
      }
   }

   if (v & XAT_FLAG) {

      if (read_int2(&x, cfp)) {
         Warning("read error");
         retval = -2; goto done;
      }

      numxattrs = x;

      if (numxattrs == 0) {
         goto restore;
      }

      bufsize = BUFSIZE;
      attrbuf = (char *) malloc(BUFSIZE);
      if (!attrbuf) {
         Warning("malloc error");
         retval = -2; goto done; 
      }

      for (i = 0; i < numxattrs; i++) {

         if (read_str(name_buffer, MAXNAME, cfp)) {
            Warning("read error");
            retval = -2; goto done;
         }

         if (read_int4(&xx, cfp)) {
            Warning("read error");
            retval = -2; goto done;
         }

         if (xx > (1UL << 30)) {
            Warning("overflow");
            retval = -2; goto done;
         }

         attrsz = xx;

         if (attrsz > bufsize) {
            bufsize = attrsz;
            bp = realloc(attrbuf, bufsize);
            if (!bp) {
               Warning("malloc error");
               retval = -2; goto done;
            }
            attrbuf = bp;
         }

         if (fread(attrbuf, 1, attrsz, cfp) != attrsz) {
            Warning("read error");
            retval = -2; goto done;
         }

         if (setxattr(fname, name_buffer, attrbuf, attrsz, 0, XATTR_NOFOLLOW)) {
            WARN("ERROR: failed to set xattr %s\n", name_buffer);
            retval = -1; 
         }
      }
   }

restore:

   /* NOTE: writing the resource fork can change the mtime,
    * so we restore it here.  Also, if the xattr container has an
    * explicit mtime, it gets restored here.
    */

   if (set_mtime(fname, mtime)) {
      WARN("ERROR: failed to set mtime\n");
      retval = -1;
   }

   /* NOTE: setting mtime can change crtime (if new mtime < crtime).
    * However, setting crtime never seems to change mtime.
    * Conclusion: crtime should be set *after* mtime is set.
    */

   if (got_crtime) {
      if (set_crtime(fname, crtime)) {
         WARN("ERROR: failed to set crtime\n");
         retval = -1;
      }
   }

   /* set owner/group -- this must be done before permissions,
      to preserve setuid and setgid bits */

   if (uid != sbuf->st_uid || gid != sbuf->st_gid) {
      if (lchown(fname, uid, gid)) {
         WARN("ERROR: lchown(%ld, %ld) failed\n", 
                 CAST_to_long(uid_t, uid), CAST_to_long(gid_t, gid));
         retval = -1;
      }
   }

   /* restore permissions */

   if (hfs_chmod(fname, mode)) {
      WARN("ERROR: chmod failed\n");
      retval = -1;
   }

   /* set acl if any */

   if (aclflag && acltext) {
      acl = xbup_acl_from_text(acltext);
      if (!acl || put_acl(fname, sbuf, acl)) {
         WARN("ERROR: failed to set ACL: %s", acltext);
         retval = -1;
      }
      else if (xbup_acl_from_text_warning) {
         WARN("WARNING: file %s: ", fname);
         WARN("some translations to UUIDs failed in ACL:\n");
         WARN("%s", acltext);
      }
   }

   /* set locks if any -- this must be done last! */

   if (bsd_flags) {
      if (hfs_chflags(fname, (sbuf->st_flags & (~CHFLAGS_BITS)) | bsd_flags)) {
         WARN("ERROR: chflags failed\n");
         retval = -1;
      }
   }
                         

done:
   if (attrbuf) free(attrbuf);
   if (cfp && cfp != stdin) fclose(cfp);
   if (acltext) free(acltext);
   if (acl) acl_free(acl);

   return retval;
   
}



/* just reads and skips an xattr container -- used in conjunction
 * with the joinf_xattr program.
 *
 * Return values are as in join_xattr, but all errors are
 * "unrecoverable", so the return value is -2 on error, 0
 * otherwise.
 */


int skip_xattr(const char *cname)
{
   char *attrbuf=0;
   FILE *cfp=0;
   char *acltext=0;

   int retval = -2;

   char *bp;
   long numxattrs, i, attrsz, bufsize;
   uint16_t v, x;
   uint32_t xx;


   if (!cname) {
      goto restore;
   }

   if (cname[0] != 0) {
      cfp = fopen(cname, "r");
   }
   else {
      cfp = stdin;
   }

   if (!cfp) {
      WARNING;
      goto done;
   }

   if (read_header(&v, cfp) || (v & VERSION_MASK) != VERSION) {
      WARNING;
      goto done;
   }

   if (v & LOCKS_FLAG) {
      if (read_int2(&x, cfp)) {
         WARNING;
         goto done;
      }
   }

   if (v & PERMS_FLAG) {
      if (read_int2(&x, cfp)) {
         WARNING;
         goto done;
      }
   }

   if (v & CRTIME_FLAG) {

      if (read_int4(&xx, cfp)) {
         WARNING;
         goto done;
      }

   }

   if (v & MTIME_FLAG) {

      if (read_int4(&xx, cfp)) {
         WARNING;
         goto done;
      }

   }

   if (v & OWNER_FLAG) {
      if (read_str(name_buffer, MAXNAME, cfp)) {
         WARNING;
         goto done;
      }
      if (read_int4(&xx, cfp)) {
         WARNING;
         goto done;
      }
   }

   if (v & GROUP_FLAG) {
      if (read_str(name_buffer, MAXNAME, cfp)) {
         WARNING;
         goto done;
      }
      if (read_int4(&xx, cfp)) {
         WARNING;
         goto done;
      }
   }

   if (v & ACLTEXT_FLAG) {
      acltext = read_str1(cfp);
      if (!acltext) {
         WARNING;
         goto done;
      }
   }

   if (v & XAT_FLAG) {

      if (read_int2(&x, cfp)) {
         WARNING;
         goto done;
      }

      numxattrs = x;

      if (numxattrs == 0) {
         goto restore;
      }

      bufsize = BUFSIZE;
      attrbuf = (char *) malloc(BUFSIZE);
      if (!attrbuf) {
         WARNING;
         goto done; 
      }

      for (i = 0; i < numxattrs; i++) {

         if (read_str(name_buffer, MAXNAME, cfp)) {
            WARNING;
            goto done;
         }

         if (read_int4(&xx, cfp)) {
            WARNING;
            goto done;
         }

         if (xx > (1UL << 30)) {
            WARNING;
            goto done;
         }

         attrsz = xx;

         if (attrsz > bufsize) {
            bufsize = attrsz;
            bp = realloc(attrbuf, bufsize);
            if (!bp) {
               WARNING;
               goto done;
            }
            attrbuf = bp;
         }

         if (fread(attrbuf, 1, attrsz, cfp) != attrsz) {
            WARNING;
            goto done;
         }

      }
   }

restore:

   retval = 0;

done:
   if (attrbuf) free(attrbuf);
   if (cfp && cfp != stdin) fclose(cfp);
   if (acltext) free(acltext);

   return retval;
}




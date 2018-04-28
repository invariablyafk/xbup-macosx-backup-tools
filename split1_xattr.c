

/* usage: split1_xattr options fname 
 *          options: --crtime
 *                   --mtime
 *                   --lnkmtime
 *                   --acl
 *                   --fixperms
 *                   --lnkperms
 *                   --perms
 *                   --owner oname
 *                   --group gname
 *
 * 
 * writes xattr container for fname to stdout
 *
 * the --crtime flag causes the creation time of the file
 * to be stored in the xattr container
 *
 * the --mtime flag causes the mod time of the file
 * to be stored in the xattr container
 *
 * the --lnkmtime flag causes the mod time of the file
 * to be stored in the xattr container, if the file is a symlink
 *
 * the --acl flag causes the acl (if any) to be stored in the xattr container
 *
 * the --fixperms flag causes permissions for files with "problematic"
 * permissions to be stored in the xattr container
 *
 * the --lnkperms flag causes permissions for symbolic links
 *
 * the --perms flag causes permissions for all files to be stored
 *
 * the --owner flag causes the file owner to be saved;
 * as an optimization, if oname is not -, then the
 * owner name will not be saved if it is equal to oname;
 * oname can be either symbolic or numeric.
 *
 * the --group flag causes the file owner to be saved;
 * as an optimization, if gname is not -, then the
 * group name will not be saved if it is equal to gname;
 * gname can be either symbolic or numeric.
 * 
 * Returns -1 if errors detected, and 0 otherwise.
 *
 */

#include "util.h"
#include "xattr_util.h"

void usage()
{
   WARN("usage: split1_xattr options fname\n");
   WARN("  options: --crtime\n");
   WARN("           --mtime\n");
   WARN("           --lnkmtime\n");
   WARN("           --acl\n");
   WARN("           --fixperms\n");
   WARN("           --lnkperms\n");
   WARN("           --perms\n");
   WARN("           --owner oname\n");
   WARN("           --group gname\n");
}



int main(int argc, char **argv)
{
   int retval;
   char *fname;
   struct stat sbuf;
   int crtimeflag, mtimeflag, lnkmtimeflag,
       aclflag, fixpermsflag, allpermsflag, lnkpermsflag;
   char *owner_name, *group_name;
   int i;
   owner_prefs_t oprefs;
   int owner_status;

   crtimeflag = 0;
   mtimeflag = 0;
   lnkmtimeflag = 0;
   aclflag = 0;
   fixpermsflag = 0;
   allpermsflag = 0;
   lnkpermsflag = 0;
   owner_name = 0;
   group_name = 0;

   i = 1;
   while (i < argc) {
      if (strcmp(argv[i], "--crtime") == 0) {
         i++;
         crtimeflag = 1;
      }
      else if (strcmp(argv[i], "--mtime") == 0) {
         i++;
         mtimeflag = 1;
      }
      else if (strcmp(argv[i], "--lnkmtime") == 0) {
         i++;
         lnkmtimeflag = 1;
      }
      else if (strcmp(argv[i], "--acl") == 0) {
         i++;
         aclflag = 1;
      }
      else if (strcmp(argv[i], "--fixperms") == 0) {
         i++;
         fixpermsflag = 1;
      }
      else if (strcmp(argv[i], "--lnkperms") == 0) {
         i++;
         lnkpermsflag = 1;
      }
      else if (strcmp(argv[i], "--perms") == 0) {
         i++;
         allpermsflag = 1;
      }
      else if (strcmp(argv[i], "--owner") == 0) {
         if (i == argc-1) {
            usage();
            return -1;
         }
         i++;
         owner_name = argv[i];
         i++;
      }
      else if (strcmp(argv[i], "--group") == 0) {
         if (i == argc-1) {
            usage();
            return -1;
         }
         i++;
         group_name = argv[i];
         i++;
      }

      else
         break;
   }

   if (i != argc-1) {
      usage();
      return -1;
   }

   fname = argv[argc-1];

   if (lstat(fname, &sbuf)) {
      WARN("split1_xattr: bad file name %s\n", fname); 
      return -1;
   }

   owner_status = set_owner_prefs(&oprefs, owner_name, group_name);

   if (owner_status) {
      if (owner_status & 1) 
         WARN("split1_xattr: bad owner name %s\n", owner_name);
      if (owner_status & 2) 
         WARN("split1_xattr: bad group name %s\n", group_name);
      return -1;
   }

   xattr_access_error = 0;

   retval = split_xattr(fname, &sbuf, "", crtimeflag, 
                (mtimeflag || (lnkmtimeflag && S_ISLNK(sbuf.st_mode))),
                aclflag ? get_acl(fname, &sbuf) : 0, 
                (allpermsflag ||  (lnkpermsflag && S_ISLNK(sbuf.st_mode))
                 || (fixpermsflag && problem_perms(&sbuf))),
                &oprefs);

   if (retval == 0 && xattr_access_error) {
      WARN("split1_xattr: cannot access all metadata of %s\n", fname);
      retval = -1;
   }

   return retval;
}


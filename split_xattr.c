
/* usage: split_xattr options srcdir dstdir
 *    options:  --files-from file
 *              --recycle olddst
 *              --crtime
 *              --mtime
 *              --lnkmtime
 *              --acl
 *              --fixperms
 *              --lnkperms
 *              --perms
 *              --owner oname
 *              --group gname
 * 
 * creates dstdir, a repository of xattr containers from srcdir
 * dstdir should *not* exist prior to invocation.
 * 
 * with the --files-from file option, the contructed repository is 
 * pruned to only include those files and directories listed in file.
 *
 * with the --recycle olddst option, an attempt is made to
 * move existing xattrs containers from olddst.
 * When an xattr container is created, its mtime is set to the ctime
 * of the file.  If the mtime of the xattr container
 * in olddst is equal to the ctime of the file, then it is moved
 * from olddst to dstdir.
 *
 * the --crtime flag causes the creation times of all files to
 * be preserved -- using this flag causes an xattr container
 * file to be generated for *every* file and directory
 *
 * the --mtime flag causes the mod times of all files to
 * be preserved -- using this flag causes an xattr container
 * file to be generated for *every* file and directory
 * 
 * the --lnkmtime flag causes modtimes to be saved only for symlinks
 *
 * the --acl flag causes the acl of all files to
 * be preserved 
 *
 * the --perms flag causes permissions for all files to be stored
 * using this flag causes an xattr container to be generated for
 * *every* file and directory
 *
 * the --fixperms flag causes permissions for files with "problematic"
 * permissions to be stored in the xattr container
 *
 * the --lnkperms flag causes permissions for symbolic links
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



static int return_value = 0;
static int source_name_len = 0;
static char *destination_name = 0;
static char *linkdir_name = 0;

static int crtimeflag = 0;
static int mtimeflag = 0;
static int lnkmtimeflag = 0;
static int aclflag = 0;
static int fixpermsflag = 0;
static int allpermsflag = 0;
static int lnkpermsflag = 0;
static owner_prefs_t oprefs;

static char dblname[MAXLEN];
static char linkname[MAXLEN];


void process_xattrs(const char *itemname, const struct stat *itemstat, 
                    const char *dirname, const char *basename)
{
   acl_t acl=0;
   struct stat linkstat;
   int gotlink;
   int saveperms;
   int savemtime;

   xattr_access_error = 0;

   if (aclflag) acl = get_acl(itemname, itemstat);

   saveperms = allpermsflag || (lnkpermsflag && S_ISLNK(itemstat->st_mode)) 
               || (fixpermsflag && problem_perms(itemstat));

   savemtime = mtimeflag || (lnkmtimeflag && S_ISLNK(itemstat->st_mode));

   if ( need_container(itemname, itemstat, crtimeflag, savemtime,
                       acl, saveperms, &oprefs) ) {

      gotlink = 0;

      if (snprintf(dblname, MAXLEN, "%s%s/%s%s", 
         destination_name, 
         dirname + source_name_len, 
         basename,
         DBL_SUFFIX) >= MAXLEN) overflow();

      if (linkdir_name) {
         if (snprintf(linkname, MAXLEN, "%s%s/%s%s", 
            linkdir_name, 
            dirname + source_name_len, 
            basename,
            DBL_SUFFIX) >= MAXLEN) overflow();

         if (lstat(linkname, &linkstat) == 0 &&
             S_ISREG(linkstat.st_mode) &&
             linkstat.st_mtime == itemstat->st_ctime) {

            if (rename(linkname, dblname)) {
               WARN("split_xattr: could not move %s to %s\n", 
                    linkname, dblname);
               return_value = -1;
            }
            else {
               gotlink = 1;
            }
         }
      }

      if (!gotlink) {
        
         if ( split_xattr(itemname, itemstat, dblname, crtimeflag, savemtime,
                          acl, saveperms, &oprefs) ||
              set_mtime(dblname, itemstat->st_ctime) ) {

               WARN("split_xattr: error making %s\n", dblname);
               return_value = -1;

         }

      }
   }

   if (xattr_access_error) {
      WARN("split_xattr: some metadata unreadable: %s\n", itemname);
      return_value = -1;
   }

   if (acl) acl_free(acl);
}


void dirwalk(const char *dirname, const struct stat *dirstat, int walk_state)
{
   char itemname[MAXLEN];
   DIR *dirlist;
   struct dirent *diritem; 
   struct stat itemstat;
   int walk_state1;


   if (snprintf(dblname, MAXLEN, "%s%s", destination_name, 
      dirname + source_name_len) >= MAXLEN) overflow();

   if (mkdir(dblname, 0777)) {
      WARN("split_xattr: failed to create %s\n", dblname);
      return_value = -1;
      return;
   }

   dirlist = opendir(dirname);

   if (!dirlist) {
      WARN("split_xattr: opendir failed on %s\n", dirname);
      return_value = -1;
      return;
   }

   while ( (diritem = readdir(dirlist)) ) {

      if (strcmp(diritem->d_name, ".") == 0 
          || strcmp(diritem->d_name, "..") == 0)
         continue;

      if (snprintf(itemname, MAXLEN, "%s/%s", 
          dirname, diritem->d_name) >= MAXLEN) overflow();

      if (is_suffix(DBL_SUFFIX, DBL_SUFFIX_LEN, 
                    diritem->d_name, strlen(diritem->d_name))) {
         WARN("split_xattr: name conflict: %s\n", itemname);
         return_value = -1;
      }

      walk_state1 = walk_state;

      if (walk_state1 == 0) {
	    walk_state1 = lookup_name(itemname + source_name_len + 1);
	    if (walk_state1 == -1) continue; /* pruning */
      }

      if (lstat(itemname, &itemstat)) {
         WARN("split_xattr: lstat failed on %s\n", itemname);
         return_value = -1;
         continue;
      }


#if 0
      if (!(S_ISDIR(itemstat.st_mode) || S_ISREG_OR_LNK(itemstat.st_mode)))
         continue;
#endif

      if (walk_state1 == 1 && !S_ISDIR(itemstat.st_mode)) 
         process_xattrs(itemname, &itemstat, dirname, diritem->d_name);

      if (S_ISDIR(itemstat.st_mode)) {
	 dirwalk(itemname, &itemstat, walk_state1);
      }

   }

   closedir(dirlist);

   process_xattrs(dirname, dirstat, dirname, ".");
}

void usage()
{
   WARN("usage: split_xattr options srcdir dstdir\n");
   WARN("  options:  --files-from file\n");
   WARN("            --recycle olddst\n");
   WARN("            --crtime\n");
   WARN("            --mtime\n");
   WARN("            --lnkmtime\n");
   WARN("            --acl\n");
   WARN("            --fixperms\n");
   WARN("            --lnkperms\n");
   WARN("            --perms\n");
   WARN("            --owner oname\n");
   WARN("            --group gname\n");

}


int main(int argc, char **argv)
{
   char *fname, *lname, *srcname, *dstname;
   struct stat srcstat, dststat, linkstat;
   int srcname_len;
   int walk_state;
   char *owner_name, *group_name;
   int owner_status;

   

   int i;

   fname = 0;
   lname = 0;

   owner_name = 0;
   group_name = 0;

   
   i = 1;
   while (i < argc) {
      if (strcmp(argv[i], "--files-from") == 0) {
         if (i == argc-1) {
            usage();
            return -1;
         }
         i++;
         fname = argv[i];
         i++;
      }
      else if (strcmp(argv[i], "--recycle") == 0) {
         if (i == argc-1) {
            usage();
            return -1;
         }
         i++;
         lname = argv[i];
         i++;
      }
      else if (strcmp(argv[i], "--crtime") == 0) {
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

   if (i != argc-2) {
      usage();
      return -1;
   }

   srcname = argv[argc-2];
   dstname = argv[argc-1];

   if (fname) {
      collect_names(fname);
      walk_state = 0;
   }
   else {
      walk_state = 1;
   }

   srcname_len = strip_slashes(srcname);

   strip_slashes(dstname);

   if (lstat(srcname, &srcstat) || !S_ISDIR(srcstat.st_mode)) {
      usage();
      return -1;
   }

   if (lname) {

      strip_slashes(lname);

      if (lstat(lname, &linkstat) || !S_ISDIR(linkstat.st_mode)) {
         usage();
         return -1;
      }
   }

   if (!lstat(dstname, &dststat)) {
      WARN("split_xattr: %s already exists\n", dstname);
      return -1;
   }

   source_name_len = srcname_len;

   destination_name = dstname;

   linkdir_name = lname;

   owner_status = set_owner_prefs(&oprefs, owner_name, group_name);

   if (owner_status) {
      if (owner_status & 1) 
         WARN("split_xattr: bad owner name %s\n", owner_name);
      if (owner_status & 2) 
         WARN("split_xattr: bad group name %s\n", group_name);
      return -1;
   }

   dirwalk(srcname, &srcstat, walk_state);

   return return_value;

}

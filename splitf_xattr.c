
/* usage: splitf_xattr options srcdir 
 *    options:  --files-from file
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
 * Works like split_xattr, but writes all xattr information to
 * stdout, rather than creating a directory structure.
 * 
 * with the --files-from file option, the contructed repository is 
 * pruned to only include those files and directories listed in file.
 *
 * the --crtime flag causes the creation times of all files to
 * be preserved 
 *
 * the --mtime flag causes the mod times of all files to
 * be preserved 
 *
 * the --lnkmtime flag causes the mod times of all symlinks to
 * be preserved 
 *
 * the --acl flag causes the acl of all files to
 * be preserved 
 *
 * the --perms flag causes permissions for all files to be stored
 *
 * the --lnkperms flag causes permissions for symbolic links
 *
 * the --fixperms flag causes permissions for files with "problematic"
 * permissions to be stored in the xattr container
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


/* output file format:
 *   - header (8 "magic" bytes)
 *   - for each entry:
 *       - a null terminated relative path name (starting with "/", if non-empty)
 *       - an xattr container
 */


#include "util.h"
#include "xattr_util.h"



static int crtimeflag = 0;
static int mtimeflag = 0;
static int lnkmtimeflag = 0;
static int aclflag = 0;
static int fixpermsflag = 0;
static int allpermsflag = 0;
static int lnkpermsflag = 0;
static owner_prefs_t oprefs;

static int return_value = 0;
static int source_name_len = 0;


static char magic[8] = { 0xb7, 0x0e, 0xbf, 0xb2, 0xc2, 0x91, 0xf2, 0x92 };


void process_xattrs(const char *itemname, const struct stat *itemstat)
{
   acl_t acl=0;
   const char *ext;
   long extlen;
   int saveperms;
   int savemtime;


   xattr_access_error = 0;

   if (aclflag) acl = get_acl(itemname, itemstat);

   saveperms = allpermsflag || (lnkpermsflag && S_ISLNK(itemstat->st_mode)) 
               || (fixpermsflag && problem_perms(itemstat));

   savemtime = mtimeflag || (lnkmtimeflag && S_ISLNK(itemstat->st_mode));


   ext = itemname + source_name_len;
   extlen = strlen(ext);

   if (fwrite(ext, 1, extlen+1, stdout) != extlen+1 ||
       split_xattr(itemname, itemstat, "", crtimeflag, savemtime,  
                   acl, saveperms, &oprefs)) {

         WARN("splitf_xattr: error processing %s --- aborting\n", itemname);
         exit(-1);

   }

   if (xattr_access_error) {
      WARN("splitf_xattr: some metadata unreadable: %s\n", itemname);
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


   dirlist = opendir(dirname);

   if (!dirlist) {
      WARN("splitf_xattr: opendir failed on %s\n", dirname);
      return_value = -1;
      return;
   }

   while ( (diritem = readdir(dirlist)) ) {

      if (strcmp(diritem->d_name, ".") == 0 
          || strcmp(diritem->d_name, "..") == 0)
         continue;

      if (snprintf(itemname, MAXLEN, "%s/%s", 
          dirname, diritem->d_name) >= MAXLEN) overflow();
       

      walk_state1 = walk_state;

      if (walk_state1 == 0) {
	    walk_state1 = lookup_name(itemname + source_name_len + 1);
	    if (walk_state1 == -1) continue; /* pruning */
      }

      if (lstat(itemname, &itemstat)) {
         WARN("splitf_xattr: lstat failed on %s\n", itemname);
         return_value = -1;
         continue;
      }


#if 0
      if (!(S_ISDIR(itemstat.st_mode) || S_ISREG_OR_LNK(itemstat.st_mode)))
         continue;
#endif


      if (walk_state1 == 1 && !S_ISDIR(itemstat.st_mode)) 
         process_xattrs(itemname, &itemstat);

      if (S_ISDIR(itemstat.st_mode)) {
	 dirwalk(itemname, &itemstat, walk_state1);
      }

   }

   closedir(dirlist);

   process_xattrs(dirname, dirstat);
}

void usage()
{
   WARN("usage: splitf_xattr options srcdir\n");
   WARN("  options:  --files-from file\n");
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
   char *fname, *srcname;
   struct stat srcstat;
   int srcname_len;
   int walk_state;
   char *owner_name, *group_name;
   int owner_status;


   int i;

   fname = 0;
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

   if (i != argc-1) {
      usage();
      return -1;
   }

   srcname = argv[argc-1];

   if (fname) {
      collect_names(fname);
      walk_state = 0;
   }
   else {
      walk_state = 1;
   }

   srcname_len = strip_slashes(srcname);

   if (lstat(srcname, &srcstat) || !S_ISDIR(srcstat.st_mode)) {
      usage();
      return -1;
   }

   source_name_len = srcname_len;

   if (fwrite(magic, 1, 8, stdout) != 8) {
      WARN("write error --- aborting\n");
      return -1;
   }

   owner_status = set_owner_prefs(&oprefs, owner_name, group_name);

   if (owner_status) {
      if (owner_status & 1) 
         WARN("splitf_xattr: bad owner name %s\n", owner_name);
      if (owner_status & 2) 
         WARN("splitf_xattr: bad group name %s\n", group_name);
      return -1;
   }

   dirwalk(srcname, &srcstat, walk_state);

   return return_value;

}

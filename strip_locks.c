
/* usage: strip_locks options srcdir 
 *   options:  --files-from file
 *             --acl
 * 
 * strips locks from files in srcdir
 * 
 * with the --files-from flag, the only files in srcdir that
 * are examined are those listed in file (and subdirectories
 * along the way)
 *
 * with the --acl flag, acls are also stripped
 *
 * Returns -1 if errors detected, and 0 otherwise.
 *
 */

#include "util.h"
#include "xattr_util.h"



static int return_value = 0;
static int aclflag = 0;
   
static int source_name_len = 0;


void do_strip(const char *itemname, const struct stat *itemstat)
{
   if (has_locks(itemstat)) {
      // WARN("strip_locks: removing locks on %s\n", itemname);

      if (remove_locks(itemname, itemstat)) {
         WARN("strip_locks: failed to remove lock on %s\n", itemname);
         return_value = -1;
      }
   }

   if (aclflag && has_acl(itemname, itemstat)) {
      // WARN("strip_locks: removing acl on %s\n", itemname);

      if (strip_acl(itemname, itemstat)) {
         WARN("strip_locks: failed to remove acl on %s\n", itemname);
         return_value = -1;
      }
   }
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
      WARN("strip_locks: opendir failed on %s\n", dirname);
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
         WARN("strip_locks: lstat failed on %s\n", itemname);
         return_value = -1;
         continue;
      }


#if 0
      if (!(S_ISDIR(itemstat.st_mode) || S_ISREG_OR_LNK(itemstat.st_mode)))
         continue;
#endif

      do_strip(itemname, &itemstat);

      if (S_ISDIR(itemstat.st_mode))
	 dirwalk(itemname, &itemstat, walk_state1);
   }

   closedir(dirlist);
}

void usage()
{
   WARN("usage: strip_locks options srcdir\n");
   WARN("  options: --files-from file\n");
   WARN("           --acl\n");
}


int main(int argc, char **argv)
{
   char *fname, *srcname;
   struct stat srcstat;
   int srcname_len;
   int walk_state;
   int i;

   fname = 0;
   
   i = 1;
   while (i < argc) {
      if (strcmp(argv[i], "--acl") == 0) {
         i++;
         aclflag = 1;
      }
      else if (strcmp(argv[i], "--files-from") == 0) {
         if (i == argc-1) {
            usage();
            return -1;
         }
         i++;
         fname = argv[i];
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

   do_strip(srcname, &srcstat);

   source_name_len = srcname_len;

   dirwalk(srcname, &srcstat, walk_state);

   return return_value;

}

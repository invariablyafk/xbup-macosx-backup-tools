
/* usage: join_xattr options srcdir dstdir
 *    options:  --files-from file 
 *              --acl
 *              --owner oname
 *              --group gname
 *              --numeric-ids
 *              --preserve-uuids
 *              --ignore-uuids
 *              --usermap map
 *              --groupmap map
 * 
 * this "undoes" split_xattr, setting xattrs in srcdir
 * based on the xattr container appearing files in dstdir.
 * dstdir should definitely exist prior to invocation.
 * 
 * with the --files-from flag, the only files in srcdir that
 * are examined are those listed in file.
 *
 * with the --acl flag, acls are restored
 *
 * the --owner flag causes the file owner to be restored;
 * if oname is not -, and the container does not have any owner info,
 * then the owner will be restored to oname;
 * oname can be either symbolic or numeric.
 *
 * the --group flag causes the file owner to be saved;
 * if gname is not -, and the container does not have any group info,
 * then the group will be restored to gname;
 * gname can be either symbolic or numeric.
 *
 * the --numeric-ids flag makes forces the uid/gid info
 * to be used in restoring owner and group, and in translating
 * identities in ACLs (without this flag, the symbolic
 * user/group names are used, if possible).  Only relevant
 * if --owner, --group, or --acl options are present.
 *
 * the --preserve-uuids flag forces UUIDs to be used in
 * restoring identities in ACLs, even if the UUID is not
 * recognized.  The default behavior is to see if the UUID
 * is recognized, and if not, to use either the symbolic name
 * or (if --numeric-ids) the numeric uid/gid.
 *
 * the --ignore-uuids flag forces UUIDs to essentially be ignored.
 * Only the symbolic/numeric user/group info is used in restoring ACLs.
 *
 * The --usermap and --groupmap options allow translation
 * of users/groups
 *
 * Returns -1 if errors detected, and 0 otherwise.
 *
 */

#include "util.h"
#include "xattr_util.h"


static int aclflag=0;
static owner_prefs_t oprefs;

static int return_value = 0;
static int source_name_len = 0;
static char *destination_name = 0;

static char dblname[MAXLEN];

void process_xattrs(const char *itemname, const struct stat *itemstat, 
                    const char *dirname, const char *basename)
{
   int has_d;
   struct stat dblstat;


   if (snprintf(dblname, MAXLEN, "%s%s/%s%s", 
      destination_name, 
      dirname + source_name_len, 
      basename,
      DBL_SUFFIX) >= MAXLEN) overflow();

    has_d = ( !lstat(dblname, &dblstat) && S_ISREG(dblstat.st_mode) );

    if (has_d || need_reset(itemname, itemstat, aclflag, &oprefs)) {
       char *dn = (has_d ? dblname : 0);

       if (join_xattr(itemname, itemstat, dn, aclflag, &oprefs)) {

          WARN("join_xattr: error processing %s\n", itemname);
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
      WARN("join_xattr: opendir failed on %s\n", dirname);
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
         WARN("join_xattr: lstat failed on %s\n", itemname);
         return_value = -1;
         continue;
      }


#if 0
      if (!(S_ISDIR(itemstat.st_mode) || S_ISREG_OR_LNK(itemstat.st_mode)))
         continue;
#endif

      if (walk_state1 == 1 && !S_ISDIR(itemstat.st_mode)) {
         process_xattrs(itemname, &itemstat, dirname, diritem->d_name);
      }

      if (S_ISDIR(itemstat.st_mode)) {
	 dirwalk(itemname, &itemstat, walk_state1);
      }

   }

   closedir(dirlist);

   process_xattrs(dirname, dirstat, dirname, ".");

}

void usage()
{
   WARN("usage: join_xattr options srcdir dstdir\n");
   WARN("  option: --files-from file\n");
   WARN("          --acl\n");
   WARN("          --owner oname\n");
   WARN("          --group gname\n");
   WARN("          --numeric-ids\n");
   WARN("          --preserve-uuids\n");
   WARN("          --ignore-uuids\n");
   WARN("          --usermap map\n");
   WARN("          --groupmap map\n");
}


int main(int argc, char **argv)
{
   char *fname, *srcname, *dstname;
   struct stat srcstat, dststat;
   int srcname_len;
   int walk_state;

   char *owner_name, *group_name;
   int owner_status;
   char *usermap, *groupmap;

   int i;

   fname = 0;
   owner_name = 0;
   group_name = 0;
   usermap = groupmap = 0;

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
      else if (strcmp(argv[i], "--acl") == 0) {
         i++;
         aclflag = 1;
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
      else if (strcmp(argv[i], "--numeric-ids") == 0) {
         i++;
         xbup_opt_numeric_ids = 1;
      }
      else if (strcmp(argv[i], "--preserve-uuids") == 0) {
         i++;
         xbup_opt_preserve_uuids = 1;
      }
      else if (strcmp(argv[i], "--ignore-uuids") == 0) {
         i++;
         xbup_opt_preserve_uuids = -1;
      }
      else if (strcmp(argv[i], "--usermap") == 0) {
         if (i == argc-1) {
            usage();
            return -1;
         }
         i++;
         usermap = argv[i];
         i++;
      }
      else if (strcmp(argv[i], "--groupmap") == 0) {
         if (i == argc-1) {
            usage();
            return -1;
         }
         i++;
         groupmap = argv[i];
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

   process_usermap(usermap);
   process_groupmap(groupmap);

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

   if (lstat(dstname, &dststat) || !S_ISDIR(dststat.st_mode)) {
      usage();
      return -1;
   }

   source_name_len = srcname_len;

   destination_name = dstname;

   owner_status = set_owner_prefs(&oprefs, owner_name, group_name);

   if (owner_status) {
      if (owner_status & 1) 
         WARN("join_xattr: bad owner name %s\n", owner_name);
      if (owner_status & 2) 
         WARN("join_xattr: bad group name %s\n", group_name);
      return -1;
   }

   dirwalk(srcname, &srcstat, walk_state);

   return return_value;

}

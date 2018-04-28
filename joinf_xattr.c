
/* usage: joinf_xattr options srcdir 
 *    options:  --acl
 *              --owner oname
 *              --group gname
 *              --numeric-ids
 *              --preserve-uuids
 *              --ignore-uuids
 *              --usermap map
 *              --groupmap map
 * 
 * this "undoes" splitf_xattr, setting xattrs in srcdir
 * based on the xattr containers appearing in stdin.
 *
 * the --acl flag cause the acl of each file to be restored
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


static char magic[8] = { 0xb7, 0x0e, 0xbf, 0xb2, 0xc2, 0x91, 0xf2, 0x92 };


void usage()
{
   WARN("usage: joinf_xattr options srcdir dstdir\n");
   WARN("  option: --acl\n");
   WARN("          --owner oname\n");
   WARN("          --group gname\n");
   WARN("          --numeric-ids\n");
   WARN("          --preserve-uuids\n");
   WARN("          --ignore-uuids\n");
   WARN("          --usermap map\n");
   WARN("          --groupmap map\n");
}


static char extension[MAXLEN];
static char itemname[MAXLEN];


int main(int argc, char **argv)
{
   char *srcname;
   struct stat srcstat, itemstat;
   int srcname_len;
   char mbuf[8];

   int c, k;
   int ret, retval;

   int aclflag = 0;

   owner_prefs_t oprefs;
   char *owner_name = 0, *group_name = 0;
   int owner_status;
   char *usermap = 0, *groupmap = 0;

   int i;

   retval = 0;


   i = 1;
   while (i < argc) {
      if (strcmp(argv[i], "--acl") == 0) {
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

   if (i != argc-1) {
      usage();
      return -1;
   }

   srcname = argv[argc-1];

   process_usermap(usermap);
   process_groupmap(groupmap);

   srcname_len = strip_slashes(srcname);

   if (lstat(srcname, &srcstat) || !S_ISDIR(srcstat.st_mode)) {
      usage();
      return -1;
   }

   owner_status = set_owner_prefs(&oprefs, owner_name, group_name);

   if (owner_status) {
      if (owner_status & 1) 
         WARN("joinf_xattr: bad owner name %s\n", owner_name);
      if (owner_status & 2) 
         WARN("joinf_xattr: bad group name %s\n", group_name);
      return -1;
   }

   if (fread(mbuf, 1, 8, stdin) != 8 || memcmp(magic, mbuf, 8)) {
      WARN("bad file format\n");
      return -1;
   }

   for (;;) {

      c = getchar();
      if (c == EOF) return retval;

      k = 0;
      for (;;) {
         if (c == EOF) return -1;
         if (k >= MAXLEN) {
            WARN("buffer overflow\n");
            return -1;
         }
         extension[k] = c; 
         k++;
         if (c == 0) break;
         c = getchar();
      }

      if (snprintf(itemname, MAXLEN, "%s%s", srcname, extension) >= MAXLEN) 
         overflow();

      ret = 0;

      if (lstat(itemname, &itemstat)) {
         ret = skip_xattr("");
      }
      else {
         ret = join_xattr(itemname, &itemstat, "", aclflag, &oprefs);
      }

      if (ret) {
         if (ret == -1) {
            WARN("recoverble error processing %s -- continuing\n", itemname); 
            retval = -1;
         }
         else {
            WARN("unrecoverble error processing %s -- aborting\n", itemname); 
            return -1;
         }

      }
   }
}

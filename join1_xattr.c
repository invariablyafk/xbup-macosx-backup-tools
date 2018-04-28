

/* usage: join1_xattr options fname 
 *          options: --acl
 *                   --owner oname
 *                   --group gname
 *                   --numeric-ids
 *                   --preserve-uuids
 *                   --ignore-uuids
 *                   --usermap map
 *                   --groupmap map
 *
 * 
 * undoes split1_xattr fname, reading xattr container for fname from stdin
 *
 * the --acl flag causes the acl to be restored
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
 * Returns non-zero if errors detected, and 0 otherwise.
 *
 */

#include "util.h"
#include "xattr_util.h"

void usage()
{
   WARN("usage: join1_xattr options fname\n");
   WARN("  options: --acl\n");
   WARN("           --owner oname\n");
   WARN("           --group gname\n");
   WARN("           --numeric-ids\n");
   WARN("           --preserve-uuids\n");
   WARN("           --ignore-uuids\n");
   WARN("           --usermap map\n");
   WARN("           --groupmap map\n");
}

int main(int argc, char **argv)
{
   char *fname;
   struct stat sbuf;
   int aclflag;
   char *owner_name, *group_name;
   char *usermap, *groupmap;


   int i;

   owner_prefs_t oprefs;
   int owner_status;

   aclflag = 0;
   owner_name = 0;
   group_name = 0;
   usermap = groupmap = 0;



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

   fname = argv[argc-1];

   process_usermap(usermap);
   process_groupmap(groupmap);

   if (lstat(fname, &sbuf)) {
      WARN("join1_xattr: bad file name %s\n", fname); 
      return -1;
   }

   owner_status = set_owner_prefs(&oprefs, owner_name, group_name);

   if (owner_status) {
      if (owner_status & 1) 
         WARN("join1_xattr: bad owner name %s\n", owner_name);
      if (owner_status & 2) 
         WARN("join1_xattr: bad group name %s\n", group_name);
      return -1;
   }


   return join_xattr(fname, &sbuf, "", aclflag, &oprefs);
}



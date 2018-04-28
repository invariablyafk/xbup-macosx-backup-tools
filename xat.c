
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/xattr.h>


#define WARNING (fprintf(stderr, "%s %d: ", __FILE__, __LINE__), perror(""))

void usage()
{
   fprintf(stderr, "usage: xat <option> <file>\n");
   fprintf(stderr, "   where <option> is one of the following:\n\n");
   fprintf(stderr, "--list               list xattr names and their lengths\n");
   fprintf(stderr, "--get <name>         write value of xattr <name> to stdout\n");
   fprintf(stderr, "--print <name>       same as above, but human readble\n");
   fprintf(stderr, "--del <name>         delete xattr <name>\n");
   fprintf(stderr, "--set <name>         set value of xattr <name> to value read from stdin\n");
   fprintf(stderr, "--set <name>=<value> set value of xattr <name> to <value>\n");
   fprintf(stderr, "--has <name>         test if xattr <name> exists (useful in find scripts)\n");
   fprintf(stderr, "--has-any            test if any xattrs exist (useful in find scripts)\n");
}



int do_list(const char *file)
{
   long sz, i;
   char *namebuf;

   sz = listxattr(file, 0, 0, XATTR_NOFOLLOW);

   if (sz < 0) {
      WARNING;
      return -1;
   }

   if (sz == 0) {
      return 0;
   }

   namebuf = (char *) malloc(sz);
   if (!namebuf) {
      WARNING;
      return -1;
   }

   if (listxattr(file, namebuf, sz, XATTR_NOFOLLOW) != sz) {
      WARNING;
      return -1;
   }


   i = 0;
   while (i < sz) {
      printf("%s: %ld\n", namebuf + i, getxattr(file, namebuf+i, 0,0,0,XATTR_NOFOLLOW));
      i += strlen(namebuf + i);
      i++;
   }

   return 0;
}

int do_has_any(const char *file)
{
   long sz;

   sz = listxattr(file, 0, 0, XATTR_NOFOLLOW);

   if (sz <= 0)
      return -1;
   else
      return 0;
}



int do_get(const char *file, const char *name, int print)
{
   long sz, i;
   char *buf;

   sz = getxattr(file, name, 0, 0, 0, XATTR_NOFOLLOW);

   if (sz < 0) {
      WARNING;
      return -1;
   }

   if (sz == 0) {
      return 0;
   }

   buf = (char *) malloc(sz);
   if (!buf) {
      WARNING;
      return -1;
   }

   if (getxattr(file, name, buf, sz, 0, XATTR_NOFOLLOW) != sz) {
      WARNING;
      return -1;
   }

   if (print) {
      for (i = 0; i < sz; i++) {
         if (isprint(buf[i]))
            putchar(buf[i]);
         else
            putchar('.');
      }
      putchar('\n');
   }
   else {
      if (fwrite(buf, 1, sz, stdout) != sz) {
	 WARNING;
	 return -1;
      }
   }

   return 0;
}

int do_has(const char *file, const char *name)
{
   long sz;

   sz = getxattr(file, name, 0, 0, 0, XATTR_NOFOLLOW);

   if (sz < 0) 
      return -1;
   else
      return 0;
}


int do_del(const char *file, const char *name)
{
   if (getxattr(file, name, 0, 0, 0, XATTR_NOFOLLOW) >= 0) {
      if (removexattr(file, name, XATTR_NOFOLLOW)) {
         WARNING;
         return -1;
      }
   }

   return 0;
}

int do_set(const char *file, const char *name, const char *value)
{
   long sz;
   char *valbuf;
   long bufsz, pos, nread;

   if (value) {
      sz = strlen(value);
   }
   else {
      bufsz = 32*1024;
      
      valbuf = malloc(bufsz);
      if (!valbuf) {
         WARNING;
         return -1;
      }

      pos = 0;
      while ( (nread = fread(valbuf + pos, 1, bufsz - pos, stdin)) == bufsz - pos) {
         pos = bufsz;
         bufsz += bufsz/4;
         valbuf = realloc(valbuf, bufsz);
	 if (!valbuf) {
	    WARNING;
	    return -1;
	 }
      }
      sz = pos + nread;
      value = valbuf;
   }
      
   if (strcmp(name, "com.apple.ResourceFork") == 0) {

         /* We have to remove the resource fork first, if it exists;
          * otherwise, we may end up simply overwriting a portion of it.
          */

         if (getxattr(file, name, 0, 0, 0, XATTR_NOFOLLOW) >= 0) {
            if (removexattr(file, name, XATTR_NOFOLLOW) < 0) {
               WARNING;
               return -1;
            }
         }
   }

   if (setxattr(file, name, value, sz, 0, XATTR_NOFOLLOW)) {
      WARNING;
      return -1;
   }

   return 0;
}




int main(int argc, char **argv)
{
   char *verb, *file, *name, *value;
   int i;

   if (argc < 2) {
      usage();
      return -1;
   }

   verb = argv[1];

   if (strcmp(verb, "--list") == 0) {
      if (argc != 3) {
         usage();
         return -1;
      }
      file = argv[2];

      return do_list(file);
   }
   else if (strcmp(verb, "--has-any") == 0) {
      if (argc != 3) {
         usage();
         return -1;
      }
      file = argv[2];

      return do_has_any(file);
   }
   else if (strcmp(verb, "--get") == 0) {
      if (argc != 4) {
         usage();
         return -1;
      }
      name = argv[2];
      file = argv[3];

      return do_get(file, name, 0);
   }
   else if (strcmp(verb, "--has") == 0) {
      if (argc != 4) {
         usage();
         return -1;
      }
      name = argv[2];
      file = argv[3];

      return do_has(file, name);
   }
   else if (strcmp(verb, "--print") == 0) {
      if (argc != 4) {
         usage();
         return -1;
      }
      name = argv[2];
      file = argv[3];
 
      return do_get(file, name, 1);
   }
   else if (strcmp(verb, "--del") == 0) {
      if (argc != 4) {
         usage();
         return -1;
      }
      name = argv[2];
      file = argv[3];
 
      return do_del(file, name);
   }
   else if (strcmp(verb, "--set") == 0) {
      if (argc != 4) {
         usage();
         return -1;
      }
      name = argv[2];
      file = argv[3];

      i = 0;
      while (name[i] != '\0' && name[i] != '=') i++;
      if (name[i] == '=') {
         name[i] = '\0';
         value = name + i + 1;
      }
      else
         value = 0;

      return do_set(file, name, value);
   }
   else {
      usage();
      return -1;
   }
}

   

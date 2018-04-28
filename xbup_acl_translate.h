

#ifndef XBUP__xbup_acl_translate__H
#define XBUP__xbup_acl_translate__H

#include <sys/types.h>
#include <sys/acl.h>

extern int xbup_acl_from_text_warning; 
  /* set by xbup_acl_from_text if a translation to uuid fails */


acl_t xbup_acl_from_text(const char *buf_p);

char * xbup_acl_to_text(acl_t acl, ssize_t *len_p);


#endif

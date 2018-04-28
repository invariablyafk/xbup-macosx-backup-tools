/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

// This defined replacements xbup_acl_to_text/xbup_acl_from_text for
// acl_to_text/acl_from_text.  Some bugs and memory leaks are fixed.
// Also, they facilitate translation of uuids, in the case where
// the uuid is known on the source but unknown on the destination.

#include "xbup_acl_translate.h"
#include "util.h"

#include <sys/types.h>
#include <sys/acl.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <membership.h>
#include <pwd.h>
#include <grp.h>




#define ACL_TYPE_DIR	(1<<0)
#define ACL_TYPE_FILE	(1<<1)
#define ACL_TYPE_ACL	(1<<2)

static struct {
	acl_perm_t	perm;
	char		*name;
	int		type;
} acl_perms[] = {
	{ACL_READ_DATA,		"read",		ACL_TYPE_FILE},
//	{ACL_LIST_DIRECTORY,	"list",		ACL_TYPE_DIR},
	{ACL_WRITE_DATA,	"write",	ACL_TYPE_FILE},
//	{ACL_ADD_FILE,		"add_file",	ACL_TYPE_DIR},
	{ACL_EXECUTE,		"execute",	ACL_TYPE_FILE},
//	{ACL_SEARCH,		"search",	ACL_TYPE_DIR},
	{ACL_DELETE,		"delete",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_APPEND_DATA,	"append",	ACL_TYPE_FILE},
//	{ACL_ADD_SUBDIRECTORY,	"add_subdirectory", ACL_TYPE_DIR},
	{ACL_DELETE_CHILD,	"delete_child",	ACL_TYPE_DIR},
	{ACL_READ_ATTRIBUTES,	"readattr",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_WRITE_ATTRIBUTES,	"writeattr",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_READ_EXTATTRIBUTES, "readextattr",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_WRITE_EXTATTRIBUTES, "writeextattr", ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_READ_SECURITY,	"readsecurity",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_WRITE_SECURITY,	"writesecurity", ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_CHANGE_OWNER,	"chown",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{0, NULL, 0}
};

static struct {
	acl_flag_t	flag;
	char		*name;
	int		type;
} acl_flags[] = {
	{ACL_ENTRY_INHERITED,		"inherited",		ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_FLAG_DEFER_INHERIT,	"defer_inherit",	ACL_TYPE_ACL},
	{ACL_ENTRY_FILE_INHERIT,	"file_inherit",		ACL_TYPE_DIR},
	{ACL_ENTRY_DIRECTORY_INHERIT,	"directory_inherit",	ACL_TYPE_DIR},
	{ACL_ENTRY_LIMIT_INHERIT,	"limit_inherit",	ACL_TYPE_FILE | ACL_TYPE_DIR},
	{ACL_ENTRY_ONLY_INHERIT,	"only_inherit",		ACL_TYPE_DIR},
	{0, NULL, 0}
};


int xbup_acl_from_text_warning = 0;

acl_t
xbup_acl_from_text(const char *buf_p)
{
    int i, error = 0, need_tag, ug_tag, valid_uuid;
    char *buf, *orig_buf = NULL;
    char *entry, *field, *sub;
    uuid_t *uu = NULL;
    acl_entry_t acl_entry;
    acl_flagset_t flags = NULL;
    acl_permset_t perms = NULL;
    acl_tag_t tag;
    acl_t acl_ret = NULL;
    uid_t uid;
    gid_t gid;
    int id_type;

    xbup_acl_from_text_warning = 0;

    if (buf_p == NULL)
    {
	error = EINVAL;
	goto exit;
    }

    if ((orig_buf = strdup(buf_p)) == NULL) {
        error = ENOMEM;
	goto exit;
    }

    buf = orig_buf;

    if ((acl_ret = acl_init(1)) == NULL) {
        error = ENOMEM;
	goto exit;
    }

    // this fixes memory leak in original...
    if((uu = calloc(1, sizeof(uuid_t))) == NULL)
    {
        error = ENOMEM;
        goto exit;
    }

    /* global acl flags
     * format: !#acl <version> [<flags>]
     */
    if ((entry = strsep(&buf, "\n")) != NULL && *entry)
    {
	/* field 1: !#acl */
	field = strsep(&entry, " ");
	if (*field && strncmp(field, "!#acl", strlen("!#acl")))
	{
	    error = EINVAL;
	    goto exit;
	}

	/* field 2: <version>
	 * currently only accepts 1
	 */
	field = strsep(&entry, " ");
	if (!*field || string_to_long(field) != 1 || conversion_error)
	{
	    error = EINVAL;
	    goto exit;
	}

	/* field 3: <flags>
	 * optional
	 */
	if((field = strsep(&entry, " ")) != NULL && *field)
	{
	    acl_get_flagset_np(acl_ret, &flags);
	    while ((sub = strsep(&field, ",")) && *sub)
	    {
		for (i = 0; acl_flags[i].name != NULL; ++i)
		{
		    if (acl_flags[i].type & ACL_TYPE_ACL
			    && !strcmp(acl_flags[i].name, sub))
		    {
			acl_add_flag_np(flags, acl_flags[i].flag);
			break;
		    }
		}
		if (acl_flags[i].name == NULL)
		{
		    /* couldn't find flag */
		    error = EINVAL;
		    goto exit;
		}
	    }
	}
    } else {
	error = EINVAL;
	goto exit;
    }

    /* parse each acl line
     * format: <user|group>:
     *	    [<uuid>]:
     *	    [<user|group>]:
     *	    [<uid|gid>]:
     *	    <allow|deny>[,<flags>]
     *	    [:<permissions>[,<permissions>]]
     *
     * The current logic for translating id's is as follows:
     *
     *    if there is a valid uuid that belongs to a known user/group 
     *       and not --ignore-uuids then
     *       use it
     *    else if the username/groupname is known
     *            and not --preserve-uuids and not --numeric-ids
     *       use it
     *    else if the uid/gid is known 
     *            and not --preserve-uuids and --numeric-ids
     *       use it 
     *    else if the uuid is valid (but unknown) 
     *       and not --ignore-uuids
     *       use it 
     *    else
     *       ERROR
     *
     * The variable xbup_acl_from_text_warning gets set if
     * a translation from name or numeric id as attempted
     * but failed.
     * 
     *
     * Some properties of this logic:
     *   * backup/restore on same machine is always the identfity function
     *      even if uuid does not belong to a known user/group
     *   * backup from A restore on B:
     *        uuid unknown on A and unknown on B => uuid is preserved
     *        uuid unknown on A and known on B => uuid is preserved
     *        uuid known on A and unknown on B => uuid translated (if possible)
     *                                            using name/id
     *        uuid known on A and known on B => uuid is preserved
     */

    while ((entry = strsep(&buf, "\n")) && *entry)
    {
	need_tag = 1;
        valid_uuid = 0;
	ug_tag = -1;

        uuid_clear(*uu);

	/* field 1: <user|group> */
	field = strsep(&entry, ":");

	if(acl_create_entry(&acl_ret, &acl_entry))
	{
	    error = ENOMEM;
	    goto exit;
	}

	if (-1 == acl_get_flagset_np(acl_entry, &flags)
	 || -1 == acl_get_permset(acl_entry, &perms))
	{
	    error = EINVAL;
	    goto exit;
	}

	switch(*field)
	{
	    case 'u':
		if(!strcmp(field, "user"))
		    ug_tag = ID_TYPE_UID;
		break;
	    case 'g':
		if(!strcmp(field, "group"))
		    ug_tag = ID_TYPE_GID;
		break;
	    default:
		error = EINVAL;
		goto exit;
	}

	/* field 2: <uuid> */

        // if we find a "valid looking" uuid, even if it
        // does not belong it a known user or group,
        // we set valid_uuid to 1;  if it does belong
        // to a known user/group, and not --ignore-uuids,
        // we also set need_tag to 0

	if ((field = strsep(&entry, ":")) != NULL && *field && 
            xbup_opt_preserve_uuids >= 0)
	{
            if (uuid_parse(field, *uu) == 0) {
               if (map_uuid_to_id(*uu, &uid, &id_type) == 0) {
                  switch (id_type) {
                     case ID_TYPE_UID:
                        if (map_uid_to_name(uid)) 
                        {
                            need_tag = 0;
                        }

                        valid_uuid = 1;
                        break;
                     case ID_TYPE_GID:
                        if (map_gid_to_name((gid_t) uid)) 
                        {
                           need_tag = 0;
                        }
                        valid_uuid = 1;
                        break;
                     default: ;
                  }
               }
            }
	}

	/* field 3: <username|groupname> */
	if ((field = strsep(&entry, ":")) != NULL && *field && need_tag &&
            xbup_opt_preserve_uuids <= 0 && !xbup_opt_numeric_ids)
	{
	    switch(ug_tag)
	    {
		case ID_TYPE_UID:
                    if (map_name_to_uid(field, &uid) == 0) {
			if (map_uid_to_uuid(uid, *uu) != 0)
			{
			    error = EINVAL;
			    goto exit;
			}
                        need_tag = 0;  
                        valid_uuid = 1;
                     }
                    else
                        xbup_acl_from_text_warning = 1;
		    break;
		case ID_TYPE_GID:
                    if (map_name_to_gid(field, &gid) == 0) {
			if (map_gid_to_uuid(gid, *uu) != 0)
			{
			    error = EINVAL;
			    goto exit;
			}
                        need_tag = 0; 
                        valid_uuid = 1;
                    }
                    else
                        xbup_acl_from_text_warning = 1;
		    break;
		default:
		    error = EINVAL;
		    goto exit;
	    }
	}

	/* field 4: <uid|gid> */
	if ((field = strsep(&entry, ":")) != NULL && *field && need_tag &&
            xbup_opt_preserve_uuids <= 0 && xbup_opt_numeric_ids)
	{
            uid = (uid_t) string_to_long(field);

	    if (conversion_error)
	    {
		error = EINVAL;
		goto exit;
	    }

	    switch(ug_tag)
	    {
		case ID_TYPE_UID:
                    translate_uid(&uid);
		    if (map_uid_to_name(uid)) {
			if (map_uid_to_uuid(uid, *uu) != 0)
			{
			    error = EINVAL;
			    goto exit;
			}
                        need_tag = 0; 
                        valid_uuid = 1;
                    }
                    else
                        xbup_acl_from_text_warning = 1;
		    break;
		case ID_TYPE_GID:
                    gid = (gid_t) uid;
                    translate_gid(&gid);
		    if (map_gid_to_name(gid)) {
			if (map_gid_to_uuid(gid, *uu) != 0)
			{
			    error = EINVAL;
			    goto exit;
			}
                        need_tag = 0; 
                        valid_uuid = 1;
                    }
                    else
                        xbup_acl_from_text_warning = 1;
		    break;
	    }
	}

	/* sanity check: nothing set as qualifier */
	if (!valid_uuid)
	{
            xbup_acl_from_text_warning = 1;

            // at this point, all of the translations failed
            // UUID is null

	    // error = EINVAL;
	    // goto exit;
	}

	/* field 5: <flags> */
	if((field = strsep(&entry, ":")) == NULL || !*field)
	{
	    error = EINVAL;
	    goto exit;
	}

	for (tag = 0; (sub = strsep(&field, ",")) && *sub;)
	{
	    if (!tag)
	    {
		if (!strcmp(sub, "allow"))
		    tag = ACL_EXTENDED_ALLOW;
		else if (!strcmp(sub, "deny"))
		    tag = ACL_EXTENDED_DENY;
		else {
		    error = EINVAL;
		    goto exit;
		}
		continue;
	    }

	    for (i = 0; acl_flags[i].name != NULL; ++i)
	    {
		if (acl_flags[i].type & (ACL_TYPE_FILE | ACL_TYPE_DIR)
			&& !strcmp(acl_flags[i].name, sub))
		{
		    acl_add_flag_np(flags, acl_flags[i].flag);
		    break;
		}
	    }
	    if (acl_flags[i].name == NULL)
	    {
		/* couldn't find perm */
		error = EINVAL;
		goto exit;
	    }
	}

	/* field 6: <perms> (can be empty) */
	if((field = strsep(&entry, ":")) != NULL && *field)
	{
	    while ((sub = strsep(&field, ",")) && *sub)
	    {
		for (i = 0; acl_perms[i].name != NULL; i++)
		{
		    if (acl_perms[i].type & (ACL_TYPE_FILE | ACL_TYPE_DIR)
			    && !strcmp(acl_perms[i].name, sub))
		    {
			acl_add_perm(perms, acl_perms[i].perm);
			break;
		    }
		}
		if (acl_perms[i].name == NULL)
		{
		    /* couldn't find perm */
		    error = EINVAL;
		    goto exit;
		}
	    }
	}
	acl_set_tag_type(acl_entry, tag);
	acl_set_qualifier(acl_entry, *uu);
    }

exit:
    if (orig_buf) free(orig_buf);
    if (uu) free(uu);
    if (error)
    {
	if (acl_ret) acl_free(acl_ret);
	acl_ret = NULL;
	errno = error;
    }
    return acl_ret;
}


/*
 * reallocing snprintf with offset
 */

static int
raosnprintf(char **buf, size_t *size, ssize_t *offset, char *fmt, ...)
{
    va_list ap;
    int ret;

    do
    {
	if (*offset < *size)
	{
	    va_start(ap, fmt);
	    ret = vsnprintf(*buf + *offset, *size - *offset, fmt, ap);
	    va_end(ap);
	    if (ret < (*size - *offset))
	    {
		*offset += ret;
		return ret;
	    }
	}
	*buf = reallocf(*buf, (*size *= 2));
    } while (*buf);

    //warn("reallocf failure");
    return 0;
}


char *
xbup_acl_to_text(acl_t acl, ssize_t *len_p)
{
	acl_tag_t tag;
	acl_entry_t entry = NULL;
	acl_flagset_t flags;
	acl_permset_t perms;
	int i, first;
	size_t bufsize = 1024;
	char *buf = NULL;

	if (acl_valid(acl)) {
		errno = EINVAL;
		return NULL;
	}

	if (len_p == NULL)
	    if ((len_p = alloca(sizeof(ssize_t))) == NULL)
		goto err_nomem;

	*len_p = 0;

	if ((buf = malloc(bufsize)) == NULL)
	    goto err_nomem;

	if (!raosnprintf(&buf, &bufsize, len_p, "!#acl %d", 1))
	    goto err_nomem;

	if (acl_get_flagset_np(acl, &flags) == 0)
	{
	    for (i = 0, first = 0; acl_flags[i].name != NULL; ++i)
	    {
		if (acl_flags[i].type & ACL_TYPE_ACL
			&& acl_get_flag_np(flags, acl_flags[i].flag) != 0)
		{
		    if(!raosnprintf(&buf, &bufsize, len_p, "%s%s",
			    first++ ? "," : " ", acl_flags[i].name))
			goto err_nomem;
		}
	    }
	}
	for (;acl_get_entry(acl,
		    entry == NULL ? ACL_FIRST_ENTRY : ACL_NEXT_ENTRY, &entry) == 0;)
	{
	    int valid;
	    uuid_t *uu;
	    char uu_str[37];
            uid_t id;
            int idt;
            char *name;

	    if (((uu = (uuid_t *) acl_get_qualifier(entry)) == NULL)
		|| (acl_get_tag_type(entry, &tag) != 0)
		|| (acl_get_flagset_np(entry, &flags) != 0)
		|| (acl_get_permset(entry, &perms) != 0)
                || (map_uuid_to_id(*uu, &id, &idt) != 0))  
            {
                 if (uu != NULL) acl_free(uu);
                 if (buf != NULL) free(buf);
                 errno = EINVAL;
                 return NULL;
            }
            
	    uuid_unparse_upper(*uu, uu_str);
	    acl_free(uu);

            switch (idt) {
                case ID_TYPE_UID:
                    if ((name = map_uid_to_name(id)))
                        valid = raosnprintf(&buf, &bufsize, len_p, "\n%s:%s:%s:%ld",
                                   "user", uu_str, name,
                                   CAST_to_long(uid_t, id));
                    else
                        valid = raosnprintf(&buf, &bufsize, len_p, "\n%s:%s:%s:%s",
                                   "user", uu_str, "", "");
                    break;

                case ID_TYPE_GID:
                    if ((name = map_gid_to_name((gid_t)id)))
                        valid = raosnprintf(&buf, &bufsize, len_p, "\n%s:%s:%s:%ld",
                                   "group", uu_str, name,
                                   CAST_to_long(uid_t, id));
                    else
                        valid = raosnprintf(&buf, &bufsize, len_p, "\n%s:%s:%s:%s",
                                   "group", uu_str, "", "");
                    break;

                default:
                    if (buf != NULL) free(buf);
                    errno = EINVAL;
                    return NULL;
            }

            if (!valid) goto err_nomem;

            if (!raosnprintf(&buf, &bufsize, len_p, ":%s",
		(tag == ACL_EXTENDED_ALLOW) ? "allow" : "deny")) goto err_nomem;


	    for (i = 0; acl_flags[i].name != NULL; ++i)
	    {
		if (acl_flags[i].type & (ACL_TYPE_DIR | ACL_TYPE_FILE))
		{
		    if(acl_get_flag_np(flags, acl_flags[i].flag) != 0)
		    {
			if(!raosnprintf(&buf, &bufsize, len_p, ",%s",
			    acl_flags[i].name))
			    goto err_nomem;
		    }
		}
	    }

	    for (i = 0, first = 0; acl_perms[i].name != NULL; ++i)
	    {
		if (acl_perms[i].type & (ACL_TYPE_DIR | ACL_TYPE_FILE))
		{
		    if(acl_get_perm_np(perms, acl_perms[i].perm) != 0)
		    {
			if(!raosnprintf(&buf, &bufsize, len_p, "%s%s",
			    first++ ? "," : ":", acl_perms[i].name))
			    goto err_nomem;
		    }
		}
	    }
	}

        if (!raosnprintf(&buf, &bufsize, len_p, "\n")) goto err_nomem;

	return buf;

err_nomem:
	if (buf != NULL) free(buf);
	errno = ENOMEM;
	return NULL;
}



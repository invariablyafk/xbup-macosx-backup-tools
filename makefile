NAME = xbup-2.1

PROGS = split_xattr join_xattr strip_locks split1_xattr join1_xattr \
        splitf_xattr joinf_xattr xat 

SCRIPTS = xbup gen_pat

HELPERS = xbup_helper 

OBJ = util.o xattr_util.o xbup_acl_translate.o

DOC = doc.tex doc.pdf

CFILES = split_xattr.c util.c xattr_util.c join_xattr.c strip_locks.c \
         split1_xattr.c join1_xattr.c splitf_xattr.c joinf_xattr.c xat.c \
         xbup_acl_translate.c

HFILES = util.h xattr_util.h xbup_acl_translate.h uthash.h

SAMPLES = sample-.xbupconfig



all: ${OBJ} ${PROGS}

%.o: %.c
	gcc -O -Wall -c $<

%: %.c ${OBJ}
	gcc -O -Wall -o $@ $< ${OBJ}

clean:
	rm ${OBJ} 

install:
	cp ${PROGS} ${SCRIPTS} ${HELPERS} ~/bin

tarball:
	rm -rf ${NAME}
	mkdir ${NAME}
	cp README.txt makefile ${DOC} ${CFILES} ${HFILES} ${SCRIPTS} ${HELPERS} ${SAMPLES}  ${NAME}
	tar -czvf ${NAME}.tgz ${NAME}
	
.SUFFIXES:

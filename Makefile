CC = gcc
CFLAGS = -g -Wall -Wextra -Wshadow -Wunreachable-code -Wredundant-decls -Wmissing-declarations -Wold-style-definition \
	 -Wmissing-prototypes -Wdeclaration-after-statement -Wno-return-local-addr -Wunsafe-loop-optimizations -Wuninitialized -Werror
PROGS = viktar

all: $(PROGS)

$(PROGS): $(PROGS).o
	$(CC) $(CFLAGS) -o $@ $^ -lz -lbsd

$(PROGS).o: $(PROGS).c $(PROGS).h
	$(CC) $(CFLAGS) -c $<

clean cls:
	rm -f $(PROGS) *.o *~ \#~

git:
	git add $(PROGS).c $(PROGS).h [mM]akefile
	git status

make_tar:
	tar cvfa Lab2_${LOGNAME}.tar.gz $(PROGS).c $(PROGS).h [mM]akefile













tinyshell

a small shell i'm writing in C. mostly a skeleton right now — the main loop,
tokenizer, builtin stubs, and external command runner are there. pipes,
redirections, and job control are not implemented yet.

build:

	make

run:

	make run

debug (asan + ubsan + gdb):

	make debug

layout:

	src/tinyshell.c   - main loop
	src/tinyshell.h   - constants
	src/parser.c      - line tokenizer
	src/builtins.c    - cd, exit (stubs)
	src/executor.c    - fork/exec/waitpid

todo (in no particular order):

- actually implement cd (chdir + HOME fallback)
- actually implement exit (with status code)
- PATH lookup for commands without a slash
- redirections > < >>
- pipes |
- background jobs &
- quoting / escaping
- SIGINT handling so Ctrl-C doesn't kill the shell

license: MIT

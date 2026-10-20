# M2: exit

also done already. look at `builtin_exit()` in `src/builtins.c` and the end
of `main()` in `src/tinyshell.c`.

```
$ build/tinyshell
> false
> exit
$ echo $?
1
$ build/tinyshell
> exit 258
$ echo $?
2
```

## turning text into a number

the obvious tool is `atoi()`, and it's the wrong one. `atoi("abc")` is 0,
`atoi("0")` is 0, and `atoi("12abc")` is 12. you can't tell a real number
from garbage.

`strtol()` gives you enough information to check:

```c
char *end = NULL;
errno = 0;
long code = strtol(argv[1], &end, 10);

if (errno != 0 || end == argv[1] || *end != '\0') {
  ts_errorx("exit: '%s': not a number", argv[1]);
  ...
}
```

`errno` is non-zero if the number was out of range
(`exit 99999999999999999999`). `end == argv[1]` means no digits were read
at all (`exit abc`). `*end != '\0'` means there was junk after the digits
(`exit 12abc`).

setting `errno = 0` first matters. `strtol()` sets `errno` on overflow but
never clears it, so an old value left over from some unrelated call would
look like an error.

## 8 bits

```c
*status = (int)(code & 0xFF);
```

the status a parent gets back from `waitpid()` only has room for 8 bits of
the exit code. so `exit 256` comes out as 0 and `exit -1` as 255. bash
truncates the same way.

## not calling exit() directly

`builtin_exit()` doesn't call `exit()`. it returns `TS_BUILTIN_EXIT`, the
loop in `shell_step()` stops, and `main()` returns the status. the end
result is identical. the difference shows up later: when you want to save
history or clean up background jobs before quitting, the end of `main()`
is the one place to do it.

## exit with no number

plain `exit` quits with the status of the previous command. before calling
any builtin, `shell_step()` puts `last_status` into `*status`, and
`builtin_exit()` only overwrites it if you gave it a number. reaching end
of input does the same thing, which is handy from scripts:

```sh
$ printf 'false\n' | build/tinyshell; echo $?
1
```

## questions to try

1. `strtol()` needs `errno = 0` beforehand. `chdir()` doesn't. why?
2. what does `exit -1` give you, and why?
3. `exit 1 2` prints an error and keeps the shell running. would exiting
   with 1 be better or worse?

# M1: cd

this one is already done. the code is `builtin_cd()` in `src/builtins.c`,
about twenty lines, and this chapter walks through it so you know what the
rest of the milestones expect from you.

```
> cd /tmp
> pwd
/tmp
> cd
> pwd
/home/you
> cd /nope
tinyshell: cd: /nope: No such file or directory
```

## why it can't be a program

suppose `cd` lived in `/usr/bin` like `ls` does. the shell would fork, the
child would exec `/usr/bin/cd`, and that program would call
`chdir("/tmp")`. the child's directory changes. then the child exits, and
the shell prints its next prompt from wherever it was before.

each process has its own current directory, and nothing a child does can
change its parent's. so anything that needs to change the shell itself has
to run inside the shell. that's what a builtin is.

(if you haven't read [processes and fork](../concepts/processes.md) yet,
the bit about the child getting a *copy* of everything is what this
depends on.)

## the code

```c
const char *target = argv[1];
if (target == NULL) {
  target = getenv("HOME");
  if (target == NULL || target[0] == '\0') {
    ts_errorx("cd: HOME not set");
    *status = TS_STATUS_FAILURE;
    return TS_BUILTIN_OK;
  }
}

if (chdir(target) == -1) {
  ts_error("cd: %s", target);
  *status = TS_STATUS_FAILURE;
  return TS_BUILTIN_OK;
}
```

`getenv()` returns `NULL` when the variable isn't set. that sounds
unlikely for `HOME`, but cron jobs and anything started with `env -i` run
with an empty environment, and dereferencing `NULL` would take the shell
down with it.

when `chdir()` fails it leaves the reason in `errno`. `ts_error()` tacks
`strerror(errno)` onto the message, which is how you get "No such file or
directory" or "Not a directory" instead of a bare "cd failed".

a failed `cd` sets the status to 1 and returns. the loop keeps going.

## the table

builtins are looked up by name in a small array:

```c
static const builtin_t builtins[] = {
  {"cd", builtin_cd},
  {"exit", builtin_exit},
  ...
};
```

every builtin has the same signature, so adding one means writing the
function and adding a line here. for five entries a linear `strcmp` scan
is fine.

## questions to try

1. walk through what would happen, process by process, if `cd` were an
   external program.
2. why does this code need both `ts_error()` and `ts_errorx()`?
3. if `HOME` is set to an empty string, what should `cd` do? why not just
   pass it to `chdir()`?

## if you want more

`cd -` goes back to the previous directory; bash remembers it in
`$OLDPWD`. bash also updates `$PWD` after every `cd` (look at `getcwd()`
and `setenv()`). and a `pwd` builtin is a nice ten-minute exercise.

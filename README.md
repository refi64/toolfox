# toolfox

toolfox is an easy way for you to be able to expose binaries from within your
[toolbox](https://github.com/debarshiray/toolbox/) into the host world.

This is partly aiming at being a lighter version of the toolboxd FUSE filesystem work, which felt
a bit awkward and fragile to me. I want to also add a D-Bus interface at some point later on,
however.

toolfox also installs a "command not found" handler in your shell (zsh and bash are supported)
that will let you know if you should be running that command from inside your toolbox.

## Installation

Enter your toolbox via `toolbox enter`, make sure `glib2-devel` is installed, then run
`./toolfox-installer.sh`.

## Usage

```bash
# Expose the toolbox's dnf command into your PATH.
$ toolfox link dnf
# Now we can run it...
$ dnf --help
# ...and then maybe remove it.
$ toolfox unlink dnf
# Commands can also be run without using link.
$ toolfox run dnf --help
```

## How it works

toolbox doesn't natively support running commands inside the toolbox. Therefore, toolfox tricks
toolbox into using a custom "shell", toolbox-redirect-internal-helper. In addition, the command
to run is serialized via a GVariant inside `$VTE_VERSION`, which is one of the environment
variables that toolbox forwards. Once the toolbox starts the helper, it will deserialize the
command to run and run it.

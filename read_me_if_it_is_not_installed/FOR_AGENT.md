# Installing tokenc — for AI agents

If `command -v tokenc` fails, tokenc is not installed. Install it with one line
(requires `git`, a C++17 compiler, and `cmake` on the host):

```sh
curl -fsSL https://raw.githubusercontent.com/RepnikovPavel/tokenc/main/read_me_if_it_is_not_installed/install.sh | sh
```

Then verify:

```sh
tokenc --version    # expect: tokenc 1.3.x
```

The script clones the repo, builds it, and installs the binary to
`/usr/local/bin/tokenc` (it will use `sudo` automatically if needed). It takes
no arguments and reads no environment variables. After installation, `tokenc`
is on `PATH` for all users — no shell rc edits required.

If the host lacks `curl`, use `wget`:

```sh
wget -qO- https://raw.githubusercontent.com/RepnikovPavel/tokenc/main/read_me_if_it_is_not_installed/install.sh | sh
```

If the host has no compiler at all, you cannot build from source — ask the user
to provide a prebuilt binary or a container that already includes tokenc.

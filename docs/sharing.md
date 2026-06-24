# `secret` HTTP sharing — groups, `serve`, and `pull-http`

> Pulling secret stores over HTTP from groups of peers. Companion to
> `docs/secret.md` (CLI contract) and `lib/http.c` / `lib/group.c`.

## Model

Each store is a git repository of GPG-encrypted parameters. Alongside the
existing SSH sync (`pull` / `push` / `sync`), stores can be **pulled over
HTTP**:

- `secret serve` exposes every store for read-only "dumb HTTP" git pull
  at `/app/secret/<store>`.
- A **group** is a named set of HTTP/HTTPS endpoints (peers) you pull
  from. `secret pull-http <store> [group]` clones/updates the store from
  the group's endpoints.

**Pull only.** There is intentionally no HTTP push — publishing changes
to a peer is done over SSH (`push` / `sync`). This keeps the HTTP surface
a read-only, unauthenticated file server.

## Why unauthenticated HTTP is fine

The server only ever exposes **GPG ciphertext and public keys** — exactly
what the SSH transport already moves. Confidentiality comes from the
encryption, not the channel: anyone who can reach the port can fetch the
ciphertext, but only a store **recipient** can decrypt it (see
`add-gpg-key`). Use HTTPS (terminate TLS at a reverse proxy) if you also
want to hide *which* stores/among whom are shared, but it is not required
for secrecy of the values.

## The port (derived from uid)

The server port is, in order of precedence:

1. `$SECRET_HTTP_PORT`, if set;
2. otherwise `49152 + (uid % 16384)` — a deterministic value in the
   dynamic port range.

So a peer that is the **same user** (same uid) on another machine knows
the port automatically; `secret port` prints it. Cross-uid peers should
record the explicit port in the endpoint.

## Serving

```
secret serve                       # binds 0.0.0.0:<port>, runs until Ctrl-C
SECRET_HTTP_BIND=127.0.0.1 secret serve   # localhost only (e.g. behind a proxy)
```

Internally the server maps `/app/secret/<store>/<path>` to
`$SELF_CONFIG/<store>/.git/<path>` and runs `git update-server-info`
when `info/refs` is requested, so freshly committed changes are served
without a restart.

## Groups

Endpoints live one-per-line under `$SELF_CONFIG/.groups/<group>`:

```
secret group-add team http://alice.example          # port defaults to your uid port
secret group-add team http://bob.example:50321      # explicit port (different uid)
secret group-add team https://vault.example         # behind a TLS proxy
secret groups                                        # team<TAB>http://alice.example
secret group-del team http://bob.example:50321
```

An endpoint may be written as `host`, `host:port`, or a full
`http(s)://host[:port]` URL. A missing scheme defaults to `http`; a
missing port defaults to the local `port`.

## Pulling

```
secret pull-http bitcoin team       # pull 'bitcoin' from group 'team'
secret pull-http bitcoin            # ...from every group's endpoints
```

For each endpoint, `pull-http` builds `<endpoint>/app/secret/<store>` and:

- **clones** it if the store is not present locally;
- otherwise **pulls** and auto-resolves merge conflicts by taking the
  remote side (same policy as `pull` over SSH).

After pulling, decrypt as usual (`secret get <store>/<param>`) — you must
be a recipient of the store.

## A note on git identity

Stores need a git identity for commits to land (and thus to be syncable).
`secret init` now configures a per-repo `user.name`/`user.email` from the
`secret identity`, so commits — and therefore both SSH and HTTP sync —
work without any global git configuration.

## See also

- `docs/secret.md` — the `serve` / `port` / `groups` / `pull-http` CLI
  contract.
- `lib/http.c` (server + port), `lib/group.c` (groups + pull-http).

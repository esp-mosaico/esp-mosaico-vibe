# Simulator preview connection troubleshooting

[简体中文](simulator-preview_CN.md) | [Documentation index](README.md)

Start the project simulator and record its listening address and port. Use
`127.0.0.1` when the browser and service run on the same machine. With SSH,
remote `localhost` differs from the user's computer. Prefer existing forwarding;
if needed, run this on the user's computer, replace uppercase placeholders and
keep the terminal running:

```sh
ssh -N -o ExitOnForwardFailure=yes -L 127.0.0.1:LOCAL_PORT:127.0.0.1:REMOTE_PORT USER@HOST
```

Open `http://127.0.0.1:LOCAL_PORT/`. Preserve existing SSH aliases, jump hosts and
port settings. A reachable remote private IP also works if the service listens
on that interface. `0.0.0.0` is a listening address; browse to the actual host IP.
Previewing does not require disabling firewalls or publishing a public service.

For connection failures, check the remote service, then the local forwarded port
and API on the user's computer:

```powershell
# Windows PowerShell; replace LOCAL_PORT with the actual number.
Test-NetConnection 127.0.0.1 -Port LOCAL_PORT
curl.exe --noproxy "*" --max-time 5 http://127.0.0.1:LOCAL_PORT/api/v1/state
```

```sh
# macOS / Linux
nc -vz 127.0.0.1 LOCAL_PORT
curl --noproxy '*' --max-time 5 http://127.0.0.1:LOCAL_PORT/api/v1/state
```

A refused connection calls for checking the SSH process and local listener. If
the port connects but HTTP fails, inspect SSH logs and the remote service port.
If an embedded browser rewrites the URL, compare its final address with the
actual mapping. After the API returns the expected game identity, verify frames,
clicks and keyboard input in the intended browser.

If the page displays but clicks do nothing, inspect JS errors, input requests and
state changes. Input code should support changing pointer IDs, map coordinates
to the displayed image and preserve press/move/release ordering. Cover these
behaviors in code and tests rather than repeating every check during troubleshooting.

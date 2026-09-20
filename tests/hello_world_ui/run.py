#!/usr/bin/env python3
"""Exercise the real Hello World C backend through the pinned GSP simulator."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import socket
import subprocess
import time
from urllib.parse import urlparse


def counter_pixels(path: Path) -> bytes:
    # Simulator PPM is P6 with no comments, tightly packed RGB888.
    magic, width, height, maximum, pixels = path.read_bytes().split(maxsplit=4)
    assert (magic, width, height, maximum) == (b'P6', b'480', b'480', b'255')
    assert len(pixels) == 480 * 480 * 3
    return b''.join(pixels[(y * 480 + 346) * 3:(y * 480 + 454) * 3]
                    for y in range(357, 416))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--sim', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    manifest = json.loads(args.manifest.read_text())
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    ready = output / 'ready.json'
    ready.unlink(missing_ok=True)
    backend = None
    with (output / 'sim.log').open('w') as sim_log, (output / 'backend.log').open('w') as backend_log:
        sim = subprocess.Popen([str(args.sim.resolve()), '--bundle', manifest['bundle'],
                                '--frames', '0', '--headless', '--backend-enable',
                                '--backend-required', '--api-listen', 'tcp://127.0.0.1:0',
                                '--ready-file', str(ready)], stdout=sim_log, stderr=sim_log)
        try:
            deadline = time.monotonic() + 20
            while True:
                assert sim.poll() is None, 'simulator exited before readiness'
                try:
                    info = json.loads(ready.read_text())
                    break
                except (FileNotFoundError, json.JSONDecodeError):
                    assert time.monotonic() < deadline, 'simulator readiness timeout'
                    time.sleep(0.05)
            assert info['pid'] == sim.pid and info['bridge_version'] == 1
            backend = subprocess.Popen([manifest['executable'], '--endpoint', info['backend'],
                                        '--duration-ms', '30000'],
                                       stdout=backend_log, stderr=backend_log)
            endpoint = urlparse(info['api'])
            with socket.create_connection((endpoint.hostname, endpoint.port), timeout=10) as sock:
                stream = sock.makefile('rb')
                sequence = 0

                def rpc(method, params=None):
                    nonlocal sequence
                    sequence += 1
                    data = json.dumps(dict(jsonrpc='2.0', id=sequence, method=method,
                                           params=params or {})).encode()
                    sock.sendall(f'Content-Length: {len(data)}\r\n\r\n'.encode() + data)
                    while True:
                        header = {}
                        while (line := stream.readline()) not in (b'\r\n', b'\n'):
                            assert line, 'simulator closed API connection'
                            key, value = line.decode().split(':', 1)
                            header[key.lower()] = value.strip()
                        response = json.loads(stream.read(int(header['content-length'])))
                        if response.get('id') == sequence:
                            assert 'error' not in response, response
                            return response.get('result')

                def capture(name):
                    # Allow input dispatch, backend callback, bind write, and rendering.
                    time.sleep(0.12)
                    rpc('wait', {'frames': 8})
                    path = output / (name + '.ppm')
                    rpc('screenshot', {'path': str(path), 'format': 'ppm'})
                    return counter_pixels(path)

                zero = capture('00-initial')
                rpc('tap', {'x': 100, 'y': 398})  # Button label.
                one = capture('01-label-tap')
                assert one != zero, 'label tap did not change the rendered counter'
                rpc('tap', {'x': 290, 'y': 398})  # Arrow child also activates button.
                two = capture('02-arrow-tap')
                assert two not in (zero, one), 'arrow tap did not increment counter'
                rpc('tap', {'x': 200, 'y': 300})  # Noninteractive text.
                assert capture('02-outside-tap') == two, 'outside tap changed count'
                # A held contact is not repeated as multiple greetings.
                rpc('feed_pointer', {'x': 170, 'y': 398, 'pressed': True})
                capture('02-pressed')
                initial_pixels = (output / '02-outside-tap.ppm').read_bytes().split(maxsplit=4)[4]
                pressed_pixels = (output / '02-pressed.ppm').read_bytes().split(maxsplit=4)[4]
                # The engine's pressed shade must not paint outside the button.
                paper = initial_pixels[:3]
                for y in range(367, 426):
                    for x in range(28, 323):
                        i = (y * 480 + x) * 3
                        if (x < 60 or x > 293) and initial_pixels[i:i+3] == paper:
                            assert pressed_pixels[i:i+3] == paper, 'pressed shade spills outside rounded button'
                assert initial_pixels != pressed_pixels, 'button lacks pressed feedback'
                rpc('wait', {'frames': 40})
                rpc('feed_pointer', {'x': 170, 'y': 398, 'pressed': False})
                three = capture('03-held-tap')
                assert three not in (zero, one, two)
                for _ in range(96):
                    rpc('tap', {'x': 170, 'y': 398})
                    rpc('wait', {'frames': 2})
                ninety_nine = capture('99-before-wrap')
                assert ninety_nine not in (zero, one, two, three)
                rpc('tap', {'x': 170, 'y': 398})
                egg = capture('100-celebration')
                assert egg not in (zero, ninety_nine), '100th greeting did not show celebration'
                first_frame = (output / '100-celebration.ppm').read_bytes()
                time.sleep(0.35)
                rpc('tap', {'x': 170, 'y': 398})
                capture('100-confetti-next')
                assert first_frame != (output / '100-confetti-next.ppm').read_bytes(), 'confetti did not animate'
                time.sleep(3.1)
                assert capture('00-wrapped') == zero, 'celebration did not return to 00 or accepted an extra tap'
                rpc('tap', {'x': 170, 'y': 398})
                assert capture('01-after-wrap') == one, 'counter did not resume after wrap'
                assert backend.wait(timeout=35) == 0
                backend_log.flush()
                assert 'count=1, last_error=0' in (output / 'backend.log').read_text()
                # A required-backend host stops processing API requests once
                # its backend detaches; the finally block owns host shutdown.
                stream.close()
            (output / 'result.json').write_text(json.dumps(dict(
                status='passed', greetings=101, checks=['label', 'arrow', 'outside',
                'held-contact', 'pressed-corners', '100-celebration', 'animated-confetti', 'ignore-celebration-tap', 'timed-reset', 'after-wrap', 'clean-shutdown']), indent=2) + '\n')
            print(f'Hello World native interaction checks: PASS ({output})')
        finally:
            for process in (backend, sim):
                if process is not None and process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()


if __name__ == '__main__':
    main()

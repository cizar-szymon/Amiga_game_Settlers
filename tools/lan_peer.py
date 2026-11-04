"""A LAN game peer on the PC, for testing the Amiga side (port/amiga_net.c).

  python tools/lan_peer.py listen [seconds]
      print the announcements of servers (UDP 6810)
  python tools/lan_peer.py announce [--name PC] [--version 0.67] [--to IP]
      announce a server like RUN SERVER does, once a second
  python tools/lan_peer.py connect <address> [--name SLAVE] [--version 0.67]
                                   [--seconds 20] [--pings 0] [--echo-sums]
      join a server like the game does: HELLO, answer its PINGs, and with
      --pings N send N pings of our own and print the times there and back;
      --echo-sums sends the server's checksums back (a test of the LAN line
      in the game without a second Amiga)
      --turns: after the SETUP an empty turn (no commands) every 80 ms, a
      few ahead, so the server's game goes on as with a real player;
      --stall AT:FOR stops the turns at AT s after the SETUP for FOR s (the
      server shows WAITING), --slow-pongs AT:FOR:MS answers PINGs MS late
      (SLOW CONNECTION); several of each, comma separated

Frames on TCP 6810: length (1 byte: type + data) | type | data.
--port N (before the mode) uses another port than 6810, TCP and UDP.
"""
import argparse
import socket
import struct
import time

F_HELLO, F_BYE, F_PING, F_PONG = 1, 2, 3, 4
PORT = 6810


def listen(seconds):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("", PORT))
    s.settimeout(0.5)
    end = time.time() + seconds
    n = 0
    while time.time() < end:
        try:
            data, sender = s.recvfrom(512)
        except socket.timeout:
            continue
        n += 1
        print(f"{sender[0]}: {data.decode('latin-1')}", flush=True)
    print(f"{n} announcements in {seconds:g} s")


def announce(name, version, seconds, address):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    text = f"SETTLERS-LAN 1 {version} {name}".encode()
    for _ in range(int(seconds)):
        s.sendto(text, (address, PORT))
        time.sleep(1)
    print(f"{int(seconds)} announcements to {address}")


def frame(ftype, data=b""):
    return bytes([1 + len(data), ftype]) + data


def connect(args):
    t0 = time.time()
    s = socket.create_connection((args.address, PORT), timeout=5)
    s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
    print(f"connected in {(time.time() - t0) * 1000:.1f} ms", flush=True)
    hello = bytes([1]) + args.version.encode() + b"\0" + args.name.encode() + b"\0"
    s.sendall(frame(F_HELLO, hello))
    s.settimeout(0.02)
    buf = b""
    setup_at = None
    next_turn = 0
    late_pongs = []

    def windows(spec, parts):
        out = []
        for one in (spec or "").split(","):
            if one:
                out.append([float(x) for x in one.split(":")][:parts])
        return out

    stalls = windows(args.stall, 2)
    slows = windows(args.slow_pongs, 3)
    end = time.time() + args.seconds
    pings_left = args.pings
    sent_at = {}
    times = []
    seq = 0
    while time.time() < end:
        now = time.time()
        if setup_at is not None and args.turns:
            t = now - setup_at
            stalled = any(a <= t < a + d for a, d in stalls)
            if not stalled:
                target = int(t / 0.08) + 6
                while next_turn <= target:
                    s.sendall(frame(16 + 2, struct.pack(">I", next_turn) + b"\0"))
                    next_turn += 1
        for item in [x for x in late_pongs if x[0] <= now]:
            s.sendall(frame(F_PONG, item[1]))
            late_pongs.remove(item)
        if pings_left > 0 and not sent_at:
            seq += 1
            sent_at[seq] = time.perf_counter()
            s.sendall(frame(F_PING, struct.pack(">I", seq)))
            pings_left -= 1
        try:
            chunk = s.recv(4096)
            if not chunk:
                print("closed by the Amiga")
                break
            buf += chunk
        except socket.timeout:
            continue
        while len(buf) >= 2 and len(buf) >= 1 + buf[0]:
            length, ftype = buf[0], buf[1]
            data = buf[2:1 + length]
            buf = buf[1 + length:]
            if ftype == F_HELLO:
                version, name = data[1:].split(b"\0")[:2]
                print(f"HELLO from {name.decode()} version {version.decode()}",
                      flush=True)
            elif ftype == F_PING:
                late = 0
                if setup_at is not None:
                    t = time.time() - setup_at
                    for a, d, ms in slows:
                        if a <= t < a + d:
                            late = ms / 1000
                if late:
                    late_pongs.append((time.time() + late, data))
                else:
                    s.sendall(frame(F_PONG, data))
            elif ftype == F_PONG:
                n = struct.unpack(">I", data)[0]
                if n in sent_at:
                    times.append((time.perf_counter() - sent_at.pop(n)) * 1000)
            elif ftype == F_BYE:
                print(f"BYE: {data.decode('latin-1')}")
                end = 0
            elif ftype == 16 + 2:
                pass  # the server's turns
            else:
                if ftype == 16 and setup_at is None:
                    setup_at = time.time()
                    print(f"{time.strftime('%H:%M:%S')} SETUP", flush=True)
                print(f"game frame {ftype - 16}: {data.decode('latin-1')}",
                      flush=True)
                # SUM (game frame 1) back: the server sees its own
                # checksums as the other side's - "same" in its LAN line.
                if ftype == 17 and args.echo_sums:
                    s.sendall(frame(ftype, data))
    s.close()
    if times:
        times.sort()
        print(f"{len(times)} pings: mean {sum(times) / len(times):.2f} ms, "
              f"median {times[len(times) // 2]:.2f} ms, worst {times[-1]:.2f} ms")


def main():
    global PORT
    p = argparse.ArgumentParser()
    p.add_argument("--port", type=int, default=PORT)
    sub = p.add_subparsers(dest="mode", required=True)
    pl = sub.add_parser("listen")
    pl.add_argument("seconds", nargs="?", type=float, default=15)
    pa = sub.add_parser("announce")
    pa.add_argument("--name", default="PC")
    pa.add_argument("--version", default="0.68")
    pa.add_argument("--seconds", type=float, default=5)
    pa.add_argument("--to", default="255.255.255.255")
    pc = sub.add_parser("connect")
    pc.add_argument("address")
    pc.add_argument("--name", default="SLAVE")
    pc.add_argument("--version", default="0.68")
    pc.add_argument("--seconds", type=float, default=20)
    pc.add_argument("--pings", type=int, default=0)
    pc.add_argument("--echo-sums", action="store_true")
    pc.add_argument("--turns", action="store_true")
    pc.add_argument("--stall", default="")
    pc.add_argument("--slow-pongs", default="")
    args = p.parse_args()
    PORT = args.port
    if args.mode == "listen":
        listen(args.seconds)
    elif args.mode == "announce":
        announce(args.name, args.version, args.seconds, args.to)
    else:
        connect(args)


if __name__ == "__main__":
    main()

"""Versioned G1 NDJSON acquisition for the bundled independent local plant.

No Torch, model API, credentials or actuator interface. A seed/preset is frozen at
process start; restart creates a new epoch. Only lifecycle commands are accepted.
"""
from __future__ import annotations
import argparse
import heapq
import json
import math
import os
from pathlib import Path
import queue
import re
import sys
import threading
import time
import uuid

from local_plant import Plant, PRESETS

MAX_FRAME = 256*1024


def strict_object(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError("Duplicate JSON key")
        value[key] = item
    return value


def strict_json(text):
    try:
        document=json.loads(text, object_pairs_hook=strict_object,
                            parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Nonfinite JSON")))
        pending=[(document,0)]
        while pending:
            value,depth=pending.pop()
            if depth>64:
                raise ValueError("JSON nesting bound exceeded")
            if type(value) in (float,int) and not math.isfinite(value):
                raise ValueError("Unrepresentable JSON number")
            if isinstance(value,str):
                value.encode("utf-8",errors="strict")
            elif isinstance(value,dict):
                pending.extend((item,depth+1) for pair in value.items() for item in pair)
            elif isinstance(value,list):
                pending.extend((item,depth+1) for item in value)
        return document
    except (RecursionError,OverflowError) as error:
        raise ValueError("JSON resource bound exceeded") from error


def read_document(path):
    with Path(path).open("rb") as stream:
        raw = stream.read(4*1024*1024+1)
    if len(raw) > 4*1024*1024:
        raise ValueError("Local configuration exceeds its bound")
    return strict_json(raw)


class Publisher:
    def __init__(self, configuration, context, configuration_hash, physical_hash, *, seed, preset, onset_s, recovery_s,
                 output=sys.stdout, monotonic=time.monotonic):
        if not all(re.fullmatch(r"[0-9a-f]{64}", value or "") for value in (configuration_hash,physical_hash)):
            raise ValueError("Missing supplied registered identities")
        if configuration["physical_hash"] != physical_hash or context["physical_hash"] != physical_hash:
            raise ValueError("Registered physical identity mismatch")
        self.configuration, self.context = configuration, context
        self.configuration_hash, self.physical_hash = configuration_hash, physical_hash
        self.options = dict(seed=seed,preset=preset,onset_s=onset_s,recovery_s=recovery_s)
        self.output, self.monotonic = output, monotonic
        self.stream_id = "local-"+uuid.uuid4().hex
        self.rate, self.paused, self.stopped = 1.0, False, False
        self.sequence = 0
        self.pending = []
        self.acks = {}
        self.new_epoch()

    def frame(self, kind, payload):
        frame = dict(telemetry_version=1,kind="stream_frame",type=kind,stream_id=self.stream_id,
                     epoch_id=self.epoch_id,sequence=self.sequence,configuration_hash=self.configuration_hash,
                     physical_hash=self.physical_hash,event_time_s=self.plant.time_s,payload=payload)
        self.sequence += 1
        if self.sequence > 9007199254740991:
            raise ValueError("Sequence resource limit")
        raw = json.dumps(frame,separators=(",",":"),allow_nan=False)+"\n"
        if len(raw.encode("utf-8")) > MAX_FRAME:
            raise ValueError("Frame resource limit")
        return raw

    def emit(self, raw):
        self.output.write(raw)
        self.output.flush()

    def clock(self):
        return dict(paused=self.paused,clock_rate=self.rate)

    def new_epoch(self):
        self.plant = Plant(self.context["network"],self.configuration,**self.options)
        self.epoch_id = "epoch-"+uuid.uuid4().hex
        self.sequence = 0
        self.pending.clear()
        self.acks.clear()
        self.last_monotonic = self.monotonic()
        self.heartbeat_at = self.last_monotonic
        self.emit(self.frame("start",dict(mode="synthetic_live",**self.clock())))
        self.sample()

    def sample(self):
        rows = self.plant.sample(due_only=True)
        if not rows:
            return
        raw = self.frame("readings",dict(readings=rows))
        delay = self.plant.delivery_delay_s()
        if delay:
            if len(self.pending) >= 16:
                raise ValueError("Synthetic delay queue resource limit")
            # Wire sequence is assigned on acquisition, not rewritten on receipt.
            heapq.heappush(self.pending,(self.monotonic()+delay, self.sequence, raw))
        else:
            self.emit(raw)

    def tick(self):
        now = self.monotonic()
        if now < self.last_monotonic:
            raise ValueError("Monotonic clock moved backwards")
        elapsed = now-self.last_monotonic
        self.last_monotonic = now
        if not self.paused:
            self.plant.advance(self.plant.time_s+elapsed*self.rate)
            self.sample()
        while self.pending and self.pending[0][0] <= now:
            self.emit(heapq.heappop(self.pending)[2])
        if now-self.heartbeat_at >= min(1.0,self.configuration["policy"]["heartbeat_period_s"]):
            self.heartbeat_at = now
            self.emit(self.frame("heartbeat",self.clock()))

    def command(self, command):
        expected = {"telemetry_version","kind","request_id","stream_id","epoch_id","operation","payload"}
        if type(command) is not dict or set(command) != expected or type(command["telemetry_version"]) is not int or command["telemetry_version"] != 1 or command["kind"] != "stream_command":
            raise ValueError("Unsupported command shape/version")
        identity = command["request_id"]
        if type(identity) is not str or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.:-]{0,127}",identity):
            raise ValueError("Invalid command identity")
        if command["stream_id"] != self.stream_id or command["epoch_id"] != self.epoch_id:
            raise ValueError("Stale command epoch")
        if identity in self.acks:
            original, ack = self.acks[identity]
            if command != original:
                raise ValueError("Conflicting duplicate command")
            self.emit(ack)
            return
        operation, payload = command["operation"], command["payload"]
        if operation not in {"pause","resume","set_rate","restart","stop"} or type(payload) is not dict:
            raise ValueError("Unsupported lifecycle operation")
        if set(payload) != ({"clock_rate"} if operation == "set_rate" else set()):
            raise ValueError("Invalid lifecycle payload")
        rate = payload.get("clock_rate", self.rate)
        if type(rate) not in (int,float) or not math.isfinite(rate) or not 0.1 <= rate <= 4:
            raise ValueError("Invalid lifecycle clock rate")
        self.tick()
        if operation == "pause":
            self.paused = True
        elif operation == "resume":
            self.paused = False
        elif operation == "set_rate":
            self.rate = rate
        ack = self.frame("ack",dict(request_id=identity,operation=operation,effective_event_time_s=self.plant.time_s,**self.clock()))
        self.acks[identity] = (command,ack)
        while len(self.acks) > 256:
            self.acks.pop(next(iter(self.acks)))
        self.emit(ack)
        if operation == "restart":
            self.new_epoch()
        elif operation == "stop":
            # Pending delayed acquisitions are omitted rather than emitted after
            # the end frame. The receiver retains the actual received prefix.
            self.pending.clear()
            self.emit(self.frame("end",dict(reason="Local synthetic publisher stopped")))
            self.stopped = True


def main(argv=None):
    parser = argparse.ArgumentParser(description="Breakwater local synthetic G1 acquisition")
    parser.add_argument("--seed",type=int,default=1337)
    parser.add_argument("--preset",choices=PRESETS,default="healthy")
    parser.add_argument("--onset-s",type=float,default=60)
    parser.add_argument("--recovery-s",type=float,default=180)
    args = parser.parse_args(argv)
    commands = queue.Queue(maxsize=16)
    input_failed = threading.Event()

    def read_commands():
        buffered=bytearray()
        try:
            while True:
                # A daemon blocked in BufferedReader can abort CPython during
                # finalization when QProcess keeps stdin open after a stop ack.
                # Raw bounded reads avoid that lock; stop also ends this reader.
                chunk=os.read(sys.stdin.fileno(),4096)
                if not chunk:
                    if buffered:
                        raise ValueError("Truncated command input")
                    commands.put_nowait(None)
                    return
                buffered.extend(chunk)
                while b"\n" in buffered:
                    end=buffered.index(b"\n")+1
                    if end>MAX_FRAME:
                        raise ValueError("Oversized command")
                    command=strict_json(bytes(buffered[:end])); del buffered[:end]
                    commands.put_nowait(command)
                    if isinstance(command,dict) and command.get("operation")=="stop":
                        return
                if len(buffered)>=MAX_FRAME:
                    raise ValueError("Oversized partial command")
        except (ValueError,UnicodeError,OSError,queue.Full):
            input_failed.set()

    try:
        configuration = read_document(os.environ["BREAKWATER_TELEMETRY_CONFIGURATION_FILE"])
        context = read_document(os.environ["BREAKWATER_TELEMETRY_CONTEXT_FILE"])
        publisher = Publisher(configuration,context,os.environ["BREAKWATER_TELEMETRY_CONFIGURATION_HASH"],
                              os.environ["BREAKWATER_TELEMETRY_PHYSICAL_HASH"],seed=args.seed,preset=args.preset,
                              onset_s=args.onset_s,recovery_s=args.recovery_s)
        threading.Thread(target=read_commands,daemon=True).start()
        while not publisher.stopped:
            if input_failed.is_set():
                raise ValueError("Malformed or overflowing command input")
            publisher.tick()
            try:
                command = commands.get(timeout=0.01)
            except queue.Empty:
                continue
            if command is None or command is False or commands.full():
                raise ValueError("Command input ended or exceeded its bound")
            publisher.command(command)
        return 0
    except (ValueError,KeyError,TypeError,IndexError,OSError,UnicodeError):
        # A literal diagnostic does not echo user paths, raw input or environment.
        sys.stderr.write("Local synthetic acquisition failed; check the registered configuration and lifecycle input.\n")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

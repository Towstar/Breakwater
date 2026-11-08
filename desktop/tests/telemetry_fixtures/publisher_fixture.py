"""Protocol-only synthetic values for lifecycle tests; no plant/mapper physics."""
import json
import os
import queue
import sys
import threading
import time
import uuid

mode = sys.argv[1] if len(sys.argv) > 1 else "normal"
with open(os.environ["BREAKWATER_TELEMETRY_CONFIGURATION_FILE"], encoding="utf-8") as stream:
    configuration = json.load(stream)
configuration_hash = os.environ["BREAKWATER_TELEMETRY_CONFIGURATION_HASH"]
physical_hash = os.environ["BREAKWATER_TELEMETRY_PHYSICAL_HASH"]
stream_id = "fixture-stream"
epoch_id = "epoch-" + uuid.uuid4().hex
sequence = 0
# The stale-lifecycle case must remain strictly above its deliberately old
# event-time-zero end frame, even if pause arrives before the first heartbeat.
event = 100.0 if mode == "stale_lifecycle" else 0.0
paused = False
rate = 1.0
commands = queue.Queue()


def read_commands():
    for line in sys.stdin:
        commands.put(json.loads(line))


def emit(kind, payload, *, epoch=None, stream=None, at=None):
    global sequence
    frame = dict(telemetry_version=1, kind="stream_frame", type=kind,
                 stream_id=stream or stream_id, epoch_id=epoch or epoch_id, sequence=sequence,
                 configuration_hash=configuration_hash, physical_hash=physical_hash,
                 event_time_s=event if at is None else at, payload=payload)
    wire = json.dumps(frame, separators=(",", ":")) + "\n"
    if mode == "fragmented":
        for index in range(0, len(wire), 17):
            sys.stdout.write(wire[index:index+17])
            sys.stdout.flush()
            time.sleep(0.001)
    else:
        sys.stdout.write(wire)
        sys.stdout.flush()
    sequence += 1
    return wire


if mode == "silent":
    time.sleep(60)
    sys.exit(0)
if mode == "oversized":
    sys.stdout.write("x" * (256 * 1024 + 1))
    sys.stdout.flush()
    time.sleep(60)
    sys.exit(0)
if mode == "stderr_flood":
    sys.stderr.write("x" * (64 * 1024 + 1))
    sys.stderr.flush()
    time.sleep(60)
    sys.exit(0)
threading.Thread(target=read_commands, daemon=True).start()
start_wire = emit("start", dict(mode="synthetic_live", clock_rate=rate, paused=paused))
if mode == "duplicate_start":
    sys.stdout.write(start_wire)
    sys.stdout.flush()
readings = []
for sensor in configuration["sensors"]:
    value = "unavailable" if sensor["quantity"] == "pump_state" else 132.0
    readings.append(dict(sensor_id=sensor["sensor_id"], event_time_s=event,
                         acquisition_start_s=event, acquisition_end_s=event,
                         value=value, unit=sensor["unit"], quality="good"))
emit("readings", dict(readings=readings))
if mode == "heartbeat_clock":
    emit("heartbeat", dict(paused=True, clock_rate=1.0))
if mode == "crash":
    os._exit(7)
last = time.monotonic()
while True:
    time.sleep(0.05)
    current = time.monotonic()
    if not paused:
        event += (current-last) * rate
    last = current
    try:
        command = commands.get_nowait()
    except queue.Empty:
        emit("heartbeat", dict(paused=paused, clock_rate=rate))
        continue
    if mode == "no_ack":
        continue
    operation = command["operation"]
    if operation == "pause":
        paused = True
    elif operation == "resume":
        paused = False
    elif operation == "set_rate":
        rate = command["payload"]["clock_rate"]
    request_id = "wrong-request" if mode == "wrong_ack" else command["request_id"]
    acknowledgement = dict(request_id=request_id, operation=operation,
                           effective_event_time_s=event, paused=paused, clock_rate=rate)
    if mode == "stale_lifecycle":
        emit("ack", acknowledgement, epoch="retired-epoch")
        emit("ack", acknowledgement, stream="retired-stream")
        emit("end", dict(reason="old epoch ended"), epoch="retired-epoch")
        emit("end", dict(reason="future evidence only"), at=event+100)
        emit("end", dict(reason="older than watermark"), at=0)
        if len(sys.argv) > 2:
            # Test-only acknowledgement gate: the harness first observes that
            # stale frames had no effect, then permits the correct reply. This
            # avoids depending on a wall-clock sleep under ASan or runner load.
            deadline = time.monotonic() + 10
            while not os.path.exists(sys.argv[2]):
                if time.monotonic() > deadline:
                    sys.exit(2)
                time.sleep(0.01)
        else:
            time.sleep(0.5)
    ack_wire = emit("ack", acknowledgement)
    if mode == "duplicate_ack":
        sys.stdout.write(ack_wire)
        sys.stdout.flush()
    if operation == "stop":
        emit("end", dict(reason="fixture stopped"))
        if mode == "no_exit":
            time.sleep(60)
        break
    if operation == "restart":
        epoch_id = "epoch-" + uuid.uuid4().hex
        sequence = 0
        event = 0.0
        emit("start", dict(mode="synthetic_live", clock_rate=rate, paused=paused))
        for reading in readings:
            reading.update(event_time_s=0.0, acquisition_start_s=0.0, acquisition_end_s=0.0)
        emit("readings", dict(readings=readings))

from __future__ import annotations

import asyncio
import collections.abc
import logging
import sys
import typing

import js

# Patch some built-in modules so that pyserial imports
try:
    import fcntl  # noqa: F401
except ImportError:
    sys.modules["fcntl"] = object()  # type: ignore[assignment]

try:
    import termios  # noqa: F401
except ImportError:
    sys.modules["termios"] = object()  # type: ignore[assignment]

class MockSqlite3:
    sqlite_version = "3.31.1"
    sqlite_version_info = (3, 31, 1)
try:
    import sqlite3  # noqa: F401
except ImportError:
    sys.modules["sqlite3"] = MockSqlite3()

_SERIAL_PORT = None
_SERIAL_PORT_CLOSING_TASKS = []

_LOGGER = logging.getLogger(__name__)


async def close_port(port: Any) -> None:
    _LOGGER.debug("Closing serial port")
    # XXX: `port.close` isn't a coroutine, it's an awaitable, so it cannot be directly
    # passed into `asyncio.create_task()`
    await port.close()
    _LOGGER.debug("Closed serial port")


class WebSerialTransport(asyncio.Transport):
    def __init__(
        self,
        loop: asyncio.BaseEventLoop,
        protocol: asyncio.Protocol,
        port,
    ) -> None:
        super().__init__()
        self._loop: asyncio.BaseEventLoop = loop
        self._protocol: asyncio.BaseProtocol | None = protocol
        self._port = port

        self._write_queue: asyncio.Queue = asyncio.Queue()
        self._is_closing = False

        self._js_reader = self._port.readable.getReader()
        self._js_writer = self._port.writable.getWriter()

        self._reader_task = loop.create_task(self._reader_loop())
        self._writer_task = loop.create_task(self._writer_loop())

        self._loop.call_soon(self._protocol.connection_made, self)

    async def _writer_loop(self) -> None:
        while True:
            chunk = await self._write_queue.get()

            try:
                await self._js_writer.write(js.Uint8Array.new(chunk))
            except Exception as e:
                self._cleanup(e)
                break

    async def _reader_loop(self) -> None:
        while True:
            result = await self._js_reader.read()
            if result.done:
                self._cleanup(RuntimeError("Other side has closed"))
                return

            self._protocol.data_received(bytes(result.value))

    def write(self, data: bytes) -> None:
        self._write_queue.put_nowait(data)

    def set_protocol(self, protocol: asyncio.BaseProtocol) -> None:
        self._protocol = protocol

    def get_protocol(self) -> asyncio.BaseProtocol:
        assert self._protocol is not None
        return self._protocol

    def is_closing(self) -> bool:
        return self._is_closing

    def __del__(self):
        self._cleanup(RuntimeError("Transport was not closed!"))

    def _cleanup(self, exception: BaseException | None) -> None:
        self._is_closing = True

        self._reader_task.cancel()
        self._writer_task.cancel()

        if self._js_reader is not None:
            self._js_reader.releaseLock()
            self._js_reader = None

        if self._js_writer is not None:
            self._js_writer.releaseLock()
            self._js_writer = None

        closing_task = None

        if self._port is not None:
            closing_task = asyncio.create_task(close_port(self._port))
            _SERIAL_PORT_CLOSING_TASKS.append(closing_task)
            self._port = None

        if self._protocol is not None:
            if closing_task is None:
                self._protocol.connection_lost(exception)
            else:
                closing_task.add_done_callback(
                    lambda _, protocol=self._protocol: protocol.connection_lost(exception)
                )
                closing_task.add_done_callback(
                    lambda _: _SERIAL_PORT_CLOSING_TASKS.remove(closing_task)
                )

            self._protocol = None

    def close(self) -> None:
        self._cleanup(None)


def set_global_serial_port(serial_port) -> None:
    global _SERIAL_PORT
    _SERIAL_PORT = serial_port


async def create_serial_connection(
    loop: asyncio.BaseEventLoop,
    protocol_factory: typing.Callable[[], asyncio.Protocol],
    url: str,
    *,
    parity=None,
    stopbits=None,
    baudrate: int,
    rtscts=False,
    xonxoff=False,
) -> tuple[WebSerialTransport, asyncio.Protocol]:
    while _SERIAL_PORT_CLOSING_TASKS:
        _LOGGER.warning(
            "Serial connection was not closed before a new one was opened!"
            " Waiting before opening a new one."
        )
        await _SERIAL_PORT_CLOSING_TASKS.pop()

    # `url` is ignored, `_SERIAL_PORT` is used instead
    await _SERIAL_PORT.open(
        baudRate=baudrate,
        flowControl="hardware" if rtscts else None,
    )

    protocol = protocol_factory()
    transport = WebSerialTransport(loop, protocol, _SERIAL_PORT)

    return transport, protocol



# Directly patch zigpy-serial
import zigpy.serial
zigpy.serial.create_serial_connection = create_serial_connection

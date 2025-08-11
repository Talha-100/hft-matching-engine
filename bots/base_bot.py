import asyncio
import signal
import logging
import argparse
from typing import Optional, Tuple

"""
BaseBot: Async TCP client for the C++ engine.

Protocol (current engine):
- Send lines like: "BUY <price> <qty>\n" or "SELL <price> <qty>\n" or "CANCEL <id>\n"
- Receive per-line responses, e.g.:
    "CONFIRMED OrderID: 12"
    "TRADE BuyID: 12, SellID: 8, Price: 101.50, Quantity: 5"
    "MARKET TRADE Price: 101.50, Quantity: 5"   (when you add broadcasting)
    "INVALID INPUT"

Design:
- Async line-based reader
- Reconnect-on-failure (configurable)
- Hook methods you can override in subclasses:
    - on_connected / on_disconnected
    - on_confirm(order_id)
    - on_trade_fill(buy_id, sell_id, price, qty)
    - on_market_trade(price, qty)
    - on_invalid_input(line)
    - on_raw(line)       # always called last
"""

DEFAULT_HOST = '127.0.0.1'
DEFAULT_PORT = 12345
RECONNECT_DELAY_SECS = 1.0

class BaseBot:
    def __init__(self, host: str = DEFAULT_HOST, port: int = DEFAULT_PORT, name: str = "bot"):
        self.host = host
        self.port = port
        self.name = name
        self._reader: Optional[asyncio.StreamReader] = None
        self._writer: Optional[asyncio.StreamWriter] = None
        self._stop = asyncio.Event()
        self._connected = asyncio.Event()
        self._log = logging.getLogger(self.name)
        self._in_welcome_message = False

    # ---------- Public API ----------

    async def start(self, reconnect: bool = True) -> None:
        """
        Start the bot: connect, run read loop, and keep trying unless stopped.
        """
        self._install_signal_handlers()
        while not self._stop.is_set():
            try:
                await self._connect()
                await self.on_connected()
                await self._read_loop() # returns when disconnected
            except asyncio.CancelledError:
                raise
            except Exception as e:
                self._log.exception("Exception in main loop: %s", e)
            
            await self.on_disconnected()
            self._connected.clear()
            self._close_writer()

            if not reconnect or self._stop.is_set():
                break

            self._log.info("Reconnecting in %.1f seconds...", RECONNECT_DELAY_SECS)
            try:
                await asyncio.wait_for(self._stop.wait(), RECONNECT_DELAY_SECS)
            except asyncio.TimeoutError:
                pass # loop and try to reconnect

    async def stop(self) -> None:
        """
        Signal graceful shutdown.
        """
        self._stop.set()
        self._close_writer()

    async def send_order(self, side: str, price: float, qty: int) -> None:
        """
        Send BUY/SELL order.
        Accepts int or float for price; formats with 2dp.
        """
        if side.upper() not in ('BUY', 'SELL'):
            raise ValueError("Invalid order side")
        line = f"{side.upper()} {price:.2f} {qty}\n"
        await self._send_line(line)

    async def cancel_order(self, order_id: int) -> None:
        """
        Cancel an order by ID.
        """
        line = f"CANCEL {order_id}\n"
        await self._send_line(line)

    # ---------- Overridable hooks ----------

    async def on_connected(self) -> None:
        self._log.info("Connected to %s:%d", self.host, self.port)

    async def on_disconnected(self) -> None:
        self._log.info("Disconnected.")

    async def on_confirm(self, order_id: int) -> None:
        self._log.info("CONFIRMED OrderID: %d", order_id)

    async def on_trade_fill(self, buy_id: int, sell_id: int, price: float, qty: int) -> None:
        self._log.info("TRADE FILL BuyID: %d, SellID: %d, Price: %.2f, Quantity: %d", buy_id, sell_id, price, qty)

    async def on_market_trade(self, price: float, qty: int) -> None:
        self._log.info("MARKET TRADE Price: %.2f, Quantity: %d", price, qty)

    async def on_invalid_input(self, line: str) -> None:
        self._log.warning("INVALID INPUT: %s", line)

    async def on_cancel(self, order_id: int) -> None:
        self._log.info("CANCELLED OrderID: %d", order_id)

    async def on_welcome_message(self, line: str) -> None:
        """
        Called for welcome/info messages from the server.
        Override this to handle welcome messages differently.
        """
        self._log.debug("WELCOME: %s", line)

    async def on_raw(self, line: str) -> None:
        """
        Called on specific handlers; useful for logging everything.
        """
        self._log.debug("RAW <- %s", line)

    # ---------- Internals ----------

    def _install_signal_handlers(self) -> None:
        # Graceful Ctrl+C
        loop = asyncio.get_running_loop()
        for sig in (signal.SIGINT, signal.SIGTERM):
            try:
                loop.add_signal_handler(sig, lambda: asyncio.create_task(self.stop()))
            except NotImplementedError:
                # Windows may not support all signals
                pass
    
    async def _connect(self) -> None:
        self._log.info("Connecting to %s:%d", self.host, self.port)
        self._reader, self._writer = await asyncio.open_connection(self.host, self.port)
        self._connected.set()
    
    async def _send_line(self, line: str) -> None:
        await self._connected.wait()
        if not self._writer:
            raise ConnectionError("Not connected to server")
        self._log.debug("RAW -> %s", line.strip())
        self._writer.write(line.encode("utf-8"))
        await self._writer.drain()

    async def _read_loop(self) -> None:
        if not self._reader:
            raise RuntimeError("Not connected")
        while not self._stop.is_set():
            line = await self._reader.readline()
            if not line:
                break
            text = line.decode("utf-8", errors="replace")
            await self._dispatch_line(text)
    
    async def _dispatch_line(self, line: str) -> None:
        s = line.strip()
        
        # Skip empty lines
        if not s:
            return

        # Check if this is the start of a welcome message
        if s.startswith("=") and "=" in s[1:]:  # Lines with multiple = characters
            self._in_welcome_message = True
            await self.on_welcome_message(s)
            await self.on_raw(s)
            return

        # If we're in a welcome message, check if it's ending
        if self._in_welcome_message:
            # Welcome message ends with the final === line
            if s.startswith("=") and s.endswith("=") and len(s) > 10:
                await self.on_welcome_message(s)
                await self.on_raw(s)
                self._in_welcome_message = False
                return
            # All lines during welcome message are welcome content
            await self.on_welcome_message(s)
            await self.on_raw(s)
            return

        # Handle any remaining welcome/info messages that might not be in the main block
        if ("Welcome" in s or 
            "Available Commands" in s or
            "Place a" in s or
            "Cancel an" in s or
            "Disconnect from" in s):
            await self.on_welcome_message(s)
            await self.on_raw(s)
            return

        # Fast-path checks for protocol messages (order matters; keep conservative parsing)
        if s.startswith("CONFIRMED"):
            order_id = _parse_trailing_int(s, key="OrderID:")
            if order_id is not None:
                await self.on_confirm(order_id)
            else:
                await self.on_invalid_input(s)
        
        elif s.startswith("TRADE "):
            buy_id, sell_id, price, qty = _parse_trade_line(s)
            if None not in (buy_id, sell_id, price, qty):
                await self.on_trade_fill(buy_id, sell_id, price, qty)
            else:
                await self.on_invalid_input(s)
        
        elif s.startswith("MARKET TRADE"):
            price, qty = _parse_market_trade_line(s)
            if None not in (price, qty):
                await self.on_market_trade(price, qty)
            else:
                await self.on_invalid_input(s)
        
        elif s.startswith("INVALID INPUT"):
            await self.on_invalid_input(s)

        elif s.startswith("CANCELLED"):
            order_id = _parse_trailing_int(s, key="OrderID:")
            if order_id is not None:
                await self.on_cancel(order_id)
            else:
                await self.on_invalid_input(s)
        
        else:
            # Check if this might be a welcome message we missed
            if any(keyword in s.lower() for keyword in ["command", "order", "engine", "server", "help"]):
                await self.on_welcome_message(s)
            else:
                self._log.info("Unknown line: %s", s)
        
        # Always call on_raw for full traceability
        await self.on_raw(s)
    
    def _close_writer(self) -> None:
        try:
            if self._writer:
                self._writer.close()
        finally:
            self._writer = None
            self._reader = None

# ---------- Lightweight parsers (no regex for speed/clarity) ----------

def _parse_trailing_int(s: str, key: str) -> Optional[int]:
    try:
        idx = s.index(key)
        num = s[idx + len(key):].strip(",")
        return int(num)
    except Exception:
        return None

def _parse_float_field(s: str, key: str) -> Optional[float]:
    try:
        idx = s.index(key)
        num = s[idx + len(key):].split(",")[0].strip()
        return float(num)
    except Exception:
        return None

def _parse_int_field(s: str, key: str) -> Optional[int]:
    try:
        idx = s.index(key)
        num = s[idx + len(key):].split(",")[0].strip()
        return int(num)
    except Exception:
        return None

def _parse_trade_line(s: str) -> Tuple[Optional[int], Optional[int], Optional[float], Optional[int]]:
    # Expect: "TRADE BuyID: X, SellID: Y, Price: P, Quantity: Q"
    buy_id = _parse_int_field(s, "BuyID:")
    sell_id = _parse_int_field(s, "SellID:")
    price = _parse_float_field(s, "Price:")
    qty = _parse_int_field(s, "Quantity:")
    return buy_id, sell_id, price, qty

def _parse_market_trade_line(s: str) -> Tuple[Optional[float], Optional[int]]:
    # Expect: "MARKET TRADE Price: P, Quantity: Q"
    price = _parse_float_field(s, "Price:")
    qty = _parse_int_field(s, "Quantity:")
    return price, qty

# ---------- CLI runner (useful for quick manual testing) ----------

async def _demo_cli(bot: BaseBot):
    # Simple interactive prompt so you can play in a terminal
    await bot.start(reconnect=True)

def _build_arg_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="BaseBot TCP client for hft-matching-engine")
    p.add_argument("--host", default=DEFAULT_HOST)
    p.add_argument("--port", type=int, default=DEFAULT_PORT)
    p.add_argument("--name", default="bot")
    p.add_argument("--log", default="INFO", help="DEBUG, INFO, WARNING, ERROR")
    return p

def _setup_logging(level: str) -> None:
    lvl = getattr(logging, level.upper(), logging.INFO)
    logging.basicConfig(
        level=lvl,
        format="%(asctime)s | %(name)s | %(levelname)s | %(message)s",
    )

if __name__ == "__main__":
    args = _build_arg_parser().parse_args()
    _setup_logging(args.log)
    bot = BaseBot(
       host=args.host,
       port=args.port,
       name=args.name,
    )
    try:
        asyncio.run(_demo_cli(bot))
    except KeyboardInterrupt:
        pass

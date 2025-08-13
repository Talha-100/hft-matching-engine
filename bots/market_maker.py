import asyncio
import logging
import argparse
from collections import deque
from typing import Optional, Tuple, Deque
from base_bot import BaseBot

"""
MarketMakerBot:
- Maintains bid/ask around a mid.
- Refreshes quotes on a timer.
- Tracks inventory and simple P&L.
- Optionally cancels prior quotes before re-quoting (requires server CANCEL support).

Protocol assumptions (from engine):
- "BUY <price> <qty>\n" / "SELL <price> <qty>\n"
- "CONFIRMED OrderID: <id>"
- "TRADE BuyID: X, SellID: Y, Price: P, Quantity: Q"
- Optional broadcast: "MARKET TRADE Price: P, Quantity: Q"
- Optional cancel: "CANCEL <id>\n"
"""

class MarketMakerBot(BaseBot):
    def __init__(
        self,
        mid: float,
        spread: float,
        qty: int,
        refresh_ms: int,
        ema_alpha: float,
        max_position: int,
        use_cancel: bool,
        **kwargs
    ):
        super().__init__(**kwargs)
        self.mid = mid
        self.spread = spread
        self.qty = qty
        self.refresh_ms = refresh_ms
        self.ema_alpha = ema_alpha
        self.max_position = max_position
        self.use_cancel = use_cancel

        # State
        self._inv = 0
        self._cash = 0
        self._last_trade: Optional[float] = None

        self._bid_order_id: Optional[int] = None
        self._ask_order_id: Optional[int] = None

        # track which side we just sent to map CONFIRMED -> order_id
        self._pending_confirms: Deque[str] = deque()
        self._lock = asyncio.Lock()

    # ---------- lifecycle ----------

    async def on_connected(self):
        await super().on_connected()
        asyncio.create_task(self._run())
    
    # ---------- incoming events ----------

    async def on_confirm(self, order_id: int) -> None:
        async with self._lock:
            side = self._pending_confirms.popleft() if self._pending_confirms else None
            if side == "BID":
                self._bid_order_id = order_id
                self._log.info("QUOTE CONFIRMED BidId: %d", order_id)
            elif side == "ASK":
                self._ask_order_id = order_id
                self._log.info("QUOTE CONFIRMED AskId: %d", order_id)
            else:
                self._log.info("QUOTE CONFIRMED (unmapped) id: %d", order_id)
    
    async def on_trade_fill(self, buy_id: int, sell_id: int, price: float, qty: int) -> None:
        async with self._lock:
            # Our fills: adjust inventory and cash
            if self._bid_order_id == buy_id:
                self._inv += qty
                self._cash -= price * qty
            elif self._ask_order_id == sell_id:
                self._inv -= qty
                self._cash += price * qty
            self._last_trade = price
            self._log.info("FILL @ %.2f x %d | inv=%d PnL=%.2f",
                           price, qty, self._inv, self.mark_to_market())
    
    async def on_market_trade(self, price: float, qty: int) -> None:
        # use EMA of last market trades to drift mid slowly
        if self._last_trade is None:
            self._last_trade = price
        self._last_trade = self.ema_alpha * price + (1 - self.ema_alpha) * self._last_trade
        # nudge mid towards EMA to keep quotes relevant
        self.mid = self._last_trade
    
    # ---------- quoting loop ----------

    async def _run(self):
        while not self._stop.is_set():
            try:
                await self._refresh_quotes()
                await asyncio.sleep(self.refresh_ms / 1000.0)
            except Exception as e:
                self._log.exception("Quote loop error: %s", e)

    async def _refresh_quotes(self):
        async with self._lock:
            self._log.info("Refreshing quotes...")
            # 1. cancel existing quotes (optional)
            if self.use_cancel:
                if self._bid_order_id is not None:
                    await self.cancel_order(self._bid_order_id)
                    self._bid_order_id = None
                if self._ask_order_id is not None:
                    await self.cancel_order(self._ask_order_id)
                    self._ask_order_id = None
            
            # 2. compute target bid/ask with simple inventory tilt
            tilt = min(max(self._inv / max(1, self.max_position), -1.0), 1.0)
            tilt_bp = 0.25 * self.spread * tilt # bias quotes when inventory is large
            half = self.spread / 2.0
            bid_px = max(0.01, self.mid - half + tilt_bp)
            ask_px = max(bid_px + 0.01, self.mid + half + tilt_bp) # ensures ask > bid

            # 3. place fresh quotes, respecting position limits
            place_bid, place_ask = self._position_allows()

            if place_bid:
                await self.send_order("BUY", bid_px, self.qty)
                self._pending_confirms.append("BID")
            if place_ask:
                await self.send_order("SELL", ask_px, self.qty)
                self._pending_confirms.append("ASK")

            self._log.info("QUOTED bid=%.2f ask=%.2f inv=%d PnL=%.2f",
                           bid_px, ask_px, self._inv, self.mark_to_market())

    def _position_allows(self) -> Tuple[bool, bool]:
        # naive hard caps
        allow_bid = self._inv < self.max_position
        allow_ask = -self._inv < self.max_position
        return allow_bid, allow_ask

    # ---------- metrics ----------

    def mark_to_market(self) -> float:
        # Compute the current PnL based on the mid price
        ref = self._last_trade if self._last_trade is not None else self.mid
        return self._cash + self._inv * ref

# ---------- CLI ----------

def _build_arg_parser():
    p = argparse.ArgumentParser(description="MarketMaker bot for hft-matching-engine")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=12345)
    p.add_argument("--name", default="market_maker")
    p.add_argument("--mid", type=float, default=100.0)
    p.add_argument("--spread", type=float, default=0.20)
    p.add_argument("--qty", type=int, default=2)
    p.add_argument("--refresh-ms", type=int, default=250)
    p.add_argument("--ema-alpha", type=float, default=0.2)
    p.add_argument("--max-position", type=int, default=50)
    p.add_argument("--use-cancel", action="store_true",
                   help="Cancel previous quotes before re-quoting (requires server CANCEL)")
    p.add_argument("--log", default="INFO")
    return p

def _setup_logging(level: str):
    import logging
    lvl = getattr(logging, level.upper(), logging.INFO)
    logging.basicConfig(level=lvl, format="%(asctime)s | %(name)s | %(levelname)s | %(message)s")

if __name__ == "__main__":
    args = _build_arg_parser().parse_args()
    _setup_logging(args.log)
    bot = MarketMakerBot(
        mid=args.mid,
        spread=args.spread,
        qty=args.qty,
        refresh_ms=args.refresh_ms,
        ema_alpha=args.ema_alpha,
        max_position=args.max_position,
        use_cancel=args.use_cancel,
        host=args.host,
        port=args.port,
        name=args.name,
    )
    try:
        asyncio.run(bot.start(reconnect=True))
    except KeyboardInterrupt:
        pass
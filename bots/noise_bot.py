import asyncio
import logging
import random
import argparse
from base_bot import BaseBot

"""
NoiseBot:
- Inherits BaseBot
- Generates random BUY/SELL orders around a configurable mid price
- Random sleep between orders to simulate chaotic market flow
"""

class NoiseBot(BaseBot):
    def __init__(self, mid:float, spread: float, sigma: float,
                 qty_min: int, qty_max: int,
                 sleep_min_ms: int, sleep_max_ms: int,
                 **kwargs):
        super().__init__(**kwargs)
        self.mid = mid
        self.spread = spread
        self.sigma = sigma
        self.qty_min = qty_min
        self.qty_max = qty_max
        self.sleep_min_ms = sleep_min_ms
        self.sleep_max_ms = sleep_max_ms

    async def on_connected(self):
        await super().on_connected()
        asyncio.create_task(self._order_loop())
    
    async def _order_loop(self):
        """
        Continuously send random orders until stopped.
        """
        while not self._stop.is_set():
            # Pick a side
            side = random.choice(['BUY', 'SELL'])

            # Pick a price: mid ± spread/2 + small noise
            base_price = self.mid + ((self.spread / 2) if side == "BUY" else -(self.spread / 2))
            noisy_price = base_price + random.gauss(0, self.sigma)

            # Clamp price
            price = max(0.01, noisy_price)

            # Pick a quantity
            qty = random.randint(self.qty_min, self.qty_max)

            # Send the order
            await self.send_order(side, price, qty)

            # Random walk mid to simulate market movement
            self.mid += random.gauss(0, self.sigma / 4)

            # Sleep for a random duration
            sleep_time = random.randint(self.sleep_min_ms, self.sleep_max_ms)
            await asyncio.sleep(sleep_time / 1000)

def _build_arg_parser():
    p = argparse.ArgumentParser(description="NoiseBot for hft-matching-engine")
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=12345)
    p.add_argument("--name", default="noisebot")
    p.add_argument("--mid", type=float, default=100.0, help="Starting mid price")
    p.add_argument("--spread", type=float, default=0.20, help="Bid/Ask spread")
    p.add_argument("--sigma", type=float, default=0.05, help="Price noise stddev")
    p.add_argument("--qty-min", type=int, default=1)
    p.add_argument("--qty-max", type=int, default=10)
    p.add_argument("--sleep-min-ms", type=int, default=50)
    p.add_argument("--sleep-max-ms", type=int, default=250)
    p.add_argument("--log", default="INFO", help="DEBUG, INFO, WARNING, ERROR")
    return p

def _setup_logging(level: str):
    lvl = getattr(logging, level.upper(), logging.INFO)
    logging.basicConfig(
        level=lvl,
        format="%(asctime)s | %(name)s | %(levelname)s | %(message)s",
    )


if __name__ == "__main__":
    args = _build_arg_parser().parse_args()
    _setup_logging(args.log)
    bot = NoiseBot(
        mid=args.mid,
        spread=args.spread,
        sigma=args.sigma,
        qty_min=args.qty_min,
        qty_max=args.qty_max,
        sleep_min_ms=args.sleep_min_ms,
        sleep_max_ms=args.sleep_max_ms,
        host=args.host,
        port=args.port,
        name=args.name
    )
    try:
        asyncio.run(bot.start(reconnect=True))
    except KeyboardInterrupt:
        pass
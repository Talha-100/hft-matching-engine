# hft-matching-engine

A high-performance matching engine simulator written in modern C++. The goal is to replicate a central limit order book (CLOB) for building and testing high-frequency trading strategies in a controlled environment.

---

## 🚀 Overview

This project simulates key components of a real electronic exchange, including:

- Limit order matching with price-time priority
- Multiclient TCP networking for external bot connections
- Simple trading bots (e.g. noise bot, market maker)

---

## ✅ Progress So Far

- Core matching engine with FIFO order matching
- Boost.Asio-based TCP server for client communication
- Initial bot framework with basic market behaviours
- Structured terminal output and logging

---

## 🧭 Next Steps

- Expand bot types and add randomness
- Improve order book logic (e.g. partial fills, edge cases)
- Add market data broadcasting

---

## 🛠 Tech Stack

- C++20
- Boost.Asio
- GoogleTest
- Python (for bots)

---

MIT License • Created by Talha Ahmed

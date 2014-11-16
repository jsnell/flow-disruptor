/* -*- mode: c++; c-basic-offset: 4 indent-tabs-mode: nil -*- */
/*
 * Copyright 2014 Teclo Networks AG
 */

#include "throttler.h"

Throttler::Throttler(State* state)
    : enabled_(false),
      tick_timer_(state, this, std::mem_fn(&Throttler::tick)) {
}

void Throttler::enable(const LinkProperties& properties) {
    enabled_ = true;
    throttle_kbps_ = properties.throughput_kbps();
    recompute();
    tick_timer_.reschedule(0.001);
}

void Throttler::insert(uint64_t cost, const Throttler::Callback& callback) {
    if (!enabled_) {
        callback();
    } else if (capacity_ > cost) {
        capacity_ -= cost;
        callback();
    } else {
        callbacks_.push_back(std::make_pair(cost, callback));
    }
}

void Throttler::tick() {
    capacity_ = std::min(max_capacity_,
                         capacity_ + capacity_per_tick_);
    while (!callbacks_.empty()) {
        uint64_t cost = callbacks_.front().first;
        if (capacity_ < cost) {
            break;
        }
        capacity_ -= cost;
        callbacks_.front().second();
        callbacks_.pop_front();
    }

    tick_timer_.reschedule(0.001);
}

void Throttler::recompute() {
    // 0.1 seconds of capacity.
    max_capacity_ = throttle_kbps_ * 1000 / 8.0 * 0.1;
    capacity_ = std::min(max_capacity_, capacity_);
    // Ticks every 0.001 seconds.
    capacity_per_tick_ = max_capacity_ * 0.01;
}

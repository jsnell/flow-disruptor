/* -*- mode: c++; c-basic-offset: 4 indent-tabs-mode: nil -*- */
/*
 * Copyright 2014 Teclo Networks AG
 */

#include "throttler.h"

#include "log.h"

Throttler::Throttler(State* state)
    : enabled_(false),
      tick_timer_(state, [this] (Timer*) { tick(); }),
      queued_cost_(0),
      max_queue_(0) {
}

void Throttler::enable(const LinkProperties& properties) {
    enabled_ = true;
    throttle_kbps_ = properties.throughput_kbps();
    if (properties.has_max_queue_bytes()) {
        max_queue_ = properties.max_queue_bytes();
    }

    recompute();
    tick_timer_.reschedule(0.001);
}

void Throttler::insert(uint64_t cost, const Throttler::Callback& callback) {
    if (!enabled_) {
        callback();
    } else if (capacity_ > cost) {
        capacity_ -= cost;
        callback();
    } if (max_queue_ && queued_cost_ > max_queue_) {
        // Queue full, drop the packet.
    } else {
        queue_.push_back(std::make_pair(cost, callback));
        queued_cost_ += cost;
    }
}

void Throttler::tick() {
    capacity_ = std::min(max_capacity_,
                         capacity_ + capacity_per_tick_);
    while (!queue_.empty()) {
        uint64_t cost = queue_.front().first;
        if (capacity_ < cost) {
            break;
        }
        capacity_ -= cost;
        queued_cost_ -= cost;
        queue_.front().second();
        queue_.pop_front();
    }

    tick_timer_.reschedule(0.001);
}

void Throttler::recompute() {
    uint64_t actual_throttle = std::max(throttle_kbps_, INT64_C(0));

    // 0.1 seconds of capacity.
    max_capacity_ = actual_throttle * 1000 / 8.0 * 0.1;
    capacity_ = std::min(max_capacity_, capacity_);
    // Ticks every 0.001 seconds.
    capacity_per_tick_ = max_capacity_ * 0.01;
}

void Throttler::apply(const LinkProperties& properties) {
    int32_t delta = properties.throughput_kbps_change();

    if (delta) {
        throttle_kbps_ += delta;
        info("Applying throughput change of %d (now %ld)",
             delta,
             throttle_kbps_);
        recompute();
    }
}

void Throttler::revert(const LinkProperties& properties) {
    int32_t delta = properties.throughput_kbps_change();

    if (delta) {
        throttle_kbps_ -= delta;
        info("Reverting throughput change of %d (now %ld)",
             delta,
             throttle_kbps_);
        recompute();
    }
}


#include "transient_activity_collector.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

using namespace rgsml::research::transient_ui;

int main() {
    {   // Exact 20 ms buckets at four C00 rates and no future exposure.
        for (auto fs : {44100U, 48000U, 96000U, 192000U}) {
            TransientActivityCollector c(fs, 100U, 3);
            assert(c.status() == ObservationStatus::OK);
            assert(c.bucket_frames() == fs / 50U);
            assert(c.reserved_capacity() >= 3U);
            auto count = c.bucket_frames();
            for (std::uint32_t i = 0; i < count; ++i) {
                double a = i == 20U ? 6.0 : (i == 40U ? -2.0 : 1.0);
                double s = i == 20U ? -5.0 : (i == 40U ? 3.0 : -1.0);
                assert(c.push(100U + i, a, s));
            }
            assert(c.visible_window(100U + count - 1U, count).empty());
            const auto b = c.visible_window(100U + count, count);
            assert(b.size() == 1U);
            assert(b[0].frame_begin == 100U);
            assert(b[0].frame_end_exclusive == 100U + count);
            assert(b[0].attack_min_db == -2.0);
            assert(b[0].attack_max_db == 6.0);
            assert(b[0].attack_last_db == 1.0);
            assert(b[0].sustain_min_db == -5.0);
            assert(b[0].sustain_max_db == 3.0);
            assert(b[0].sustain_last_db == -1.0);
            assert(c.push(100U + count, 0.0, 0.0));
            assert(c.finish());
            assert(c.completed_buckets() == 2U);
            assert(c.visible_window(101U + count, 1U).size() == 1U);
            assert(!c.push(102U + count, 0.0, 0.0));
            assert(c.status() == ObservationStatus::ALREADY_FINISHED);
        }
    }
    {   // Absolute timeline discontinuities fail telemetry closed.
        TransientActivityCollector c(48000);
        assert(c.push(0, 1.0, -1.0));
        assert(!c.push(2, 2.0, -2.0));
        assert(c.status() == ObservationStatus::FRAME_DISCONTINUITY);
        assert(c.visible_window(5, 5).empty());
    }
    {   // Invalid observations never masquerade as successful telemetry.
        TransientActivityCollector c(48000);
        assert(!c.push(0, std::numeric_limits<double>::quiet_NaN(), 0));
        assert(c.status() == ObservationStatus::INVALID_SAMPLE);
        TransientActivityCollector d(48000);
        assert(!d.push(0, 13.0, 0));
        assert(d.status() == ObservationStatus::INVALID_SAMPLE);
    }
    {   // Capacity never grows during processing.
        TransientActivityCollector c(48000, 0, 1);
        for (int i = 0; i < 960; ++i) assert(c.push(i, 0.0, 0.0));
        assert(!c.push(960, 0.0, 0.0));
        assert(c.status() == ObservationStatus::CAPACITY_EXCEEDED);
        assert(c.reserved_capacity() >= 1U);
    }
    {   // Full history supports seeking backwards within the same realization.
        TransientActivityCollector c(48000, 0, 4);
        for (int i = 0; i < 960 * 3; ++i) assert(c.push(i, i / 960.0 / 1000.0, 0));
        assert(c.finish());
        assert(c.visible_window(960, 960).size() == 1U);
        assert(c.visible_window(2880, 960).size() == 1U);
        assert(c.visible_window(2000, 960).size() == 1U);
    }
    {   // Invalid sample-rate/configuration rejected.
        TransientActivityCollector a(22050), b(48000, 0, 0);
        assert(a.status() == ObservationStatus::INVALID_CONFIGURATION);
        assert(b.status() == ObservationStatus::INVALID_CONFIGURATION);
    }
    std::cout << "transient activity collector: all tests passed\n";
}

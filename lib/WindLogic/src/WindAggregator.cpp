#include "WindAggregator.h"

#include <cmath>

namespace SQM
{
    namespace Wind
    {

        namespace
        {
            constexpr float PI_F = 3.14159265358979f;
        } // namespace

        void WindAggregator::addSample(float speedMs, float directionDeg)
        {
            const float speed = (std::isfinite(speedMs) && speedMs > 0.0f) ? speedMs : 0.0f;
            speeds[head] = static_cast<uint16_t>(std::lround(std::fmin(speed, 655.0f) * 100.0f));
            directions[head] = (std::isfinite(directionDeg) && directionDeg >= 0.0f)
                                   ? static_cast<uint16_t>(std::lround(std::fmod(directionDeg, 360.0f) * 10.0f))
                                   : NO_DIRECTION;
            head = (head + 1) % HISTORY_SECONDS;
            if (count < HISTORY_SECONDS)
                ++count;
        }

        void WindAggregator::reset()
        {
            head = 0;
            count = 0;
        }

        float WindAggregator::speedMs() const
        {
            const size_t n = count < SPEED_WINDOW_SECONDS ? count : SPEED_WINDOW_SECONDS;
            if (n == 0)
                return 0.0f;
            float sum = 0.0f;
            for (size_t i = 0; i < n; ++i)
                sum += speedAt(indexBack(i));
            return sum / static_cast<float>(n);
        }

        float WindAggregator::gustMs() const
        {
            if (count == 0)
                return 0.0f;
            if (count < GUST_WINDOW_SECONDS)
            {
                float sum = 0.0f;
                for (size_t i = 0; i < count; ++i)
                    sum += speedAt(indexBack(i));
                return sum / static_cast<float>(count);
            }
            float window = 0.0f;
            for (size_t i = 0; i < GUST_WINDOW_SECONDS; ++i)
                window += speedAt(indexBack(i));
            float best = window;
            for (size_t i = GUST_WINDOW_SECONDS; i < count; ++i)
            {
                window += speedAt(indexBack(i)) - speedAt(indexBack(i - GUST_WINDOW_SECONDS));
                if (window > best)
                    best = window;
            }
            return best / static_cast<float>(GUST_WINDOW_SECONDS);
        }

        bool WindAggregator::directionDeg(float &out) const
        {
            const size_t n = count < SPEED_WINDOW_SECONDS ? count : SPEED_WINDOW_SECONDS;
            float x = 0.0f;
            float y = 0.0f;
            for (size_t i = 0; i < n; ++i)
            {
                const size_t idx = indexBack(i);
                if (directions[idx] == NO_DIRECTION || speeds[idx] == 0)
                    continue;
                const float rad = (directions[idx] / 10.0f) * PI_F / 180.0f;
                x += speedAt(idx) * std::sin(rad);
                y += speedAt(idx) * std::cos(rad);
            }
            if (std::fabs(x) < 1e-6f && std::fabs(y) < 1e-6f)
                return false;
            float deg = std::atan2(x, y) * 180.0f / PI_F;
            if (deg < 0.0f)
                deg += 360.0f;
            out = deg >= 359.95f ? 0.0f : deg;
            return true;
        }

    } // namespace Wind
} // namespace SQM

#include "BatteryLogic.h"

// The MCP73871's status lines (research D3).

namespace SQM
{
    namespace Battery
    {
        namespace
        {
            bool samePins(const ChargerPins &a, const ChargerPins &b)
            {
                return a.stat1 == b.stat1 && a.stat2 == b.stat2 && a.pg == b.pg;
            }
        } // namespace

        ChargerOutput decodeCharger(const ChargerPins &p)
        {
            ChargerOutput out;
            out.usbPower = !p.pg; // PG is active low
            const bool s1 = !p.stat1;
            const bool s2 = !p.stat2;
            if (!p.pg)
            {
                if (s1 && !s2)
                    out.state = ChargeState::Charging;
                else if (!s1 && s2)
                    out.state = ChargeState::Charged;
                else if (s1 && s2)
                {
                    // The safety timer is off on Rev A: a fault is the NTC window (or a bad cell).
                    out.state = ChargeState::Fault;
                    out.fault = ChargeFault::Temperature;
                }
                else
                    out.state = ChargeState::NoBattery;
                return out;
            }
            // No USB: running from the pack. STAT1 low here is the charger's
            // low-battery output; any other pattern is unexpected but means the same.
            out.state = ChargeState::Discharging;
            return out;
        }

        ChargerOutput ChargerDecoder::update(const ChargerPins &pins, uint32_t nowMs)
        {
            if (!seen)
            {
                seen = true;
                candidate = pins;
                candidateSince = nowMs;
                togglingSince = nowMs;
            }
            else if (!samePins(pins, candidate))
            {
                // A change within STABLE_MS of the last one continues a run of toggling.
                if (nowMs - candidateSince >= STABLE_MS)
                    togglingSince = nowMs;
                candidate = pins;
                candidateSince = nowMs;
            }

            if (nowMs - candidateSince >= STABLE_MS)
                reported = decodeCharger(candidate);
            else if (candidateSince != togglingSince && nowMs - togglingSince > TOGGLING_MS)
            {
                reported.state = ChargeState::NoBattery;
                reported.usbPower = true;
                reported.fault = ChargeFault::None;
            }
            return reported;
        }

        ChargerOutput chargerFromRate(bool rateValid, float rate)
        {
            ChargerOutput out;
            if (rateValid)
                out.state = rate > MIN_ESTIMATE_RATE ? ChargeState::Charging : ChargeState::Discharging;
            return out;
        }
    } // namespace Battery
} // namespace SQM

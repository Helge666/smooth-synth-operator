#pragma once

namespace sso::kiwi3p
{

/** Sound generator for the Kiwi-3P with the PG-2K categories:
    Bass, Poly, Pads, FX, Keys (piano/clav), Strings, Bells, Brass.

    Voice architecture used for all categories: ENV-1 -> VCF, ENV-2 -> VCA,
    LFO-1 = vibrato via the mod wheel, LFO-2 = slow movement. */
std::unique_ptr<PatchGenerator> createGenerator (const Kiwi3PDevice&);

} // namespace sso::kiwi3p

#pragma once

namespace sso::kiwisix
{

/** Patch generator with the PG-2K style categories, tuned for the PolySix. */
std::unique_ptr<PatchGenerator> createGenerator (const KiwiSixDevice&);

} // namespace sso::kiwisix

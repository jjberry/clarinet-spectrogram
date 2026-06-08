// Unit tests for clarisynth::selectAnalysisChannel.
//
// Regression coverage for the "silent input" bug: ch0 carried a constant noise floor
// (~3e-5 RMS) that sat above the old fixed 1e-6 threshold, so analysis stayed stuck on
// the silent channel while the guitar played on ch1. The fix selects the loudest channel
// instead. These tests pin that behaviour.

#include "ChannelRouting.h"

#include <cstdio>

namespace
{
int failures = 0;

void check (bool cond, const char* name)
{
    if (cond)
    {
        std::printf ("  ok   %s\n", name);
    }
    else
    {
        std::printf ("  FAIL %s\n", name);
        ++failures;
    }
}
} // namespace

int main()
{
    using clarisynth::selectAnalysisChannel;

    std::printf ("selectAnalysisChannel:\n");

    // The exact regression: noise floor on ch0 above the old 1e-6 threshold, signal on ch1.
    {
        const float rms[2] = { 3.0e-5f, 0.035f };
        check (selectAnalysisChannel (rms, 2) == 1,
               "noise floor on ch0, real signal on ch1 -> picks ch1");
    }

    // Signal genuinely on ch0.
    {
        const float rms[2] = { 0.05f, 3.0e-5f };
        check (selectAnalysisChannel (rms, 2) == 0,
               "signal on ch0 -> picks ch0");
    }

    // Both channels silent -> stay on ch0 (don't flap to ch1 on noise).
    {
        const float rms[2] = { 0.0f, 0.0f };
        check (selectAnalysisChannel (rms, 2) == 0,
               "both silent -> ch0");
    }

    // Exactly equal levels -> ties break toward the lower index.
    {
        const float rms[2] = { 0.01f, 0.01f };
        check (selectAnalysisChannel (rms, 2) == 0,
               "equal levels -> ch0 (tie -> lower index)");
    }

    // Loudest among several channels.
    {
        const float rms[4] = { 0.01f, 0.02f, 0.5f, 0.1f };
        check (selectAnalysisChannel (rms, 4) == 2,
               "loudest of four channels -> index 2");
    }

    // Single (mono) input.
    {
        const float rms[1] = { 0.02f };
        check (selectAnalysisChannel (rms, 1) == 0,
               "single channel -> 0");
    }

    // Degenerate inputs.
    check (selectAnalysisChannel (nullptr, 0) == 0, "null / zero channels -> 0");
    {
        const float rms[1] = { 0.02f };
        check (selectAnalysisChannel (rms, 0) == 0, "zero count -> 0");
    }

    using clarisynth::broadcastToAllChannels;

    std::printf ("\nbroadcastToAllChannels:\n");

    auto channelsEqual = [] (const float* a, const float* b, int n)
    {
        for (int i = 0; i < n; ++i)
            if (a[i] != b[i]) return false;
        return true;
    };

    // Mono source on ch0 -> ch1 must receive it (the mirror-image of the original bug).
    {
        float l[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
        float r[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float* ch[2] = { l, r };
        broadcastToAllChannels (ch, 2, /*source*/ 0, 4);
        check (channelsEqual (r, l, 4), "mono on ch0 -> ch1 gets the signal");
    }

    // Source on ch1 -> ch0 must receive it (the original-bug routing).
    {
        float l[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float r[4] = { 5.0f, 6.0f, 7.0f, 8.0f };
        float* ch[2] = { l, r };
        broadcastToAllChannels (ch, 2, /*source*/ 1, 4);
        check (channelsEqual (l, r, 4), "source on ch1 -> ch0 gets the signal");
    }

    // Source channel itself is left untouched.
    {
        float l[3] = { 1.0f, 2.0f, 3.0f };
        float r[3] = { 9.0f, 9.0f, 9.0f };
        float src_copy[3] = { 1.0f, 2.0f, 3.0f };
        float* ch[2] = { l, r };
        broadcastToAllChannels (ch, 2, /*source*/ 0, 3);
        check (channelsEqual (l, src_copy, 3), "source channel is preserved");
    }

    // Broadcast across more than two channels.
    {
        float a[2] = { 4.0f, 4.0f };
        float b[2] = { 0.0f, 0.0f };
        float c[2] = { 0.0f, 0.0f };
        float* ch[3] = { a, b, c };
        broadcastToAllChannels (ch, 3, /*source*/ 0, 2);
        check (channelsEqual (b, a, 2) && channelsEqual (c, a, 2),
               "mono on ch0 -> all of ch1, ch2 filled");
    }

    // Mono output (single channel) is a no-op, not a crash.
    {
        float only[2] = { 1.0f, 2.0f };
        float* ch[1] = { only };
        broadcastToAllChannels (ch, 1, /*source*/ 0, 2);
        check (only[0] == 1.0f && only[1] == 2.0f, "single channel -> no-op");
    }

    // Degenerate args don't write or crash.
    {
        float l[1] = { 1.0f };
        float r[1] = { 2.0f };
        float* ch[2] = { l, r };
        broadcastToAllChannels (ch, 2, /*source*/ 5, 1);   // out-of-range source
        broadcastToAllChannels (nullptr, 2, 0, 1);
        broadcastToAllChannels (ch, 2, 0, 0);              // zero samples
        check (r[0] == 2.0f, "out-of-range / null / zero-sample args are safe no-ops");
    }

    std::printf ("\n%s (%d failure%s)\n",
                 failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED",
                 failures, failures == 1 ? "" : "s");

    return failures == 0 ? 0 : 1;
}

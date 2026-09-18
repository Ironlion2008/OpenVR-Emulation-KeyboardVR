#include <gtest/gtest.h>

#include <Driver/InputMath.hpp>

using namespace OpenVREmulatorDriver;

// ---------------------------------------------------------------------------
// NormalizeThumbAxis
// ---------------------------------------------------------------------------

TEST(NormalizeThumbAxis, ZeroIsZero)
{
    EXPECT_FLOAT_EQ(NormalizeThumbAxis(0, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE), 0.0f);
}

TEST(NormalizeThumbAxis, InsideDeadzoneReturnsZero)
{
    const SHORT dz = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    EXPECT_FLOAT_EQ(NormalizeThumbAxis(dz, dz), 0.0f);
    EXPECT_FLOAT_EQ(NormalizeThumbAxis(-dz, dz), 0.0f);
    EXPECT_FLOAT_EQ(NormalizeThumbAxis(1, dz), 0.0f);
    EXPECT_FLOAT_EQ(NormalizeThumbAxis(-1, dz), 0.0f);
}

TEST(NormalizeThumbAxis, MaxPositiveIsOne)
{
    const float result = NormalizeThumbAxis(32767, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    EXPECT_FLOAT_EQ(result, 1.0f);
}

TEST(NormalizeThumbAxis, MaxNegativeIsNegativeOne)
{
    const float result = NormalizeThumbAxis(-32768, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
    EXPECT_FLOAT_EQ(result, -1.0f);
}

TEST(NormalizeThumbAxis, PositiveScalesMonotonically)
{
    const SHORT dz = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    const float low = NormalizeThumbAxis(static_cast<SHORT>(dz + 100), dz);
    const float high = NormalizeThumbAxis(static_cast<SHORT>(dz + 500), dz);
    EXPECT_GT(high, low);
    EXPECT_GT(low, 0.0f);
}

TEST(NormalizeThumbAxis, NegativeScalesMonotonically)
{
    const SHORT dz = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
    const float low = NormalizeThumbAxis(static_cast<SHORT>(-(dz + 500)), dz);
    const float high = NormalizeThumbAxis(static_cast<SHORT>(-(dz + 100)), dz);
    EXPECT_LT(low, high);
    EXPECT_LT(high, 0.0f);
}

// ---------------------------------------------------------------------------
// NormalizeTrigger
// ---------------------------------------------------------------------------

TEST(NormalizeTrigger, BelowThresholdIsZero)
{
    EXPECT_FLOAT_EQ(NormalizeTrigger(0), 0.0f);
    EXPECT_FLOAT_EQ(NormalizeTrigger(XINPUT_GAMEPAD_TRIGGER_THRESHOLD), 0.0f);
}

TEST(NormalizeTrigger, FullPressIsOne)
{
    EXPECT_FLOAT_EQ(NormalizeTrigger(255), 1.0f);
}

TEST(NormalizeTrigger, ScalesMonotonically)
{
    const float low = NormalizeTrigger(XINPUT_GAMEPAD_TRIGGER_THRESHOLD + 10);
    const float high = NormalizeTrigger(XINPUT_GAMEPAD_TRIGGER_THRESHOLD + 50);
    EXPECT_GT(low, 0.0f);
    EXPECT_GT(high, low);
    EXPECT_LE(high, 1.0f);
}

// ---------------------------------------------------------------------------
// Clamp01
// ---------------------------------------------------------------------------

TEST(Clamp01, BelowZeroBecomesZero)
{
    EXPECT_FLOAT_EQ(Clamp01(-1.0f), 0.0f);
    EXPECT_FLOAT_EQ(Clamp01(-100.0f), 0.0f);
}

TEST(Clamp01, AboveOneBecomesOne)
{
    EXPECT_FLOAT_EQ(Clamp01(2.0f), 1.0f);
    EXPECT_FLOAT_EQ(Clamp01(100.0f), 1.0f);
}

TEST(Clamp01, MiddleValuePassesThrough)
{
    EXPECT_FLOAT_EQ(Clamp01(0.0f), 0.0f);
    EXPECT_FLOAT_EQ(Clamp01(0.5f), 0.5f);
    EXPECT_FLOAT_EQ(Clamp01(1.0f), 1.0f);
}

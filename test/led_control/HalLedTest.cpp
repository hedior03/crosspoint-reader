#include <HalLed.h>
#include <M5Ioe1.h>
#include <gtest/gtest.h>

namespace {
using namespace freeink::m5ioe1;
constexpr uint16_t LED_MASK = 0x0180;

class HalLedTest : public ::testing::Test {
 protected:
  void SetUp() override { fakeLed::reset(); }
};

TEST_F(HalLedTest, PaperMonoSupportsTheDiscreteLedWithoutADataPin) { EXPECT_TRUE(HalLed::supported()); }

TEST_F(HalLedTest, BeginClearsOnlyGreenAndBlue) {
  const auto original = fakeLed::registers;
  HalLed led;
  ASSERT_TRUE(led.begin());
  EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L], original[REG_GPIO_OUT_L] & ~LED_MASK);
  for (size_t reg = 0; reg < original.size(); ++reg) {
    if (reg != REG_GPIO_OUT_L) {
      EXPECT_EQ(fakeLed::registers[reg], original[reg]);
    }
  }
  ASSERT_EQ(fakeLed::updates.size(), 1u);
  EXPECT_EQ(fakeLed::updates[0].reg, REG_GPIO_OUT_L);
  EXPECT_EQ(fakeLed::updates[0].clearMask, LED_MASK);
  EXPECT_EQ(fakeLed::updates[0].setMask, 0);
}

TEST_F(HalLedTest, PresetsPreserveEveryOtherOutputBit) {
  HalLed led;
  ASSERT_TRUE(led.begin());
  const uint16_t unrelated = fakeLed::registers[REG_GPIO_OUT_L] & ~LED_MASK;
  const HalLed::Color presets[] = {HalLed::Color::Green, HalLed::Color::Blue, HalLed::Color::Both, HalLed::Color::Off};
  const uint16_t expected[] = {0x0080, 0x0100, 0x0180, 0};
  for (int i = 0; i < 4; ++i) {
    ASSERT_TRUE(led.setColor(presets[i]));
    EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L], unrelated | expected[i]);
  }
  for (const auto& update : fakeLed::updates) {
    EXPECT_EQ(update.reg, REG_GPIO_OUT_L);
    EXPECT_EQ(update.clearMask, LED_MASK);
    EXPECT_EQ(update.setMask & ~LED_MASK, 0);
  }
}

TEST_F(HalLedTest, UsesCurrentOutputStateInsteadOfRestoringStaleRailBits) {
  HalLed led;
  ASSERT_TRUE(led.begin());
  fakeLed::registers[REG_GPIO_OUT_L] ^= 0x2004;
  const auto current = fakeLed::registers[REG_GPIO_OUT_L];
  ASSERT_TRUE(led.setColor(HalLed::Color::Both));
  EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L], current | LED_MASK);
  ASSERT_TRUE(led.end());
  EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L], current & ~LED_MASK);
}

TEST_F(HalLedTest, NoWritesBeforeAValidatedSession) {
  HalLed led;
  EXPECT_FALSE(led.setColor(HalLed::Color::Green));
  EXPECT_TRUE(led.end());
  EXPECT_EQ(fakeLed::reads, 0);
  EXPECT_TRUE(fakeLed::updates.empty());
}

TEST_F(HalLedTest, DoesNotInitializeTheSharedBusOrProbeAnAbsentExpander) {
  for (uint8_t address : {uint8_t{0}, uint8_t{0xFF}}) {
    fakeLed::reset();
    g_addr = address;
    HalLed led;
    EXPECT_FALSE(led.begin());
    EXPECT_TRUE(led.end());
    EXPECT_EQ(fakeLed::reads, 0);
    EXPECT_TRUE(fakeLed::updates.empty());
  }
}

TEST_F(HalLedTest, AcceptsBothSupportedExpanderAddresses) {
  for (uint8_t address : {uint8_t{0x4F}, uint8_t{0x6F}}) {
    fakeLed::reset();
    g_addr = address;
    HalLed led;
    EXPECT_TRUE(led.begin());
    EXPECT_TRUE(led.end());
  }
}

TEST_F(HalLedTest, RejectsInputModeWithoutReconfiguringPins) {
  for (uint16_t pin : {PIN_LED_GREEN, PIN_LED_BLUE}) {
    fakeLed::reset();
    fakeLed::registers[REG_GPIO_MODE_L] &= ~pin;
    HalLed led;
    EXPECT_FALSE(led.begin());
    EXPECT_TRUE(fakeLed::updates.empty());
  }
}

TEST_F(HalLedTest, RejectsOpenDrainWithoutChangingDriveSettings) {
  for (uint16_t pin : {PIN_LED_GREEN, PIN_LED_BLUE}) {
    fakeLed::reset();
    fakeLed::registers[REG_GPIO_DRV_L] = pin;
    HalLed led;
    EXPECT_FALSE(led.begin());
    EXPECT_TRUE(fakeLed::updates.empty());
  }
}

TEST_F(HalLedTest, DoesNotTakeOverAnEnabledPwmChannel) {
  for (uint8_t reg : {REG_PWM1_DUTY_L, REG_PWM2_DUTY_L}) {
    fakeLed::reset();
    fakeLed::registers[reg] = 0x8000;
    HalLed led;
    EXPECT_FALSE(led.begin());
    EXPECT_TRUE(fakeLed::updates.empty());
  }
}

TEST_F(HalLedTest, FailedConfigurationReadCannotBeTreatedAsOff) {
  for (int failure = 1; failure <= 4; ++failure) {
    fakeLed::reset();
    fakeLed::failReadAt = failure;
    HalLed led;
    EXPECT_FALSE(led.begin());
    EXPECT_TRUE(led.end());
    EXPECT_TRUE(fakeLed::updates.empty());
  }
}

TEST_F(HalLedTest, InvalidPresetDoesNotChangeTheCurrentColor) {
  HalLed led;
  ASSERT_TRUE(led.begin());
  ASSERT_TRUE(led.setColor(HalLed::Color::Green));
  const auto before = fakeLed::updates.size();
  EXPECT_FALSE(led.setColor(static_cast<HalLed::Color>(255)));
  EXPECT_EQ(fakeLed::updates.size(), before);
  EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L] & LED_MASK, PIN_LED_GREEN);
}

TEST_F(HalLedTest, RepeatedBeginDoesNotResetAnActiveColor) {
  HalLed led;
  ASSERT_TRUE(led.begin());
  ASSERT_TRUE(led.setColor(HalLed::Color::Blue));
  const auto before = fakeLed::updates.size();
  EXPECT_TRUE(led.begin());
  EXPECT_EQ(fakeLed::updates.size(), before);
  EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L] & LED_MASK, PIN_LED_BLUE);
}

TEST_F(HalLedTest, InitialOffFailureCannotOpenAHealthySession) {
  fakeLed::failedUpdatesRemaining = 1;
  HalLed led;
  EXPECT_FALSE(led.begin());
  EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L] & LED_MASK, 0);
  EXPECT_FALSE(led.setColor(HalLed::Color::Green));
  EXPECT_FALSE(led.begin());
  EXPECT_TRUE(led.end());
}

TEST_F(HalLedTest, FailedUpdateAttemptsOffAndLatchesFault) {
  HalLed led;
  ASSERT_TRUE(led.begin());
  ASSERT_TRUE(led.setColor(HalLed::Color::Green));
  fakeLed::failedUpdatesRemaining = 1;
  EXPECT_FALSE(led.setColor(HalLed::Color::Blue));
  EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L] & LED_MASK, 0);
  const auto afterFailure = fakeLed::updates.size();
  EXPECT_FALSE(led.setColor(HalLed::Color::Both));
  EXPECT_FALSE(led.begin());
  EXPECT_EQ(fakeLed::updates.size(), afterFailure);
  EXPECT_TRUE(led.end());
  EXPECT_TRUE(led.begin());
}

TEST_F(HalLedTest, FailedReadbackDoesNotReportSuccess) {
  HalLed led;
  ASSERT_TRUE(led.begin());
  fakeLed::failReadAt = fakeLed::reads + 1;
  EXPECT_FALSE(led.setColor(HalLed::Color::Both));
  EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L] & LED_MASK, 0);
  EXPECT_FALSE(led.setColor(HalLed::Color::Green));
}

TEST_F(HalLedTest, MismatchingReadbackDoesNotReportSuccess) {
  HalLed led;
  ASSERT_TRUE(led.begin());
  fakeLed::corruptNextOutputRead = true;
  EXPECT_FALSE(led.setColor(HalLed::Color::Green));
  EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L] & LED_MASK, 0);
  EXPECT_FALSE(led.setColor(HalLed::Color::Blue));
}

TEST_F(HalLedTest, CleanupFailureIsReportedAndCanBeRetriedWithoutTurningOn) {
  HalLed led;
  ASSERT_TRUE(led.begin());
  ASSERT_TRUE(led.setColor(HalLed::Color::Both));
  fakeLed::failedUpdatesRemaining = 3;
  const auto before = fakeLed::updates.size();
  EXPECT_FALSE(led.setColor(HalLed::Color::Green));
  EXPECT_EQ(fakeLed::updates.size(), before + 2);
  EXPECT_FALSE(led.end());
  EXPECT_EQ(fakeLed::updates.size(), before + 3);
  EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L] & LED_MASK, LED_MASK);
  EXPECT_FALSE(led.setColor(HalLed::Color::Blue));
  EXPECT_TRUE(led.end());
  EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L] & LED_MASK, 0);
}

TEST_F(HalLedTest, EndClearsChannelsAndPreventsFurtherWritesUntilBegin) {
  HalLed led;
  ASSERT_TRUE(led.begin());
  ASSERT_TRUE(led.setColor(HalLed::Color::Both));
  ASSERT_TRUE(led.end());
  EXPECT_EQ(fakeLed::registers[REG_GPIO_OUT_L] & LED_MASK, 0);
  const auto before = fakeLed::updates.size();
  EXPECT_TRUE(led.end());
  EXPECT_FALSE(led.setColor(HalLed::Color::Green));
  EXPECT_EQ(fakeLed::updates.size(), before);
  EXPECT_TRUE(led.begin());
}
}  // namespace

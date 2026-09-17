#include <HalLed.h>
#include <M5Ioe1.h>
#include <gtest/gtest.h>

TEST(HalLedUnsupported, DoesNotExposeOrTouchHardware) {
  fakeLed::reset();
  HalLed led;
  EXPECT_FALSE(HalLed::supported());
  EXPECT_FALSE(led.begin());
  EXPECT_FALSE(led.setColor(HalLed::Color::Both));
  EXPECT_TRUE(led.end());
  EXPECT_EQ(fakeLed::reads, 0);
  EXPECT_TRUE(fakeLed::updates.empty());
}

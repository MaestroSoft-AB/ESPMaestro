#include "unity.h"

#define static

#include "../facility_config.c"

#undef static

TEST_CASE("complete config is accepted", "[facility_config]") {
  Facility_Config cfg = {
      .facility_name = "Home",
      .lat = "59.3293",
      .lon = "18.0686",
      .energy_zone = 3,
  };

  TEST_ASSERT_TRUE(facility_config_complete(&cfg));
}

TEST_CASE("config without name is rejected", "[facility_config]") {
  Facility_Config cfg = {
      .facility_name = "",
      .lat = "59.3293",
      .lon = "18.0686",
      .energy_zone = 3,
  };

  TEST_ASSERT_FALSE(facility_config_complete(&cfg));
}

TEST_CASE("config with invalid energy zone is rejected", "[facility_config]") {
  Facility_Config cfg = {
      .facility_name = "Home",
      .lat = "59.3293",
      .lon = "18.0686",
      .energy_zone = 5,
  };

  TEST_ASSERT_FALSE(facility_config_complete(&cfg));

  cfg.energy_zone = 0;
  TEST_ASSERT_FALSE(facility_config_complete(&cfg));
}

TEST_CASE("url encode keeps plain characters", "[facility_config]") {
  char out[64];

  facility_config_url_encode("Home-1_A.B~", out, sizeof(out));

  TEST_ASSERT_EQUAL_STRING("Home-1_A.B~", out);
}

TEST_CASE("url encode escapes spaces and special characters",
          "[facility_config]") {
  char out[64];

  facility_config_url_encode("My Home!", out, sizeof(out));

  TEST_ASSERT_EQUAL_STRING("My%20Home%21", out);
}

TEST_CASE("url encode handles null input", "[facility_config]") {
  char out[64];

  facility_config_url_encode(NULL, out, sizeof(out));

  TEST_ASSERT_EQUAL_STRING("", out);
}

TEST_CASE("url encode truncates safely", "[facility_config]") {
  char out[8];

  facility_config_url_encode("Hello World", out, sizeof(out));

  TEST_ASSERT_TRUE(strlen(out) < sizeof(out));
  TEST_ASSERT_EQUAL_CHAR('\0', out[strlen(out)]);
}

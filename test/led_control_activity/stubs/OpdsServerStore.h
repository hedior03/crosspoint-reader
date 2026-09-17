#pragma once

struct TestOpdsStore {
  bool enabled = false;
  bool hasServers() const { return enabled; }
};
inline TestOpdsStore testOpdsStore;
#define OPDS_STORE testOpdsStore

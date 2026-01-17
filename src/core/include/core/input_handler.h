/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <unordered_map>

namespace core
{
class InputHandler
{
  public:
    void setKeyPressed(int key);
    void setKeyReleased(int key);

    bool isKeyPressed(int key) const;

  private:
    mutable std::unordered_map<int, bool> m_keyState;
};
} // namespace core

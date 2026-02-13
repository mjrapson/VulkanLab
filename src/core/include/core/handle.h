/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include <climits>
#include <functional>
#include <stddef.h>
#include <stdint.h>
#include <unordered_map>

namespace core
{
template <typename HandleType>
struct Handle
{
    uint32_t uid{UINT_MAX};

    Handle(uint32_t handleUid)
        : uid{handleUid}
    {
    }
    Handle(const Handle&) = default;
    Handle& operator=(const Handle&) = default;
    Handle(Handle&& other) noexcept = default;
    Handle& operator=(Handle&& other) noexcept = default;

    friend bool operator==(Handle lhs, Handle rhs) noexcept
    {
        return lhs.uid == rhs.uid;
    }

    friend bool operator<(Handle lhs, Handle rhs) noexcept
    {
        return lhs.uid < rhs.uid;
    }
};

template <typename HandleType>
class HandleGenerator
{
  public:
    static Handle<HandleType> generate()
    {
        return Handle<HandleType>(nextUid());
    }

  private:
    static uint32_t nextUid()
    {
        static uint32_t val = 0;
        return val++;
    }
};

template <typename HandleType>
struct Hash
{
    size_t operator()(const HandleType& handle) const noexcept
    {
        return std::hash<uint32_t>{}(handle.uid);
    }
};

template <typename HandleType, typename DataType>
using HandleContainer = std::unordered_map<HandleType, DataType, Hash<HandleType>>;
} // namespace core

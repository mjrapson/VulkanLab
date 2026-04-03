/// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Mark Rapson

#pragma once

#include "core/handle.h"

#include <vector>

namespace core
{
template <typename Object>
class MemoryStore
{
    struct Element
    {
        uint32_t generation;
        Object data;
    };

    using MemoryBlock = std::vector<Element>;

  public:
    explicit MemoryStore(size_t blockSize)
        : blockSize_(blockSize)
    {
    }

    template <typename... Args>
    Handle<Object> allocate(Args&&... args)
    {
        auto index = uint32_t{0};

        if (!freeIds_.empty())
        {
            index = freeIds_.back();
            freeIds_.pop_back();
        }
        else
        {
            index = head_++;
        }

        // Unpack flat index into 2D indices
        const auto blockIndex = index / blockSize_;
        const auto elementIndex = index % blockSize_;

        // Do we need a new block?
        if (blockIndex >= blocks_.size())
        {
            blocks_.emplace_back();
            blocks_.back().reserve(blockSize_); // preallocate vector to prevent resizing later
        }

        auto& block = blocks_[blockIndex];

        if (elementIndex >= block.size()) // This memory element has never been used before
        {
            block.emplace_back(Element{0, Object(std::forward<Args>(args)...)});
        }
        else
        {
            block[elementIndex].data = Object(std::forward<Args>(args)...);
        }

        return Handle<Object>{index, block[elementIndex].generation};
    }

    void release(Handle<Object> handle)
    {
        const auto index = handle.index;
        const auto blockIndex = index / blockSize_;
        const auto elementIndex = index % blockSize_;

        if (blockIndex >= blocks_.size())
        {
            return;
        }

        auto& block = blocks_[blockIndex];

        if (elementIndex >= block.size())
        {
            return;
        }

        auto& element = blocks_[blockIndex][elementIndex];
        element.generation++;

        freeIds_.push_back(handle.index);
    }

    void clear()
    {
        for (auto blockIndex = size_t{0}; blockIndex < blocks_.size(); ++blockIndex)
        {
            for (auto elementIndex = size_t{0}; elementIndex < blocks_[blockIndex].size(); ++elementIndex)
            {
                auto& element = blocks_[blockIndex][elementIndex];
                element.generation++;

                std::destroy_at(&element.data);

                freeIds_.push_back(static_cast<uint32_t>(blockIndex * blockSize_ + elementIndex));
            }
        }
    }

    Object* get(Handle<Object> handle)
    {
        const auto index = handle.index;
        const auto blockIndex = index / blockSize_;
        const auto elementIndex = index % blockSize_;

        if (blockIndex >= blocks_.size())
        {
            return nullptr;
        }

        auto& block = blocks_[blockIndex];

        if (elementIndex >= block.size())
        {
            return nullptr;
        }

        auto& element = block[elementIndex];

        if (element.generation != handle.generation)
        {
            return nullptr;
        }

        return &element.data;
    }

  private:
    std::vector<MemoryBlock> blocks_;
    std::vector<uint32_t> freeIds_;
    size_t blockSize_;
    uint32_t head_{0};
};
} // namespace core
